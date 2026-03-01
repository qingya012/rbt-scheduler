#!/usr/bin/env node
/**
 * server.js — RBT Scheduler API with SQLite persistence
 *
 * Routes
 * ──────
 *  GET    /                    → index.html
 *  GET    /api/events          → all events in the C++ RBT (sourced from DB)
 *  POST   /api/events          → validate + insert/update one event
 *  DELETE /api/events/:id      → remove one event
 *  DELETE /api/events          → clear all events
 *  POST   /api/audit           → conflict check for a range
 *                                → { conflict, conflictIds[], events[] }
 *  GET    /api/stats           → { total, conflicts, busyMinutes }
 *
 * Persistence
 * ───────────
 *  SQLite (better-sqlite3) is the durable store.
 *  On startup every row is loaded into the C++ RBT.
 *  Every mutation (insert, update, delete, clear) is written to SQLite
 *  BEFORE touching the RBT so the DB is always the source of recovery.
 *
 * Conflict info
 * ─────────────
 *  /api/audit returns { conflict: bool, conflictIds: number[], events: Event[] }
 *  so the frontend can highlight every affected block by ID.
 *
 * Validation (server-side)
 * ────────────────────────
 *  • startTime and endTime must be finite numbers
 *  • startTime < endTime
 *  • Both values must be in [0, 10079]  (0..MINS_PER_WEEK-1)
 *  • title must be a non-empty string (trimmed)
 *  • id must be a positive integer when provided
 */

'use strict';

const http    = require('http');
const fs      = require('fs');
const path    = require('path');
const Database = require('better-sqlite3');

// ── Load RBT addon ─────────────────────────────────────────────────
const rbt = require('./bridge');

if (!rbt.isAvailable) {
  console.error('[server.js] FATAL: C++ addon not available.');
  console.error('  Run `npm run build` to compile the addon first.');
  process.exit(1);
}

// ── Config ─────────────────────────────────────────────────────────
const portArg    = process.argv.indexOf('--port');
const PORT       = portArg !== -1
  ? parseInt(process.argv[portArg + 1], 10)
  : (parseInt(process.env.PORT, 10) || 3000);
const INDEX_FILE = path.resolve(__dirname, '../docs/index.html');
const DB_FILE    = path.resolve(__dirname, 'scheduler.db');

// ── Validation constants ────────────────────────────────────────────
const MINS_PER_WEEK = 7 * 24 * 60; // 10080 — valid range [0, 10079]

// ── SQLite setup ────────────────────────────────────────────────────
const db = new Database(DB_FILE);

// Enable WAL for better concurrency (write-ahead log)
db.pragma('journal_mode = WAL');

db.exec(`
  CREATE TABLE IF NOT EXISTS events (
    id             INTEGER PRIMARY KEY,
    title          TEXT    NOT NULL,
    startTime      INTEGER NOT NULL,
    endTime        INTEGER NOT NULL,
    is_conflicting INTEGER NOT NULL DEFAULT 0
  );
`);

// Non-destructive migration: add the column if upgrading from an older schema
try {
  db.exec('ALTER TABLE events ADD COLUMN is_conflicting INTEGER NOT NULL DEFAULT 0');
  console.log('[db] Migrated: added is_conflicting column.');
} catch (_) { /* column already exists — normal on fresh starts */ }

// Prepared statements — created once, reused for every request
const stmtUpsert  = db.prepare(
  'INSERT OR REPLACE INTO events (id, title, startTime, endTime, is_conflicting) VALUES (?, ?, ?, ?, 0)'
);
const stmtDelete        = db.prepare('DELETE FROM events WHERE id = ?');
const stmtClear         = db.prepare('DELETE FROM events');
const stmtGetAll        = db.prepare('SELECT id, title, startTime, endTime, is_conflicting FROM events ORDER BY startTime');
const stmtGetById       = db.prepare('SELECT id, title, startTime, endTime, is_conflicting FROM events WHERE id = ?');
const stmtSetConflict   = db.prepare('UPDATE events SET is_conflicting = ? WHERE id = ?');
const stmtClearConflicts= db.prepare('UPDATE events SET is_conflicting = 0');

// ── Startup: seed RBT from SQLite + immediate conflict audit ───────
const storedEvents = stmtGetAll.all();
for (const ev of storedEvents) {
  // Force-insert so overlapping events saved in previous sessions are all
  // present in the tree — needed for accurate full-tree conflict detection.
  rbt.forceInsertEvent({ id: ev.id, title: ev.title, startTime: ev.startTime, endTime: ev.endTime });
}
console.log(`[db] Loaded ${storedEvents.length} event(s) from ${path.basename(DB_FILE)} into C++ RBT.`);

// Re-audit and persist conflict flags so the first /api/stats call after
// page load returns the correct is_conflicting values immediately.
if (storedEvents.length > 0) {
  const { conflictIdSet } = syncAllConflicts();
  if (conflictIdSet.size > 0) {
    console.log(`[db] Startup: ${conflictIdSet.size} pre-existing conflict(s) flagged in DB.`);
  } else {
    console.log('[db] Startup: no conflicts detected.');
  }
}

// ── HTTP helpers ────────────────────────────────────────────────────
function cors(res) {
  res.setHeader('Access-Control-Allow-Origin',  '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, DELETE, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
}

function json(res, statusCode, body) {
  res.writeHead(statusCode, { 'Content-Type': 'application/json; charset=utf-8' });
  res.end(JSON.stringify(body));
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    let data = '';
    req.on('data', chunk => { data += chunk; });
    req.on('end',  ()    => {
      try { resolve(JSON.parse(data || '{}')); }
      catch (e) { reject(e); }
    });
    req.on('error', reject);
  });
}

// ── Validation ──────────────────────────────────────────────────────
/**
 * Validate the fields of an incoming event body.
 * Returns null on success, or an error string to send back to the client.
 */
function validateEvent({ id, title, startTime, endTime }) {
  // title
  if (typeof title !== 'string' || title.trim().length === 0)
    return 'title must be a non-empty string';

  // id (optional — 0 means auto-assign, positive means pinned)
  if (id !== undefined && id !== 0) {
    if (!Number.isInteger(id) || id < 1)
      return 'id must be a positive integer (or 0 to auto-assign)';
  }

  // startTime
  if (typeof startTime !== 'number' || !Number.isFinite(startTime))
    return 'startTime must be a finite number';
  if (!Number.isInteger(startTime))
    return 'startTime must be an integer (week-minutes)';
  if (startTime < 0 || startTime >= MINS_PER_WEEK)
    return `startTime must be in [0, ${MINS_PER_WEEK - 1}]`;

  // endTime
  if (typeof endTime !== 'number' || !Number.isFinite(endTime))
    return 'endTime must be a finite number';
  if (!Number.isInteger(endTime))
    return 'endTime must be an integer (week-minutes)';
  if (endTime < 1 || endTime > MINS_PER_WEEK)
    return `endTime must be in [1, ${MINS_PER_WEEK}]`;

  // ordering
  if (startTime >= endTime)
    return `startTime (${startTime}) must be strictly less than endTime (${endTime})`;

  return null; // valid
}

/**
 * Validate only the time-range fields used by /api/audit.
 */
function validateRange({ startTime, endTime }) {
  if (typeof startTime !== 'number' || !Number.isFinite(startTime))
    return 'startTime must be a finite number';
  if (typeof endTime !== 'number' || !Number.isFinite(endTime))
    return 'endTime must be a finite number';
  if (startTime >= endTime)
    return `startTime (${startTime}) must be strictly less than endTime (${endTime})`;
  if (startTime < 0 || startTime >= MINS_PER_WEEK)
    return `startTime must be in [0, ${MINS_PER_WEEK - 1}]`;
  if (endTime < 1 || endTime > MINS_PER_WEEK)
    return `endTime must be in [1, ${MINS_PER_WEEK}]`;
  return null;
}

// ── Stats helper ────────────────────────────────────────────────────
// Reads is_conflicting directly from SQLite (set by syncAllConflicts).
// This is O(n) over DB rows with no C++ round-trips; fast and consistent.
// Returns { total, conflicts, conflictIds, allConflictIds, busyMinutes, events }
function computeStats() {
  const all         = stmtGetAll.all();   // rows with is_conflicting column
  let   busyMinutes = 0;
  const conflictIds = [];

  for (const ev of all) {
    busyMinutes += Math.max(0, ev.endTime - ev.startTime);
    if (ev.is_conflicting) conflictIds.push(ev.id);
  }

  return {
    total:          all.length,
    conflicts:      conflictIds.length,
    conflictIds,
    allConflictIds: conflictIds,   // alias for upsert-audit consumers
    busyMinutes,
    events:         all,           // full rows so callers can send to frontend
  };
}

// ── syncAllConflicts — THE single source of truth for conflict state ─
// Asks the C++ RBT whether each event overlaps any other, then writes
// is_conflicting = 0|1 into SQLite for every row.  Returns the set of
// currently conflicting IDs AND the wall-clock duration in microseconds.
//
// Must be called after EVERY add, update, or delete so the DB and the
// frontend always see a consistent, fully-audited conflict picture.
function syncAllConflicts() {
  const t0  = process.hrtime.bigint();   // nanosecond-resolution timer start

  const all = rbt.getAll();
  const conflictIdSet = new Set();

  // First pass: find every event that has at least one overlap partner
  for (const ev of all) {
    const res    = rbt.checkConflict(ev.startTime, ev.endTime);
    const others = res.events.filter(e => e.id !== ev.id);
    if (others.length > 0) conflictIdSet.add(ev.id);
  }

  // Second pass: write the authoritative status into SQLite in one transaction
  const writeConflicts = db.transaction(() => {
    stmtClearConflicts.run();                           // reset all to 0
    for (const id of conflictIdSet) {
      stmtSetConflict.run(1, id);                       // set conflicting ones to 1
    }
  });
  writeConflicts();

  // Convert BigInt nanoseconds → plain Number microseconds (1 ns = 0.001 µs)
  const latency_us = Number(process.hrtime.bigint() - t0) / 1000;

  return { conflictIdSet, latency_us };
}

// ── Undo stack ───────────────────────────────────────────────────────
// Stores up to 10 snapshots of the full events table taken BEFORE each
// mutating operation.  Each snapshot is a plain array of row objects.
const ACTION_STACK_MAX = 10;
const actionStack = [];

// Capture the current DB state before a mutation so it can be restored.
function pushUndo() {
  const snapshot = stmtGetAll.all();   // [{id, title, startTime, endTime, is_conflicting}, …]
  actionStack.push(snapshot);
  if (actionStack.length > ACTION_STACK_MAX) actionStack.shift();
}

// ── Upsert helper (DB + RBT) ────────────────────────────────────────
// Always force-inserts so the event is in the tree for conflict detection
// even when it overlaps. The C++ tree stores it; audit queries surface it.
function upsertEventFull(ev) {
  stmtUpsert.run(ev.id, ev.title.trim(), ev.startTime, ev.endTime);
  rbt.deleteEvent(ev.id);
  return rbt.forceInsertEvent(ev);
}

// ── Delete helper ───────────────────────────────────────────────────
function deleteEventFull(id) {
  stmtDelete.run(id);
  return rbt.deleteEvent(id);
}

// ── Request router ──────────────────────────────────────────────────
async function handler(req, res) {
  cors(res);

  const method = req.method.toUpperCase();
  const url    = req.url.split('?')[0];

  // Preflight
  if (method === 'OPTIONS') { res.writeHead(204); res.end(); return; }

  // ── Serve index.html ───────────────────────────────────────────
  if (method === 'GET' && (url === '/' || url === '/index.html')) {
    try {
      const html = fs.readFileSync(INDEX_FILE, 'utf8');
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
      res.end(html);
    } catch (_) {
      res.writeHead(404); res.end('index.html not found');
    }
    return;
  }

  // ── GET /api/events ────────────────────────────────────────────
  // Always serve from the C++ RBT (which mirrors SQLite after startup).
  if (method === 'GET' && url === '/api/events') {
    json(res, 200, rbt.getAll());
    return;
  }

  // ── POST /api/events — insert or update one event ─────────────
  if (method === 'POST' && url === '/api/events') {
    let body;
    try { body = await readBody(req); }
    catch (_) { json(res, 400, { error: 'Invalid JSON body' }); return; }

    // Normalise
    const id        = body.id ? Math.trunc(Number(body.id)) : 0;
    const title     = typeof body.title === 'string' ? body.title.trim() : '';
    const startTime = typeof body.startTime === 'number' ? Math.trunc(body.startTime) : body.startTime;
    const endTime   = typeof body.endTime   === 'number' ? Math.trunc(body.endTime)   : body.endTime;

    const validErr = validateEvent({ id, title, startTime, endTime });
    if (validErr) { json(res, 422, { error: validErr }); return; }

    const finalId = id || Date.now();
    const ev      = { id: finalId, title, startTime, endTime };

    pushUndo();   // snapshot before mutation
    stmtUpsert.run(finalId, title, startTime, endTime);
    rbt.deleteEvent(finalId);
    rbt.forceInsertEvent(ev);

    // Audit the whole tree and persist conflict flags to DB
    const { latency_us } = syncAllConflicts();
    const s              = computeStats();
    json(res, 200, {
      ok:             true,
      id:             finalId,
      allConflictIds: s.allConflictIds,
      stats:          { total: s.total, conflicts: s.conflicts, busyMinutes: s.busyMinutes },
      events:         s.events,
      latency_us,
    });
    return;
  }

  // ── DELETE /api/events — clear all ────────────────────────────
  if (method === 'DELETE' && url === '/api/events') {
    stmtClear.run();
    rbt.clearTree();
    json(res, 200, {
      ok:             true,
      allConflictIds: [],
      events:         [],
      stats: { total: 0, conflicts: 0, busyMinutes: 0 },
    });
    return;
  }

  // ── DELETE /api/events/:id — remove one ───────────────────────
  const deleteMatch = url.match(/^\/api\/events\/(\d+)$/);
  if (method === 'DELETE' && deleteMatch) {
    const id     = parseInt(deleteMatch[1], 10);
    if (!Number.isFinite(id) || id < 1) {
      json(res, 422, { error: 'id must be a positive integer' });
      return;
    }
    pushUndo();   // snapshot before mutation
    deleteEventFull(id);
    // Audit the remaining tree and persist updated conflict flags
    const { latency_us: latency_us_del } = syncAllConflicts();
    const s = computeStats();
    json(res, 200, {
      ok:             true,
      allConflictIds: s.allConflictIds,
      events:         s.events,
      stats: { total: s.total, conflicts: s.conflicts, busyMinutes: s.busyMinutes },
      latency_us:     latency_us_del,
    });
    return;
  }

  // ── POST /api/events/upsert-audit — atomic upsert + full conflict scan ──
  // Upserts the event into both SQLite and the C++ RBT, then immediately
  // runs a full-tree conflict scan and returns everything the frontend needs
  // in a single round-trip.  This eliminates the race between POST /api/events
  // and a subsequent POST /api/audit where the tree could be momentarily empty.
  //
  // Body:    { id, title, startTime, endTime }
  // Response: {
  //   ok:              boolean,
  //   insertConflict:  boolean,          // true if this specific event collides
  //   conflictIds:     number[],         // IDs of events overlapping this event's range
  //   allConflictIds:  number[],         // ALL IDs in conflict across the entire tree
  //   stats:           { total, conflicts, busyMinutes }
  // }
  if (method === 'POST' && url === '/api/events/upsert-audit') {
    let body;
    try { body = await readBody(req); }
    catch (_) { json(res, 400, { error: 'Invalid JSON body' }); return; }

    const id        = body.id ? Math.trunc(Number(body.id)) : 0;
    const title     = typeof body.title === 'string' ? body.title.trim() : '';
    const startTime = typeof body.startTime === 'number' ? Math.trunc(body.startTime) : body.startTime;
    const endTime   = typeof body.endTime   === 'number' ? Math.trunc(body.endTime)   : body.endTime;

    const validErr = validateEvent({ id, title, startTime, endTime });
    if (validErr) { json(res, 422, { error: validErr }); return; }

    const finalId = id || Date.now();
    const ev      = { id: finalId, title, startTime, endTime };

    pushUndo();   // snapshot before mutation
    // 1. Persist to SQLite (always durable, even for conflicting events)
    stmtUpsert.run(ev.id, ev.title, ev.startTime, ev.endTime);

    // 2. Remove old position from RBT (handles move/resize), then insert new.
    //    Use forceInsertEvent so conflicting events ARE in the tree — this lets
    //    the full-tree conflict scan below find all overlapping pairs correctly.
    rbt.deleteEvent(ev.id);
    const insertResult = rbt.forceInsertEvent(ev);

    // 3. Which events overlap THIS event's new range (excluding self)?
    //    Used to decide whether to show the ghost suggestion block.
    const auditRaw    = rbt.checkConflict(startTime, endTime);
    const conflictIds = auditRaw.events
      .filter(e => e.id !== finalId)
      .map(e => e.id);

    // 4. Full-tree audit — persists is_conflicting to DB for EVERY event,
    //    then computeStats reads that column for a consistent snapshot.
    const { latency_us: latency_us_ua } = syncAllConflicts();
    const s = computeStats();

    json(res, 200, {
      ok:             insertResult.ok,
      insertConflict: conflictIds.length > 0,
      conflictIds,                  // events overlapping this specific slot
      allConflictIds: s.allConflictIds,
      events:         s.events,     // full row list with is_conflicting flags
      stats: {
        total:       s.total,
        conflicts:   s.conflicts,
        busyMinutes: s.busyMinutes,
      },
      latency_us:     latency_us_ua,
    });
    return;
  }

  // ── POST /api/audit — authoritative conflict check ─────────────
  // Body: { startTime: number, endTime: number, excludeId?: number }
  // Response: { conflict: bool, conflictIds: number[], events: Event[] }
  //
  // conflictIds is the key addition: every event ID that overlaps the
  // queried range, minus excludeId, so the frontend can colour them all.
  if (method === 'POST' && url === '/api/audit') {
    let body;
    try { body = await readBody(req); }
    catch (_) { json(res, 400, { error: 'Invalid JSON body' }); return; }

    const startTime = typeof body.startTime === 'number' ? Math.trunc(body.startTime) : body.startTime;
    const endTime   = typeof body.endTime   === 'number' ? Math.trunc(body.endTime)   : body.endTime;
    const excludeId = body.excludeId != null ? Number(body.excludeId) : null;

    const validErr = validateRange({ startTime, endTime });
    if (validErr) { json(res, 422, { error: validErr }); return; }

    const raw     = rbt.checkConflict(startTime, endTime);
    const events  = excludeId != null
      ? raw.events.filter(e => e.id !== excludeId)
      : raw.events;

    const conflictIds = events.map(e => e.id);
    json(res, 200, {
      conflict:    conflictIds.length > 0,
      conflictIds,
      events,
    });
    return;
  }

  // ── POST /api/suggest — find the earliest free slot ────────────
  // Body: { duration: number, searchStart?: number }
  //   duration    – required slot length in minutes (week-minute scale)
  //   searchStart – week-minute offset to start searching from (default 0)
  //                 Pass the event's current startTime to find the nearest
  //                 free slot after its current position.
  //
  // Response: { found: boolean, slot: { startTime, endTime } | null }
  //
  // Uses the RBT's ordered in-order traversal to scan gaps in O(log n + k).
  if (method === 'POST' && url === '/api/suggest') {
    let body;
    try { body = await readBody(req); }
    catch (_) { json(res, 400, { error: 'Invalid JSON body' }); return; }

    const duration    = typeof body.duration    === 'number' ? Math.trunc(body.duration)    : null;
    const searchStart = typeof body.searchStart === 'number' ? Math.trunc(body.searchStart) : 0;

    if (!duration || duration <= 0) {
      json(res, 422, { error: 'duration must be a positive integer (week-minutes)' });
      return;
    }
    if (duration > MINS_PER_WEEK) {
      json(res, 422, { error: `duration must be ≤ ${MINS_PER_WEEK}` });
      return;
    }

    const result = rbt.findFirstFreeSlot(duration, Math.max(0, searchStart));

    if (result.found) {
      json(res, 200, {
        found: true,
        slot:  { startTime: result.startTime, endTime: result.endTime },
      });
    } else {
      // Nothing found starting at searchStart — try again from week start
      if (searchStart > 0) {
        const retry = rbt.findFirstFreeSlot(duration, 0);
        if (retry.found) {
          json(res, 200, {
            found: true,
            slot:  { startTime: retry.startTime, endTime: retry.endTime },
            wrappedAround: true,
          });
          return;
        }
      }
      json(res, 200, { found: false, slot: null });
    }
    return;
  }

  // ── GET /api/stats ─────────────────────────────────────────────
  if (method === 'GET' && url === '/api/stats') {
    const s = computeStats();
    json(res, 200, s);   // includes events[] with is_conflicting flags
    return;
  }

  // ── POST /api/undo — revert the last mutation ──────────────────
  // Pops the most recent snapshot from actionStack, wipes SQLite, re-inserts
  // the snapshot rows, rebuilds the C++ RBT from scratch, re-audits conflicts,
  // and returns the restored full state so the frontend can re-render.
  if (method === 'POST' && url === '/api/undo') {
    if (actionStack.length === 0) {
      json(res, 200, { ok: false, message: 'Nothing to undo' });
      return;
    }

    const snapshot = actionStack.pop();

    // Atomically wipe DB and restore snapshot in a single transaction
    const restore = db.transaction(() => {
      stmtClear.run();
      for (const ev of snapshot) {
        stmtUpsert.run(ev.id, ev.title, ev.startTime, ev.endTime);
      }
    });
    restore();

    // Rebuild the C++ RBT from scratch to match the restored DB
    rbt.clearTree();
    for (const ev of snapshot) {
      rbt.forceInsertEvent({ id: ev.id, title: ev.title, startTime: ev.startTime, endTime: ev.endTime });
    }

    // Re-audit and return the full restored state
    const { latency_us } = syncAllConflicts();
    const s = computeStats();

    json(res, 200, {
      ok:             true,
      restoredCount:  snapshot.length,
      stackDepth:     actionStack.length,
      allConflictIds: s.allConflictIds,
      events:         s.events,
      stats:          { total: s.total, conflicts: s.conflicts, busyMinutes: s.busyMinutes },
      latency_us,
    });
    return;
  }

  // ── GET /health ────────────────────────────────────────────────
  if (method === 'GET' && url === '/health') {
    json(res, 200, { ok: true });
    return;
  }

  // ── 404 ───────────────────────────────────────────────────────
  json(res, 404, { error: `Not found: ${method} ${url}` });
}

// ── Server boot ─────────────────────────────────────────────────────
const server = http.createServer((req, res) => {
  handler(req, res).catch(err => {
    console.error('[server.js] Unhandled error:', err);
    try { json(res, 500, { error: 'Internal server error' }); } catch (_) {}
  });
});

server.on('error', err => {
  if (err.code === 'EADDRINUSE') {
    console.error(`\n[server.js] Port ${PORT} is already in use.`);
    console.error(`  Kill existing process: kill -9 $(lsof -t -i:${PORT})`);
    console.error(`  Or use another port:   node server.js --port 3001\n`);
  } else {
    console.error('[server.js] Server error:', err.message);
  }
  process.exit(1);
});

// Close the DB cleanly on shutdown so WAL is checkpointed
process.on('SIGINT',  () => { db.close(); process.exit(0); });
process.on('SIGTERM', () => { db.close(); process.exit(0); });

server.listen(PORT, () => {
  // Print file modification time so it's obvious when the server is stale
  const fileMtime = fs.statSync(__filename).mtime.toLocaleString();
  console.log(`\n  RBT Scheduler API  →  http://localhost:${PORT}`);
  console.log(`  C++ addon          →  available`);
  console.log(`  SQLite database    →  ${DB_FILE}`);
  console.log(`  server.js loaded   →  ${__filename}`);
  console.log(`  server.js mtime    →  ${fileMtime}  ← if this is old, restart!\n`);
  console.log('  Routes:');
  console.log('    GET    /                 → index.html');
  console.log('    GET    /api/events       → all events (C++ RBT)');
  console.log('    POST   /api/events       → insert / update event (validated)');
  console.log('    DELETE /api/events/:id   → remove event');
  console.log('    DELETE /api/events       → clear all');
  console.log('    POST   /api/audit        → conflict check → { conflict, conflictIds[], events[] }');
  console.log('    POST   /api/suggest      → find free slot → { found, slot: {startTime, endTime} }');
  console.log('    GET    /api/stats        → { total, conflicts, conflictIds[], busyMinutes }');
  console.log('    GET    /health           → { ok: true }\n');
});

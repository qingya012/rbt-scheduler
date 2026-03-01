#!/usr/bin/env node
/**
 * watch.js — RBT Scheduler development server & automation pipeline
 *
 * Routes
 *   GET  /              → index.html
 *   GET  /data.json     → data.json (with CORS headers)
 *   POST /write-event   → merge one event into data.json, trigger C++ audit
 *   GET  /events        → Server-Sent Events stream (pushes updates to browser)
 *
 * Pipeline
 *   1. POST /write-event writes data.json  (status:"pending")
 *   2. fs.watch fires  →  300 ms debounce  →  ./build/scheduler --audit data.json
 *   3. C++ overwrites data.json  (status:"success"|"conflict")
 *   4. fs.watch fires again  →  SSE broadcast to all connected browsers
 */

'use strict';

const http      = require('http');
const fs        = require('fs');
const path      = require('path');
const { execFile } = require('child_process');

// ── Config ────────────────────────────────────────────────────────
const PORT        = 3000;
const DATA_FILE   = path.resolve(__dirname, '../data.json');
const INDEX_FILE  = path.resolve(__dirname, '../docs/index.html');
// Resolve the C++ binary relative to this script's directory
const CPP_BINARY  = path.resolve(__dirname, '../build', 'scheduler');

// ── SSE client registry ───────────────────────────────────────────
const sseClients = new Set();

function broadcast(payload) {
  const line = `data: ${JSON.stringify(payload)}\n\n`;
  for (const res of sseClients) {
    try { res.write(line); } catch (_) { sseClients.delete(res); }
  }
}

// ── Atomic JSON write (temp file + rename) ────────────────────────
function writeJsonAtomic(obj) {
  const tmp = DATA_FILE + '.tmp';
  fs.writeFileSync(tmp, JSON.stringify(obj, null, 2) + '\n', 'utf8');
  fs.renameSync(tmp, DATA_FILE);
}

function readJson() {
  try {
    return JSON.parse(fs.readFileSync(DATA_FILE, 'utf8'));
  } catch (_) {
    return [];
  }
}

// ── C++ audit trigger ─────────────────────────────────────────────
let auditing   = false;   // re-entrancy guard
let auditTimer = null;    // debounce handle

// Flag used to distinguish "we wrote the file" from "C++ wrote the file"
// so we only broadcast after C++ finishes, not after our own write.
let suppressNextWatch = false;

function scheduleAudit() {
  if (auditTimer) clearTimeout(auditTimer);
  auditTimer = setTimeout(runAudit, 300);
}

function runAudit() {
  if (auditing) { scheduleAudit(); return; }
  auditing = true;

  execFile(CPP_BINARY, ['--audit', DATA_FILE], { cwd: __dirname }, (err, stdout, stderr) => {
    auditing = false;
    if (err) {
      console.error('[audit] C++ exited with error:', err.message);
      if (stderr) console.error(stderr.trim());
    } else {
      if (stdout) console.log('[audit]', stdout.trim());
      // Broadcast the updated JSON to all SSE clients
      const updated = readJson();
      broadcast(updated);
    }
  });
}

// ── File watcher (watches for external changes to data.json) ──────
// We use a two-phase approach:
//   • After our own atomic write we set suppressNextWatch=true
//     so we don't double-trigger the debounce.
//   • The watcher fires after C++ writes → broadcast path above.

let watcherActive = false;

function startWatcher() {
  if (watcherActive) return;
  watcherActive = true;

  // Use fs.watchFile (stat-polling) as a more portable alternative to fs.watch
  // on macOS where fs.watch can fire 'rename' events on atomic writes.
  // Track the last mtime we processed so repeated poll ticks don't re-trigger
  let lastProcessedMtime = 0;

  fs.watchFile(DATA_FILE, { interval: 300 }, (curr, prev) => {
    if (curr.mtimeMs === prev.mtimeMs) return;      // nothing changed
    if (curr.mtimeMs === lastProcessedMtime) return; // already handled this write
    if (suppressNextWatch) { suppressNextWatch = false; lastProcessedMtime = curr.mtimeMs; return; }
    lastProcessedMtime = curr.mtimeMs;
    scheduleAudit();
  });
}

// ── HTTP request handler ──────────────────────────────────────────
function handler(req, res) {
  const url    = req.url.split('?')[0];
  const method = req.method.toUpperCase();

  // CORS headers (for file:// or alternate origins during dev)
  res.setHeader('Access-Control-Allow-Origin',  '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
  if (method === 'OPTIONS') { res.writeHead(204); res.end(); return; }

  // ── GET / ────────────────────────────────────────────────────
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

  // ── GET /data.json ───────────────────────────────────────────
  if (method === 'GET' && url === '/data.json') {
    try {
      const data = fs.readFileSync(DATA_FILE, 'utf8');
      res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
      res.end(data);
    } catch (_) {
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end('[]');
    }
    return;
  }

  // ── GET /events  (SSE) ───────────────────────────────────────
  if (method === 'GET' && url === '/events') {
    res.writeHead(200, {
      'Content-Type':  'text/event-stream',
      'Cache-Control': 'no-cache',
      'Connection':    'keep-alive',
      'X-Accel-Buffering': 'no',    // disable nginx buffering if behind proxy
    });
    res.write(': connected\n\n');   // initial comment keeps connection open

    // Send the current state immediately so a fresh page load is up-to-date
    const current = readJson();
    if (current.length) res.write(`data: ${JSON.stringify(current)}\n\n`);

    sseClients.add(res);
    req.on('close', () => sseClients.delete(res));
    return;
  }

  // ── POST /write-event ────────────────────────────────────────
  if (method === 'POST' && url === '/write-event') {
    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', () => {
      let newEvent;
      try {
        newEvent = JSON.parse(body);
      } catch (_) {
        res.writeHead(400, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Invalid JSON body' }));
        return;
      }

      // Validate required fields
      if (!newEvent.id || !newEvent.title ||
          newEvent.startTime == null || newEvent.endTime == null) {
        res.writeHead(422, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Missing required fields: id, title, startTime, endTime' }));
        return;
      }

      // Force status to pending (C++ is the authority on final status)
      newEvent.status = 'pending';

      // Merge into current data.json (upsert by id)
      const events = readJson();
      const idx    = events.findIndex(e => e.id === newEvent.id);
      if (idx >= 0) events[idx] = newEvent;
      else           events.push(newEvent);

      suppressNextWatch = true;   // our write, don't re-trigger debounce
      writeJsonAtomic(events);

      // Immediately broadcast the pending state so the UI shows it right away
      broadcast(events);

      // Trigger the C++ audit after the short debounce
      scheduleAudit();

      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ ok: true, id: newEvent.id }));
    });
    return;
  }

  // ── 404 ─────────────────────────────────────────────────────
  res.writeHead(404, { 'Content-Type': 'text/plain' });
  res.end(`Not found: ${method} ${url}`);
}

// ── Boot ──────────────────────────────────────────────────────────
// Allow overriding port via --port <n> or PORT env var
const portArg = process.argv.indexOf('--port');
const LISTEN_PORT = portArg !== -1
  ? parseInt(process.argv[portArg + 1], 10)
  : (parseInt(process.env.PORT, 10) || PORT);

const server = http.createServer(handler);

server.on('error', err => {
  if (err.code === 'EADDRINUSE') {
    console.error(`\n[watch.js] Port ${LISTEN_PORT} is already in use.`);
    console.error(`  Kill the existing process:  kill -9 $(lsof -t -i:${LISTEN_PORT})`);
    console.error(`  Or run on another port:     node watch.js --port 3001\n`);
  } else {
    console.error('[watch.js] Server error:', err.message);
  }
  process.exit(1);
});

server.listen(LISTEN_PORT, () => {
  console.log(`\nRBT Scheduler server running at http://localhost:${LISTEN_PORT}`);
  console.log(`  C++ binary : ${CPP_BINARY}`);
  console.log(`  data.json  : ${DATA_FILE}`);
  console.log(`  index.html : ${INDEX_FILE}\n`);

  // Ensure data.json exists so the watcher has something to watch
  if (!fs.existsSync(DATA_FILE)) writeJsonAtomic([]);

  startWatcher();
});

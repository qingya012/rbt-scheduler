/**
 * bridge.js — Clean JS interface over the rbt_scheduler Node addon.
 *
 * Usage
 * ─────
 *   const rbt = require('./bridge');
 *
 *   rbt.insertEvent({ id, title, startTime, endTime })
 *     → { ok: boolean, conflict: boolean, error?: string }
 *
 *   rbt.deleteEvent(id)
 *     → { ok: boolean, error?: string }
 *
 *   rbt.checkConflict(startTime, endTime)
 *     → { conflict: boolean, events: Event[] }
 *
 *   rbt.clearTree()
 *     → void
 *
 *   rbt.getAll()
 *     → Event[]
 *
 *   rbt.isAvailable   — false when the .node binary hasn't been built yet;
 *                       all functions degrade gracefully in that case.
 *
 * Building the addon
 * ──────────────────
 *   npm install          # installs node-addon-api + node-gyp
 *   npm run build        # runs node-gyp rebuild
 *
 * The compiled binary lands at:
 *   build/Release/rbt_scheduler.node
 */

'use strict';

// ── Load the compiled addon ───────────────────────────────────────
let _native = null;

try {
  _native = require('./build/Release/rbt_scheduler.node');
} catch (_) {
  // Addon not built yet — all exported functions will return safe fallbacks.
  // Run `npm run build` to compile the C++ addon.
  console.warn(
    '[bridge.js] rbt_scheduler.node not found. ' +
    'Run `npm run build` to compile the C++ addon. ' +
    'Operating in JS-only fallback mode.'
  );
}

// ── Type validation helpers ───────────────────────────────────────
function assertInt(name, value) {
  if (typeof value !== 'number' || !Number.isFinite(value)) {
    throw new TypeError(`${name} must be a finite number, got ${typeof value}`);
  }
}

// ── Public API ────────────────────────────────────────────────────

/**
 * Insert an event into the RBT.
 *
 * @param {{ id: number, title: string, startTime: number, endTime: number }} ev
 * @returns {{ ok: boolean, conflict: boolean, error?: string }}
 */
function insertEvent(ev) {
  if (!ev || typeof ev !== 'object')
    throw new TypeError('insertEvent: argument must be an event object');

  const { id = 0, title = '', startTime, endTime } = ev;
  assertInt('startTime', startTime);
  assertInt('endTime',   endTime);

  if (!_native) {
    // Fallback: cannot validate against C++ tree
    return { ok: true, conflict: false, _fallback: true };
  }

  return _native.insertEvent(
    Math.trunc(id),
    String(title),
    Math.trunc(startTime),
    Math.trunc(endTime)
  );
}

/**
 * Remove an event from the RBT by its numeric id.
 *
 * @param {number} id
 * @returns {{ ok: boolean, error?: string }}
 */
function deleteEvent(id) {
  assertInt('id', id);

  if (!_native) return { ok: true, _fallback: true };

  return _native.deleteEvent(Math.trunc(id));
}

/**
 * Check whether a time range overlaps any stored event.
 * Uses the RBT's O(log n) interval query (listConflicts).
 *
 * @param {number} startTime  week-minutes (0–10079)
 * @param {number} endTime    week-minutes (exclusive)
 * @returns {{ conflict: boolean, events: Array<{id,title,startTime,endTime}> }}
 */
function checkConflict(startTime, endTime) {
  assertInt('startTime', startTime);
  assertInt('endTime',   endTime);

  if (!_native) return { conflict: false, events: [], _fallback: true };

  return _native.checkConflict(Math.trunc(startTime), Math.trunc(endTime));
}

/**
 * Remove all events from the tree.
 */
function clearTree() {
  if (_native) _native.clearTree();
}

/**
 * Return all events currently stored in the RBT.
 *
 * @returns {Array<{id: number, title: string, startTime: number, endTime: number}>}
 */
function getAll() {
  if (!_native) return [];
  return _native.getAll();
}

// ── Module exports ────────────────────────────────────────────────
module.exports = {
  insertEvent,
  deleteEvent,
  checkConflict,
  clearTree,
  getAll,

  /** true when the native .node addon is loaded and operational */
  get isAvailable() { return _native !== null; },
};

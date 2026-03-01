/**
 * index.js — Entry-point bridge for the rbt_scheduler Node addon.
 *
 * Re-exports the full interface from bridge.js so callers can use
 * either `require('./index')` or `require('./bridge')`.
 *
 * API
 * ───
 *  insert(id, title, startMin, endMin)  → { ok, conflict, error? }
 *  delete(id)                           → { ok, error? }
 *  search(startMin, endMin)             → { conflict, events[] }
 *  clear()                              → void
 *  getAll()                             → Event[]
 *  isAvailable                          → boolean
 *
 * Type mapping
 * ────────────
 *  JS number (minutes from week-start) ──► C++ int   (startTime / endTime)
 *  JS string / number (event ID)       ──► C++ int64 (EventId)
 *  C++ bool                            ──► JS boolean
 *  C++ vector<Event>                   ──► JS Array<{ id, title, startTime, endTime }>
 */

'use strict';

const _bridge = require('./bridge');

/**
 * Insert an event into the RBT.
 * Time values are minutes from the start of the week (0 – 10 079).
 * Pass id = 0 to let the engine auto-assign one.
 *
 * @param {string|number} id
 * @param {string}        title
 * @param {number}        startMin   integer minutes
 * @param {number}        endMin     integer minutes (exclusive)
 * @returns {{ ok: boolean, conflict: boolean, error?: string }}
 */
function insert(id, title, startMin, endMin) {
  return _bridge.insertEvent({
    id:        Number(id),
    title:     String(title),
    startTime: Math.trunc(startMin),
    endTime:   Math.trunc(endMin),
  });
}

/**
 * Remove an event by its ID.
 *
 * @param {string|number} id
 * @returns {{ ok: boolean, error?: string }}
 */
function del(id) {
  return _bridge.deleteEvent(Number(id));
}

/**
 * Check whether a time window overlaps any stored event (conflict search).
 * Uses the RBT's O(log n + k) interval query.
 *
 * @param {number} startMin
 * @param {number} endMin
 * @returns {{ conflict: boolean, events: Array<{id,title,startTime,endTime}> }}
 */
function search(startMin, endMin) {
  return _bridge.checkConflict(Math.trunc(startMin), Math.trunc(endMin));
}

/**
 * Clear all events from the tree.
 */
function clear() {
  _bridge.clearTree();
}

/**
 * Return every event currently in the tree.
 *
 * @returns {Array<{id: number, title: string, startTime: number, endTime: number}>}
 */
function getAll() {
  return _bridge.getAll();
}

module.exports = {
  insert,
  delete: del,      // 'delete' is a reserved word; alias works fine as a property key
  search,
  clear,
  getAll,
  get isAvailable() { return _bridge.isAvailable; },
};

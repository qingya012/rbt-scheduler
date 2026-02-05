# Red-Black Tree Scheduler (C++)

A calendar / scheduler core implemented in **C++**, using a **Red-Black Tree with interval augmentation** to efficiently manage time-based events.

This project reimplements common calendar operations (add, remove, reschedule, search, duplicate events), but focuses on **data structure design and performance guarantees** rather than UI or full calendar integrations.

---

## Motivation

Many basic calendar or scheduler implementations rely on linear data structures (e.g. linked lists), which makes conflict detection and scheduling queries inefficient.

This project explores how a **Red-Black Tree–based interval scheduler** can:
- maintain events in sorted order by time
- detect overlapping events efficiently
- support fast rescheduling and range queries
- act as a reusable scheduling core independent of any UI

The goal of this project is not to reinvent calendar features, but to study how **balanced trees and interval augmentation** can be applied to a real-world scheduling problem.

---

## Core Features

- Add, remove, and reschedule events
- Fast conflict detection for overlapping time intervals
- Search for events by time or time range
- Duplicate events (with optional time shifting)
- Guaranteed logarithmic-time operations

---

## Event Model

Each event contains:
- `id` (unique identifier)
- `startTime`
- `endTime`
- `title`
- optional metadata (e.g. tags, priority)

Time intervals are treated as **half-open intervals**:

`[startTime, endTime)`

This simplifies boundary handling and avoids ambiguity when events touch but do not overlap.

---

## Data Structure Design

### Red-Black Tree

Events are stored in a Red-Black Tree ordered by the composite key:

`[startTime, id)`

This ensures:
- total ordering of events
- support for multiple events starting at the same time
- `O(log n)` insertion, deletion, and search

### Interval Augmentation

Each node maintains an additional field:

`maxEnd = maximum endTime within its subtree`

This augmentation allows efficient detection of overlapping intervals and enables fast scheduling queries without scanning all events.

---

## Supported Operations

| Operation | Description | Time Complexity |
|--------|------------|----------------|
| Add Event | Insert a new event | `O(log n)` |
| Remove Event | Delete an event by ID | `O(log n)` |
| Reschedule Event | Update an event’s time interval | `O(log n)` |
| Conflict Detection | Check for overlapping events | `O(log n)` |
| Next Event Search | Find the next event after time `t` | `O(log n)` |
| Range Query | List events in `[t1, t2)` | `O(log n + k)` |

(`k` is the number of returned events.)

---

## Duplicate Events

The scheduler supports duplicating events:
- exact copies with new IDs
- time-shifted duplicates (e.g. weekly repetition)
- conflict-aware duplication (overlaps are detected and handled explicitly)

This feature serves as a foundation for recurring events.

---

## Project Scope

This project is intentionally **backend-focused**.

What it **is**:
- a data-structure-driven scheduling engine
- a performance-oriented reimplementation of a common system
- a foundation for future extensions

What it **is not**:
- a full calendar application
- a UI-heavy product
- a direct integration with external calendar services

---

## Possible Extensions

- Import/export `.ics` calendar files
- Automatic free-slot suggestions
- Weekly or monthly recurring events
- REST API wrapper for frontend integration
- CLI or lightweight visualization interface

---

## Why Red-Black Trees?

Compared to a linked-list-based scheduler, this approach:
- avoids linear scans for conflict detection
- provides predictable worst-case performance
- demonstrates how classic balanced trees can be adapted for interval-based problems

---

## Build & Usage

```bash
# example
make
./scheduler

`(Exact usage depends on the chosen interface.)`


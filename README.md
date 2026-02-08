# Weekly Event Scheduler (C++)

A **weekly event scheduler core** implemented in **C++**, built around a **Red-Black Tree with interval augmentation**.

This project focuses on the **core scheduling logic**—event ordering, conflict detection, rescheduling, and time-slot queries—while intentionally staying independent of any UI or calendar platform.  
The scheduler is designed as a reusable engine that could later be wrapped by a CLI, UI, or calendar plugin, without modifying its core logic.



## Motivation

Many scheduler or calendar implementations rely on linear data structures, which makes conflict detection and scheduling queries inefficient and hard to scale conceptually.

This project explores how a **balanced tree–based interval scheduler** can be used to manage weekly events with:
- predictable worst-case performance
- explicit conflict awareness
- clean separation between core logic and external interfaces

Rather than building a full calendar product, this project treats scheduling as a **data-structure and systems design problem**.



## Scope

The scheduler operates within a **single weekly time window**:
- Time range: one week (e.g. Monday 00:00 → Sunday 24:00)
- Time granularity: minutes
- Single user, no persistence across weeks

This constrained scope keeps the system predictable and allows the core logic to remain clean and testable.



## Core Features

- Add, remove, and reschedule weekly events
- Detect overlapping events efficiently
- Search events by time or time range
- Duplicate weekly events with conflict awareness
- Suggest available time slots given a desired duration

All operations are implemented with clear performance guarantees.



## Event Model

Each event contains:
- `id` (unique identifier)
- `startTime` and `endTime` (minutes relative to the start of the week)
- `title`
- optional metadata (e.g. tags or priority)

Time intervals are treated as **half-open intervals**:

`[startTime, endTime)`


This avoids ambiguity when events touch but do not overlap.



## Data Structure Design

### Red-Black Tree

Events are stored in a Red-Black Tree ordered by a composite key:

`（startTime, id)`


This ensures:
- total ordering of events
- support for multiple events with identical start times
- `O(log n)` insertion, deletion, and search

### Interval Augmentation

Each tree node maintains an additional field:

`maxEnd = maximum endTime within its subtree`


This augmentation enables efficient overlap detection and interval queries without scanning all events.



## Supported Operations

| Operation | Description | Time Complexity |
|--------|------------|----------------|
| Add Event | Insert a new weekly event | `O(log n)` |
| Remove Event | Delete an event by ID | `O(log n)` |
| Reschedule Event | Update an event’s time interval | `O(log n)` |
| Conflict Detection | Check for overlapping events | `O(log n)` |
| Range Query | List events in `[t1, t2)` | `O(log n + k)` |
| Slot Suggestion | Find available time slots | `O(log n + k)` |

(`k` is the number of returned events.)



## Design Philosophy

- **Core-only**: The scheduler contains no UI, file I/O, or platform-specific logic.
- **Plugin-friendly**: All inputs and outputs are structured data, making it easy to wrap the core with a CLI, UI, or calendar adapter.
- **Not locked-in**: The design avoids assumptions about frontend, storage, or calendar providers.

---

## What This Project Is / Is Not

**This project is:**
- a backend scheduling engine
- a Red-Black Tree interval scheduling exercise
- a foundation for future adapters or plugins

**This project is not:**
- a full calendar application
- a UI-focused product
- a direct integration with external calendar services



## Possible Extensions

- CLI wrapper for interactive scheduling
- Import/export via standard calendar formats (e.g. `.ics`)
- REST API adapter for frontend integration
- Multi-week or recurring schedule support

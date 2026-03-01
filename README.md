# Weekly Event Scheduler

A specialized full-stack scheduling system featuring a **C++ Interval-Augmented Red-Black Tree** core, exposed via a **Node.js Native Addon**, and monitored with **microsecond-level precision**.
Live Demo:
Frontend → https://qingya012.github.io/rbt-scheduler/
Backend API → https://rbt-scheduler.onrender.com/



## Architecture
```
C++ Core (Interval RB-Tree)
        ↓
Node.js Native Addon (node-gyp)
        ↓
Express REST API
        ↓
Frontend (GitHub Pages)
```

The scheduling engine is implemented in pure C++ for performance and correctness.
It is wrapped using a Node native addon, allowing it to be exposed through a REST API and accessed from a browser-based UI.



## Core Scheduling Engine (C++)

The scheduler operates within a single weekly time window:
- Time range: Monday 00:00 → Sunday 24:00
- Time granularity: minutes
- Single user scope



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



## 🌐 REST API Layer

The C++ engine is exposed through a Node.js native addon and wrapped with an Express API.

Example endpoints:

```
GET /health
POST /addEvent
POST /removeEvent
GET /events
```

The backend is deployed on Render.



## 🖥 Frontend

The frontend is a lightweight static interface deployed via GitHub Pages.

It communicates with the deployed backend using fetch-based REST calls.



## 🧪 Testing

The C++ core is tested using:

- CMake
- GoogleTest

This ensures correctness of:

- Tree balancing
- Interval augmentation
- Conflict detection logic



## 🎯 Design Goals

- Separation of concerns (core vs API vs UI)
- Native performance with web accessibility
- Plugin-ready scheduling engine
- Clear complexity guarantees



## 🔮 Possible Extensions

- Multi-week scheduling
- Recurring events
- Calendar import/export (.ics)
- Multi-user support
- Database persistence

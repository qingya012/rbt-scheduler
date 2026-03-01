#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

using namespace std;

namespace rbt {

/**
 * Weekly time constants (minute-based).
 * A week is represented as minutes from week start (e.g., Monday 00:00).
 */
constexpr int DAYS_IN_WEEK = 7;
constexpr int HOURS_PER_DAY = 24;
constexpr int MINUTES_PER_HOUR = 60;
constexpr int MINUTES_PER_DAY = HOURS_PER_DAY * MINUTES_PER_HOUR;
constexpr int MINUTES_PER_WEEK = DAYS_IN_WEEK * MINUTES_PER_DAY;

// Optional display / rounding settings
constexpr int DEFAULT_MINUTE_QUANTA = 15;

/**
 * A time range in minutes relative to the start of the week.
 * We use half-open intervals: [start, end)
 */
struct TimeRange {
    int start = 0;  // inclusive, 0..MINUTES_PER_WEEK
    int end   = 0;  // exclusive, 0..MINUTES_PER_WEEK

    bool isValid() const { return 0 <= start && start < end && end <= MINUTES_PER_WEEK; }
};

/**
 * Optional extensibility for plugin/UI layers:
 * - You can later store provider-specific IDs, categories, colors, etc.
 * - Keep core independent: core treats this as opaque data.
 */
using Metadata = std::unordered_map<std::string, std::string>;

using EventId = std::int64_t;

/**
 * Event model (weekly scope).
 */
struct Event {
    EventId id = 0;            // unique identifier in this scheduler
    std::string title;         // name/title
    TimeRange range;           // [start,end) in week-minutes
    Metadata meta;             // optional key-value metadata (tags, source, etc.)
};

/**
 * Day view (for convenience). This is NOT the main storage.
 * Core storage should be the RB-tree ordered by (start, id).
 */
struct Day {
    int dayIndex = 0;                  // 0..6 (interpretation defined by adapter/UI)
    std::vector<EventId> eventIds;     // IDs that fall on this day (optional helper)
};

/**
 * Week view (optional helper).
 * The core can generate this on demand.
 */
struct Week {
    Day days[DAYS_IN_WEEK];
};

/**
 * Status codes for scheduler operations.
 */
enum class Status {
    OK = 0,
    INVALID_TIME_RANGE,
    NOT_FOUND,
    CONFLICT,
    DUPLICATE_ID
};

/**
 * Conflict info (useful for UI/plugin layers).
 */
struct Conflict {
    EventId existingId = 0;
    EventId incomingId = 0;   // for add/reschedule you can set this
    TimeRange overlap;        // intersection range (if you compute it)
};

/**
 * Result wrapper: avoids exceptions and keeps core easy to embed.
 */
template <typename T>
struct Result {
    Status status = Status::OK;
    T value{};
};

// ======================
// Time helper utilities
// ======================

static inline bool sameDay(int t1, int t2) {
    return t1 / MINUTES_PER_DAY == t2 / MINUTES_PER_DAY;
}

static inline int combineDayAndMinute(int day, int minute) {
    return day * MINUTES_PER_DAY + minute;
}

/**
 * Convert a minute offset (0..MINUTES_PER_WEEK-1) into a human-readable
 * "Day HH:MM" string (e.g., "Monday 08:30").
 */
std::string formatWeekMinutes(int minutes);

/**
 * Scheduler: a weekly event scheduler core.
 *
 * Design goals:
 * - Plugin-friendly: clean API, structured data
 * - Efficient conflict detection via RB-tree interval augmentation (maxEnd)
 */
class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    // Non-copyable (tree owns pointers); movable if you want later.
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    /**
     * Add a new event.
     * If allowOverlap=false, rejects if the event overlaps any existing event.
     */
    Status addEvent(const Event& e, bool allowOverlap = false);

    /**
     * Remove an event by id.
     */
    Status removeEvent(EventId id);

    /**
     * Reschedule an existing event (update its time range).
     * Optionally also update title/meta by passing a full Event.
     */
    Status rescheduleEvent(EventId id, const TimeRange& newRange, bool allowOverlap = false);

    /**
     * Fetch an event by id (returns empty if not found).
     */
    optional<Event> getEvent(EventId id) const;

    /**
     * Query events whose start time lies in [range.start, range.end).
     * (You can later enhance to true interval intersection queries.)
     */
    vector<Event> queryByStartInRange(const TimeRange& range) const;

    /**
     * Query events that intersect the given time range.
     * This is the "interval" query that benefits from maxEnd augmentation.
     */
    vector<Event> queryIntersecting(const TimeRange& range) const;

    /**
     * Conflict checks.
     * - hasConflict: quick boolean
     * - findAnyConflict: returns one conflicting event if exists
     * - listConflicts: returns all conflicts (may be O(log n + k))
     */
    bool hasConflict(const TimeRange& range) const;
    optional<Event> findAnyConflict(const TimeRange& range) const;
    vector<Event> listConflicts(const TimeRange& range) const;

    /**
     * Find next event whose start >= t (week-minutes).
     */
    optional<Event> nextEvent(int t) const;

    /**
     * Duplicate an event. Common weekly use case:
     * - shiftMinutes can be +MINUTES_PER_DAY (next day), +7*MINUTES_PER_DAY (next week), etc.
     * If allowOverlap=false, duplication fails on conflicts.
     */
    Result<EventId> duplicateEvent(EventId id, int shiftMinutes, bool allowOverlap = false);

    /**
     * Suggest available time slots in a window [window.start, window.end),
     * requiring a minimum duration in minutes.
     * Returns up to k suggestions.
     */
    vector<TimeRange> suggestSlots(const TimeRange& window, int durationMinutes, int k = 5) const;

    /**
     * Optional helper: generate a week/day view from stored events.
     * (Pure convenience for UI; core storage remains RB-tree.)
     */
    Week toWeekView() const;

    /**
     * Export all events as a JSON array string.
     * Each element includes: id, title, startTime, endTime, and node color.
     * Intended primarily for visualization/debugging of the RB-tree.
     */
    std::string exportToJson() const;

    /**
     * Persist all events to a JSON file.
     * Each event includes: id, title, startTime, endTime, plus readable labels.
     *
     * @return true on success, false on I/O or parse errors.
     */
    bool saveToFile(const std::string& filename) const;

    /**
     * Load events from a JSON file and rebuild the tree.
     * The file must contain an array of objects with id, title, startTime, endTime.
     * Existing events are cleared only if the file is valid.
     *
     * @return true on success, false if the file is missing, malformed, or contains invalid data.
     */
    bool loadFromFile(const std::string& filename);

    /**
     * Clear all events.
     */
    void clear();

    /**
     * Size of scheduler (# of events).
     */
    std::size_t size() const;
    
    void debugInsert(const Event& e); // for testing: insert without checks (e.g. for building tree from bulk data)
    void dump() const; // for debugging: print all events in order

    // ---------- Future plugin/adapter hooks (stubs) ----------
    // You can implement these later without changing the core logic.

    /**
     * Export events to a portable representation (e.g. ICS/JSON) in adapter layer.
     * Keep core independent: core only returns structured events.
     */
    std::vector<Event> exportAllEvents() const;

    

private:
    // ---------- RB-tree internals ----------
    // Key is (startTime, id) to ensure total ordering.
    struct Key {
        int start = 0;
        EventId id = 0;
    };

    enum class Color { RED, BLACK };

    struct Node {
        Event event;

        // Interval augmentation: max end time in this subtree.
        int maxEnd = 0;

        Color color = Color::RED;
        Node* parent = nullptr;
        Node* left = nullptr;
        Node* right = nullptr;

        explicit Node(const Event& e)
            : event(e), maxEnd(e.range.end) {}
    };

    // Sentinel NIL node pattern (common RB-tree approach). You can also use nullptrs instead.
    Node* nil_;
    Node* root_;
    EventId nextId_ = 1; // for generating new IDs

    // Optional fast lookup by id -> Key (helps remove/get in O(1) + O(log n)).
    std::unordered_map<EventId, Key> index_;

    // RB-tree helpers
    void rotateLeft(Node* x);
    void rotateRight(Node* y);
    void insertFixup(Node* z);
    void deleteFixup(Node* x);

    void treeInsert(Node* z);
    void treeDelete(Node* z);

    Node* minimum(Node* x) const;
    Node* successor(Node* x) const;
    void transplant(Node* u, Node* v); // helper for delete (replaces subtree rooted at u with v)

    // Key comparison
    static bool keyLess(const Key& a, const Key& b);

    // Search
    Node* findNodeByKey(const Key& key) const;
    Node* lowerBoundByStart(int start) const;
    Node* findNodeById(Node* x, EventId id) const; // internal helper for get/remove/reschedule

    // Augmentation maintenance
    void updateNode(Node* x);
    void updateUpwards(Node* x);
    int recomputeSubtreeMaxEnd(Node* x); // full subtree recomputation (used as a safety net after deletes)

    // Interval operations
    bool overlaps(const TimeRange& a, const TimeRange& b) const;
    void collectIntersecting(Node* x, const TimeRange& range, std::vector<Event>& out) const;

    // Traversal helpers
    void collectInorder(Node* node, std::vector<Event>& result) const;
    void exportNodeToJson(Node* node, std::string& out, bool& first) const;

    void dumpInorder(Node* x) const; // internal helper
    bool containsId(EventId id) const; // internal helper for testing
    void postOrderDelete(Node* node); // helper for destructor: post-order traversal to delete nodes

};

} // namespace rbt

#endif // SCHEDULER_H

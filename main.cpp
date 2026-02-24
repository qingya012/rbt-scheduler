#include "Scheduler.h"
#include <iostream>

using namespace rbt;

static int g_fail = 0;

static void EXPECT(bool condition, const char* message) {
    if (!condition) {
        cerr << "[FAIL] " << message << endl;
        g_fail++;
    } else {
        cerr << "[PASS] " << message << endl;
    }
}

static void test_insert_and_conflict() {
    cerr << "\n=== test_insert_and_conflict ===\n";

    Scheduler scheduler;

    Event e1{1, "e1", TimeRange{10,20}, {}};
    Event e2{2, "e2", TimeRange{30,40}, {}};
    Event e3{3, "e3", TimeRange{15,25}, {}};

    EXPECT(scheduler.addEvent(e1, false) == Status::OK, "Insert e1 should succeed");
    EXPECT(scheduler.addEvent(e2, false) == Status::OK, "Insert e2 should succeed");
    EXPECT(scheduler.addEvent(e3, false) == Status::CONFLICT, "Insert e3 should fail due to conflict");

    EXPECT(scheduler.hasConflict(TimeRange{12, 18}), "Should detect conflict with range 12-18");
    EXPECT(!scheduler.hasConflict(TimeRange{25,29}), "Should not detect conflict with range 25-29");
    EXPECT(!scheduler.hasConflict(TimeRange{20,30}), "Should not detect conflict with range 20-30");
}

static void test_remove() {
    cerr << "\n=== test_remove ===\n";

    Scheduler scheduler;

    Event e1{1, "e1", TimeRange{10,20}, {}};
    Event e2{2, "e2", TimeRange{30,40}, {}};

    EXPECT(scheduler.addEvent(e1, false) == Status::OK, "Insert e1 should succeed");
    EXPECT(scheduler.addEvent(e2, false) == Status::OK, "Insert e2 should succeed");

    EXPECT(scheduler.removeEvent(1) == Status::OK, "Remove e1 should succeed");
    scheduler.dump();

    cerr << "containsId(1) = " << scheduler.getEvent(1).has_value() << "\n";

    EXPECT(scheduler.removeEvent(1) == Status::NOT_FOUND, "Remove e1 again should fail");
    scheduler.dump();

    EXPECT(scheduler.hasConflict(TimeRange{12, 18}) == false, "Should not detect conflict with range 12-18 after removal");
    EXPECT(scheduler.hasConflict(TimeRange{31, 39}) == true, "Should detect conflict with range 30-40");

}

static void test_reschedule() {
    cerr << "\n=== test_reschedule ===\n";

    Scheduler scheduler;

    Event e1{1, "e1", TimeRange{10,20}, {}};
    Event e2{2, "e2", TimeRange{30,40}, {}};

    EXPECT(scheduler.addEvent(e1, false) == Status::OK, "Insert e1 should succeed");
    EXPECT(scheduler.addEvent(e2, false) == Status::OK, "Insert e2 should succeed");

    EXPECT(scheduler.rescheduleEvent(1, TimeRange{15,35}, false) == Status::CONFLICT, "Reschedule e1 to 15-35 should fail due to conflict");
    EXPECT(scheduler.rescheduleEvent(1, TimeRange{20,30}, false) == Status::OK, "Reschedule e1 to 20-30 should succeed");
    scheduler.dump();

    EXPECT(scheduler.hasConflict(TimeRange{12, 18}) == false, "Should not detect conflict with range 12-18 after reschedule");
    EXPECT(scheduler.hasConflict(TimeRange{31, 39}) == true, "Should detect conflict with range 30-40");
}

static void test_duplicate() {
    cerr << "\n=== test_duplicate ===\n";

    Scheduler scheduler;

    Event e1{1, "e1", TimeRange{10,20}, {}};

    EXPECT(scheduler.addEvent(e1, false) == Status::OK, "Insert e1 should succeed");

    auto dupResult = scheduler.duplicateEvent(1, 15, false);
    EXPECT(dupResult.status == Status::OK, "Duplicate e1 with shift 15 should succeed");
    EXPECT(dupResult.value != 0, "Duplicate should return new event ID");

    auto dupEventOpt = scheduler.getEvent(dupResult.value);
    EXPECT(dupEventOpt.has_value(), "Should be able to fetch duplicated event");
    if (dupEventOpt.has_value()) {
        Event dupEvent = dupEventOpt.value();
        cerr << "start=" << dupEvent.range.start << " end=" << dupEvent.range.end << "\n";
        EXPECT(dupEvent.range.start == 25 && dupEvent.range.end == 35, "Duplicated event should have correct shifted time range");
        EXPECT(dupEvent.title == "e1", "Duplicated event should have same title");
    }
}

static void test_destructor() {
    cerr << "\n=== test_destructor ===\n";

    {
        Scheduler scheduler;
        Event e1{1, "e1", TimeRange{10,20}, {}};
        Event e2{2, "e2", TimeRange{30,40}, {}};
        Event e3{3, "e3", TimeRange{50,60}, {}};

        EXPECT(scheduler.addEvent(e1, false) == Status::OK, "Insert e1 should succeed");
        EXPECT(scheduler.addEvent(e2, false) == Status::OK, "Insert e2 should succeed");
        EXPECT(scheduler.addEvent(e3, false) == Status::OK, "Insert e3 should succeed");

        scheduler.clear();
        EXPECT(scheduler.size() == 0, "Scheduler should be empty after clear");

        for (int i = 0; i < 1000; i++) {
            Event e{i+1, "event", TimeRange{10*i, 10*i + 5}, {}};
            EXPECT(scheduler.addEvent(e, false) == Status::OK, "Insert event should succeed");
        }

        EXPECT(scheduler.size() == 1000, "Scheduler should contain 1000 events");
    } // scheduler goes out of scope here, destructor should clean up

    cerr << "If we reach here without memory issues, destructor works.\n";
}

static void test_nextEvent() {
    cerr << "\n=== test_nextEvent ===\n";

    Scheduler scheduler;

    Event e1{1, "e1", TimeRange{10,20}, {}};
    Event e2{2, "e2", TimeRange{30,40}, {}};
    Event e3{3, "e3", TimeRange{50,60}, {}};

    EXPECT(scheduler.addEvent(e1, false) == Status::OK, "Insert e1 should succeed");
    EXPECT(scheduler.addEvent(e2, false) == Status::OK, "Insert e2 should succeed");
    EXPECT(scheduler.addEvent(e3, false) == Status::OK, "Insert e3 should succeed");

    auto nextOpt = scheduler.nextEvent(25);
    EXPECT(nextOpt.has_value(), "Should find next event after time 25");
    if (nextOpt.has_value()) {
        EXPECT(nextOpt->id == 2, "Next event after time 25 should be e2");
    }

    nextOpt = scheduler.nextEvent(45);
    EXPECT(nextOpt.has_value(), "Should find next event after time 45");
    if (nextOpt.has_value()) {
        EXPECT(nextOpt->id == 3, "Next event after time 45 should be e3");
    }

    nextOpt = scheduler.nextEvent(60);
    EXPECT(!nextOpt.has_value(), "Should not find next event after time 60");
}

static void test_query_functions() {
    cerr << "\n=== test_query_functions ===\n";

    Scheduler scheduler;

    Event e1{1, "e1", TimeRange{10,20}, {}};
    Event e2{2, "e2", TimeRange{30,40}, {}};
    Event e3{3, "e3", TimeRange{55,65}, {}};

    EXPECT(scheduler.addEvent(e1, false) == Status::OK, "Insert e1 should succeed");
    EXPECT(scheduler.addEvent(e2, false) == Status::OK, "Insert e2 should succeed");
    EXPECT(scheduler.addEvent(e3, false) == Status::OK, "Insert e3 should succeed");

    auto eventsInRange = scheduler.queryByStartInRange(TimeRange{0, 25});
    EXPECT(eventsInRange.size() == 1, "Should find 1 event starting in range 0-25");

    auto intersectingEvents = scheduler.queryIntersecting(TimeRange{35, 67});
    EXPECT(intersectingEvents.size() == 2, "Should find 2 events intersecting range 35-67");
}

static void test_suggestSlots() {
    cerr << "\n=== test_suggestSlots ===\n";

    Scheduler scheduler;

    // Add some busy events:
    // Event A: 10-30
    // Event B: 40-50
    // Event C: 55-70
    Event e1{1, "A", TimeRange{10,30}, {}};
    Event e2{2, "B", TimeRange{40,50}, {}};
    Event e3{3, "C", TimeRange{55,70}, {}};

    EXPECT(scheduler.addEvent(e1, false) == Status::OK, "Insert A");
    EXPECT(scheduler.addEvent(e2, false) == Status::OK, "Insert B");
    EXPECT(scheduler.addEvent(e3, false) == Status::OK, "Insert C");

    // Ask for slots in window [0, 80), require duration 5, up to 3 slots
    auto slots = scheduler.suggestSlots(TimeRange{0, 80}, 5, 3);

    // Expected gaps: [0,10], [30,40], [50,55], [70,80]
    // With duration 5: [0,10] (10-0=10), [30,40] (10), [50,55] (5), [70,80] (10)
    // Up to k=3 slots so result should be first three: [0,10],[30,40],[50,55]
    EXPECT(slots.size() == 3, "Should find 3 slots");

    if (slots.size() >= 3) {
        EXPECT(slots[0].start == 0 && slots[0].end == 10,   "Slot 1: [0,10]");
        EXPECT(slots[1].start == 30 && slots[1].end == 40,  "Slot 2: [30,40]");
        EXPECT(slots[2].start == 50 && slots[2].end == 55,  "Slot 3: [50,55]");
    }

    // Ask for a slot that requires a longer duration (duration=15)
    auto longslots = scheduler.suggestSlots(TimeRange{0, 80}, 15, 2);
    // Only [0,10],[30,40],[70,80] have length at least 10, but only [30,40] and [70,80] are at least 10, none are >=15 except perhaps [70,80], which is 10. So zero or one slot?
    // [0,10] = 10, [30,40]=10, [70,80]=10 => none of the slots meet >=15 requirement. Should be zero slots.
    EXPECT(longslots.size() == 0, "No slots of duration >=15 available");

    // Slot exactly matches duration
    auto fiveslot = scheduler.suggestSlots(TimeRange{50,55}, 5, 1);
    EXPECT(fiveslot.size() == 1, "One slot exactly 5 minutes");
    if (fiveslot.size() == 1) {
        EXPECT(fiveslot[0].start == 50 && fiveslot[0].end == 55, "Slot [50,55]");
    }

    // Window inside a busy event (should find no slot)
    auto nowin = scheduler.suggestSlots(TimeRange{12,29}, 5, 2);
    EXPECT(nowin.size() == 0, "No free slot inside a busy event");

    // Window completely outside any event but smaller than required duration
    auto tiny = scheduler.suggestSlots(TimeRange{80,82}, 3, 1);
    EXPECT(tiny.size() == 0, "Tiny window does not fit required duration");

    // Window is empty
    auto empty = scheduler.suggestSlots(TimeRange{20,20}, 1, 2);
    EXPECT(empty.size() == 0, "Empty window yields no slots");
}

int main() {
    test_insert_and_conflict();
    test_remove();
    test_reschedule();
    test_duplicate();
    test_destructor();
    test_nextEvent();
    test_query_functions();
    test_suggestSlots();

    if (g_fail == 0) {
        cerr << "\nALL TESTS PASSED\n" << endl;
        return 0;
    }

    cerr << "\nFAILED: " << g_fail << endl;
    return 1;
}
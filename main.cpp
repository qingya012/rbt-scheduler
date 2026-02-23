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

int main() {
    test_insert_and_conflict();
    test_remove();
    test_reschedule();
    test_duplicate();
    test_destructor();
    test_nextEvent();
    test_query_functions();

    if (g_fail == 0) {
        cerr << "\nALL TESTS PASSED\n" << endl;
        return 0;
    }

    cerr << "\nFAILED: " << g_fail << endl;
    return 1;
}
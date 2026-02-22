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

int main() {
    test_insert_and_conflict();
    test_remove();
    test_reschedule();

    if (g_fail == 0) {
        cerr << "\nALL TESTS PASSED\n" << endl;
        return 0;
    }

    cerr << "\nFAILED: " << g_fail << endl;
    return 1;
}
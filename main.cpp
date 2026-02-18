#include "Scheduler.h"
#include <iostream>

using namespace rbt;
using namespace std;

int main() {
    Scheduler scheduler;

    scheduler.debugInsert(Event{1, "e1", TimeRange{10,20}, {}});
    scheduler.debugInsert(Event{2, "e2", TimeRange{30,40}, {}});
    scheduler.debugInsert(Event{3, "e3", TimeRange{15,25}, {}});

    cout << scheduler.hasConflict(TimeRange{12, 18}) << endl; // should be true (overlaps with 10-20 and 15-25)

    return 0;
}
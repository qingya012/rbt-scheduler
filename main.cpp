#include "Scheduler.h"
#include <iostream>

using namespace rbt;
using namespace std;

int main() {
    cerr << "START" << endl;

    Scheduler scheduler;
    cerr << "after ctor" << endl;

    scheduler.debugInsert(Event{1, "e1", TimeRange{10,20}, {}});
    cerr << "after insert e1" << endl;

    scheduler.debugInsert(Event{2, "e2", TimeRange{30,40}, {}});
    cerr << "after insert e2" << endl;

    scheduler.debugInsert(Event{3, "e3", TimeRange{15,25}, {}});
    cerr << "after insert e3" << endl;

    cerr << "before conflict" << endl;
    cout << scheduler.hasConflict(TimeRange{12, 18}) << endl; // should be 1 (overlaps with 10-20 and 15-25)
    cerr << "after conflict" << endl;

    cout << scheduler.hasConflict(TimeRange{25,29}) << endl; // should be 0 (no overlap)

    cout << scheduler.hasConflict(TimeRange{20,30}) << endl; // should be 1 (overlaps with 30-40)

    return 0;
}
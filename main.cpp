#include "Scheduler.h"
#include <iostream>

using namespace rbt;
using namespace std;

int main() {
    cerr << "START" << endl;

    Scheduler scheduler;
    cerr << "after ctor" << endl;

    Event e1{1, "e1", TimeRange{10,20}, {}};
    Event e2{2, "e2", TimeRange{30,40}, {}};
    Event e3{3, "e3", TimeRange{15,25}, {}};

    cout << (scheduler.addEvent(e1, false) == Status::OK) << endl; // should be 1 (success)
    cerr << "after insert e1" << endl;

    cout << (scheduler.addEvent(e2, false) == Status::OK) << endl; // should be 1 (success)
    cerr << "after insert e2" << endl;

    cout << (scheduler.addEvent(e3, false) == Status::OK) << endl; // should be 0 (conflict)
    cerr << "after insert e3" << endl;

    cerr << "before conflict" << endl;
    cout << scheduler.hasConflict(TimeRange{12, 18}) << endl; // should be 1 (overlaps with 10-20 and 15-25)
    cerr << "after conflict" << endl;

    scheduler.dump();

    cout << scheduler.hasConflict(TimeRange{25,29}) << endl; // should be 0 (no overlap)

    cout << scheduler.hasConflict(TimeRange{20,30}) << endl; // should be 0 (no overlap)

    return 0;
}
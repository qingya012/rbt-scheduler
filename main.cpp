#include "Scheduler.h"
#include <iostream>

using namespace rbt;

int main() {
    Scheduler scheduler;

    // ---------------------------------------------------------------
    // Week minute offsets:
    //   Monday    starts at    0  (day 0 * 1440)
    //   Tuesday   starts at 1440  (day 1 * 1440)
    //   Wednesday starts at 2880  (day 2 * 1440)
    //   Thursday  starts at 4320  (day 3 * 1440)
    //   Friday    starts at 5760  (day 4 * 1440)
    //   Saturday  starts at 7200  (day 5 * 1440)
    //   Sunday    starts at 8640  (day 6 * 1440)
    // ---------------------------------------------------------------

    // Event 1 – Monday 09:00–10:30  (540–630)
    Event e1{1, "CS Lecture", TimeRange{540, 630}, {}};

    // Event 2 – Tuesday 14:00–16:00  (1440+840=2280 – 1440+960=2400)
    Event e2{2, "Gym Session", TimeRange{2280, 2400}, {}};

    // Event 3 – Wednesday 10:00–11:00  (2880+600=3480 – 2880+660=3540)
    Event e3{3, "Team Meeting", TimeRange{3480, 3540}, {}};

    // Event 4 – Friday 18:00–20:00  (5760+1080=6840 – 5760+1200=6960)
    Event e4{4, "Dinner with Friends", TimeRange{6840, 6960}, {}};

    // Event 5 – Saturday 11:00–13:00  (7200+660=7860 – 7200+780=7980)
    // A weekend event exercises a different day bucket in toWeekView()
    // and the label formatter, making it useful for visualization testing.
    Event e5{5, "Weekend Run", TimeRange{7860, 7980}, {}};

    scheduler.addEvent(e1);
    scheduler.addEvent(e2);
    scheduler.addEvent(e3);
    scheduler.addEvent(e4);
    scheduler.addEvent(e5);

    const std::string filename = "data.json";
    if (!scheduler.saveToFile(filename)) {
        std::cerr << "Error: failed to write " << filename << "\n";
        return 1;
    }

    std::cout << "Successfully generated " << filename
              << " with " << scheduler.size() << " events.\n";
    return 0;
}

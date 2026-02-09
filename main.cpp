#include "Scheduler.h"
#include <iostream>

using namespace rbt;

int main() {
    Scheduler scheduler;

    std::cout << "Scheduler initialized.\n";
    std::cout << "Current size: " << scheduler.size() << "\n";

    return 0;
}
#include "Scheduler.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>

using namespace rbt;
using json = nlohmann::json;

// ── Helpers ───────────────────────────────────────────────────────

static void printUsage(const char* prog) {
    std::cerr
        << "Usage:\n"
        << "  " << prog << "\n"
        << "      Generate sample data.json with preset events.\n\n"
        << "  " << prog << " --add \"Title\" <startMin> <endMin>\n"
        << "      Load data.json, insert the event, save back.\n"
        << "      startMin / endMin are week-minutes (0–10079).\n"
        << "      Exits with code 1 and prints CONFLICT if the slot is taken.\n\n"
        << "  " << prog << " --audit [file]\n"
        << "      Read data.json (or [file]), rebuild the RBT, audit every\n"
        << "      event marked status=pending, write back status=confirmed or\n"
        << "      status=conflict (with conflictWith id/title). Exits 0 always.\n";
}

// ── --add mode ────────────────────────────────────────────────────

static int runAdd(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Error: --add requires exactly 3 arguments: \"Title\" startMin endMin\n";
        printUsage(argv[0]);
        return 1;
    }

    const std::string title    = argv[2];
    int startMin = 0, endMin = 0;

    try {
        startMin = std::stoi(argv[3]);
        endMin   = std::stoi(argv[4]);
    } catch (const std::exception&) {
        std::cerr << "Error: startMin and endMin must be integers.\n";
        return 1;
    }

    if (startMin < 0 || endMin > MINUTES_PER_WEEK || endMin <= startMin) {
        std::cerr << "Error: invalid time range ["
                  << startMin << ", " << endMin << ").\n"
                  << "  startMin must be >= 0, endMin <= " << MINUTES_PER_WEEK
                  << ", and endMin > startMin.\n";
        return 1;
    }

    const std::string filename = "data.json";
    Scheduler scheduler;

    // Load existing schedule (missing file is fine – starts empty)
    if (!scheduler.loadFromFile(filename)) {
        std::cerr << "Warning: could not read " << filename
                  << " – starting with an empty schedule.\n";
    }

    // Build the new event; id=0 lets addEvent pick the next available id
    // (loadFromFile advances nextId_ past every loaded id).
    // We pass id=0 here so the scheduler auto-assigns; but addEvent requires
    // a non-zero id. Use size()+1 as a safe candidate, then let the scheduler
    // bump nextId_ past it if needed.
    EventId candidateId = static_cast<EventId>(scheduler.size()) + 1;
    Event newEvent{ candidateId, title, TimeRange{startMin, endMin}, {} };

    Status status = scheduler.addEvent(newEvent, /*allowOverlap=*/false);

    if (status == Status::CONFLICT) {
        // Print the conflicting events for context
        auto conflicts = scheduler.listConflicts(TimeRange{startMin, endMin});
        std::cout << "CONFLICT\n";
        std::cout << "  Requested: \"" << title << "\" ["
                  << formatWeekMinutes(startMin) << " → "
                  << formatWeekMinutes(endMin) << "]\n";
        for (const auto& c : conflicts) {
            std::cout << "  Conflicts with: \"" << c.title << "\" ["
                      << formatWeekMinutes(c.range.start) << " → "
                      << formatWeekMinutes(c.range.end) << "]\n";
        }
        return 1;
    }

    if (status == Status::INVALID_TIME_RANGE) {
        std::cerr << "Error: invalid time range (rejected by scheduler).\n";
        return 1;
    }

    if (status == Status::DUPLICATE_ID) {
        std::cerr << "Error: duplicate event id (internal error).\n";
        return 1;
    }

    if (!scheduler.saveToFile(filename)) {
        std::cerr << "Error: failed to write " << filename << "\n";
        return 1;
    }

    std::cout << "Added \"" << title << "\" ["
              << formatWeekMinutes(startMin) << " → "
              << formatWeekMinutes(endMin) << "]\n"
              << filename << " now contains " << scheduler.size() << " event(s).\n";
    return 0;
}

// ── Default mode: generate sample data.json ───────────────────────

static int runGenerate() {
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

    scheduler.addEvent({1, "CS Lecture",          TimeRange{540,  630},  {}});
    scheduler.addEvent({2, "Gym Session",          TimeRange{2280, 2400}, {}});
    scheduler.addEvent({3, "Team Meeting",         TimeRange{3480, 3540}, {}});
    scheduler.addEvent({4, "Dinner with Friends",  TimeRange{6840, 6960}, {}});
    scheduler.addEvent({5, "Weekend Run",          TimeRange{7860, 7980}, {}});

    const std::string filename = "data.json";
    if (!scheduler.saveToFile(filename)) {
        std::cerr << "Error: failed to write " << filename << "\n";
        return 1;
    }

    std::cout << "Successfully generated " << filename
              << " with " << scheduler.size() << " events.\n";
    return 0;
}

// ── --audit mode ──────────────────────────────────────────────────
//
// Schema for each event in the JSON file:
//   Required : id (int), title (string), startTime (int), endTime (int)
//   Optional : status ("pending" | "confirmed" | "conflict")
//              conflictWith : { id, title }   (set by this tool on conflict)
//
// Algorithm:
//   1. Load the full file into a json array.
//   2. Separate events into two groups:
//        • already-confirmed  → inserted into the RBT with allowOverlap=false
//          (they define the "ground truth" of the schedule)
//        • pending            → each is audited one by one, in order of startTime
//   3. For each pending event:
//        a. Check hasConflict() against the current RBT.
//        b. If no conflict → set status="confirmed", insert into RBT.
//        c. If conflict    → set status="conflict", add conflictWith from first
//           conflicting event; do NOT insert.
//   4. Overwrite the file with the updated array (pretty-printed, 2-space indent).

static int runAudit(int argc, char* argv[]) {
    const std::string filename = (argc >= 3) ? argv[2] : "data.json";

    // ── 1. Read raw JSON ─────────────────────────────────────────
    json arr;
    {
        std::ifstream ifs(filename);
        if (!ifs.is_open()) {
            std::cerr << "Error: cannot open \"" << filename << "\"\n";
            return 1;
        }
        try { ifs >> arr; }
        catch (const json::parse_error& ex) {
            std::cerr << "Error: JSON parse error in \"" << filename << "\": "
                      << ex.what() << "\n";
            return 1;
        }
        if (!arr.is_array()) {
            std::cerr << "Error: root of \"" << filename << "\" is not a JSON array.\n";
            return 1;
        }
    }

    // ── 2. Split confirmed vs pending (index into arr) ───────────
    Scheduler scheduler;

    // First pass: insert all confirmed events into the RBT.
    // Events with no "status" field are treated as already-confirmed
    // (they were written by saveToFile which doesn't set a status field).
    for (auto& ev : arr) {
        std::string status = ev.value("status", "confirmed");
        if (status == "pending" || status == "conflict") continue;

        EventId id       = ev.value("id",        EventId{0});
        std::string title = ev.value("title",    std::string{});
        int startTime    = ev.value("startTime", 0);
        int endTime      = ev.value("endTime",   0);

        if (id == 0 || startTime >= endTime) continue;   // skip malformed

        Event e{ id, title, TimeRange{startTime, endTime}, {} };
        // allowOverlap=true so pre-existing confirmed events don't block each
        // other (the original data was already validated by the engine).
        scheduler.addEvent(e, /*allowOverlap=*/true);

        // Ensure we record status in the JSON so the UI can read it back.
        ev["status"] = "success";
        ev.erase("conflictWith");
    }

    // ── 3. Audit pending events in startTime order ───────────────
    // Collect indices of pending entries, sort by startTime for deterministic
    // results when multiple pending events overlap each other.
    std::vector<std::size_t> pendingIdx;
    for (std::size_t i = 0; i < arr.size(); ++i) {
        std::string status = arr[i].value("status", "confirmed");
        if (status == "pending") pendingIdx.push_back(i);
    }
    std::sort(pendingIdx.begin(), pendingIdx.end(), [&](std::size_t a, std::size_t b) {
        return arr[a].value("startTime", 0) < arr[b].value("startTime", 0);
    });

    int confirmed_count = 0, conflict_count = 0;

    for (std::size_t idx : pendingIdx) {
        auto& ev        = arr[idx];
        EventId id       = ev.value("id",        EventId{0});
        std::string title = ev.value("title",    std::string{});
        int startTime    = ev.value("startTime", 0);
        int endTime      = ev.value("endTime",   0);

        if (id == 0 || startTime >= endTime) {
            ev["status"] = "conflict";
            ev["conflictWith"] = { {"id", 0}, {"title", "invalid time range"} };
            ++conflict_count;
            continue;
        }

        TimeRange range{ startTime, endTime };

        if (scheduler.hasConflict(range)) {
            // Find the first conflicting event for the UI label
            auto conflicting = scheduler.findAnyConflict(range);

            ev["status"] = "conflict";
            if (conflicting.has_value()) {
                ev["conflictWith"] = {
                    { "id",    conflicting->id    },
                    { "title", conflicting->title }
                };
            } else {
                ev["conflictWith"] = { {"id", 0}, {"title", "unknown"} };
            }
            ++conflict_count;
        } else {
            ev["status"] = "success";
            ev.erase("conflictWith");

            // Insert so subsequent pending events are checked against it too
            Event e{ id, title, range, {} };
            scheduler.addEvent(e, /*allowOverlap=*/false);
            ++confirmed_count;
        }
    }

    // ── 4. Write back ────────────────────────────────────────────
    {
        std::ofstream ofs(filename);
        if (!ofs.is_open()) {
            std::cerr << "Error: cannot write to \"" << filename << "\"\n";
            return 1;
        }
        ofs << arr.dump(2) << "\n";
    }

    std::cout << "Audit complete: "
              << confirmed_count << " success, "
              << conflict_count  << " conflict(s). "
              << "Written to \"" << filename << "\".\n";
    return 0;
}

// ── Entry point ───────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc == 1) {
        return runGenerate();
    }

    const std::string cmd = argv[1];

    if (cmd == "--add")   return runAdd(argc, argv);
    if (cmd == "--audit") return runAudit(argc, argv);

    std::cerr << "Error: unknown argument \"" << argv[1] << "\"\n\n";
    printUsage(argv[0]);
    return 1;
}

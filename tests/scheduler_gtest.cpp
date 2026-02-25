#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <set>
#include <utility>

#include <gtest/gtest.h>

// ---- Test-only access to Scheduler internals ----
// Scheduler.h includes standard headers; we include them above so the macro
// won't affect their contents (they're already guarded).
#define private public
#include "Scheduler.h"
#undef private

namespace {
using namespace rbt;

static Event MakeEvent(EventId id, int start, int end, std::string title = {}) {
    Event e;
    e.id = id;
    e.title = std::move(title);
    e.range = TimeRange{start, end};
    return e;
}

static Scheduler::Key KeyOf(const Scheduler::Node* n) {
    return Scheduler::Key{n->event.range.start, n->event.id};
}

struct RbtValidationResult {
    bool ok = true;
    int nodeCount = 0;
    int height = 0;
    int blackHeight = 0; // black-height from root to NIL (counting NIL as black=1)
    int subtreeMaxEnd = 0;
    std::string message;
};

static int Height(const Scheduler& s, const Scheduler::Node* n) {
    if (n == s.nil_) return 0;
    return 1 + std::max(Height(s, n->left), Height(s, n->right));
}

static bool IsNilOrHasParent(const Scheduler& s, const Scheduler::Node* child, const Scheduler::Node* parent) {
    if (child == s.nil_) return true;
    return child->parent == parent;
}

static std::pair<bool, int> ValidateBlackHeight(const Scheduler& s, const Scheduler::Node* n) {
    // Return {ok, blackHeight}. Treat NIL as black leaf with height 1.
    if (n == s.nil_) return {true, 1};
    auto [okL, bhL] = ValidateBlackHeight(s, n->left);
    auto [okR, bhR] = ValidateBlackHeight(s, n->right);
    if (!okL || !okR) return {false, 0};
    if (bhL != bhR) return {false, 0};
    int self = (n->color == Scheduler::Color::BLACK) ? 1 : 0;
    return {true, bhL + self};
}

static bool ValidateNoRedRed(const Scheduler& s, const Scheduler::Node* n) {
    if (n == s.nil_) return true;
    if (n->color == Scheduler::Color::RED) {
        if (n->left->color != Scheduler::Color::BLACK) return false;
        if (n->right->color != Scheduler::Color::BLACK) return false;
    }
    return ValidateNoRedRed(s, n->left) && ValidateNoRedRed(s, n->right);
}

static bool ValidateParentPointers(const Scheduler& s, const Scheduler::Node* n) {
    if (n == s.nil_) return true;
    if (!IsNilOrHasParent(s, n->left, n)) return false;
    if (!IsNilOrHasParent(s, n->right, n)) return false;
    return ValidateParentPointers(s, n->left) && ValidateParentPointers(s, n->right);
}

static int ComputeAndValidateMaxEnd(const Scheduler& s, const Scheduler::Node* n, bool* ok) {
    if (n == s.nil_) return 0;
    int leftMax = ComputeAndValidateMaxEnd(s, n->left, ok);
    int rightMax = ComputeAndValidateMaxEnd(s, n->right, ok);
    int computed = std::max({n->event.range.end, leftMax, rightMax});
    if (computed != n->maxEnd) *ok = false;
    return computed;
}

static void InorderKeys(const Scheduler& s, const Scheduler::Node* n, std::vector<Scheduler::Key>& out) {
    if (n == s.nil_) return;
    InorderKeys(s, n->left, out);
    out.push_back(KeyOf(n));
    InorderKeys(s, n->right, out);
}

static bool IsStrictlySortedByKey(const std::vector<Scheduler::Key>& keys) {
    for (size_t i = 1; i < keys.size(); i++) {
        const auto& prev = keys[i - 1];
        const auto& curr = keys[i];
        if (!Scheduler::keyLess(prev, curr)) return false;
    }
    return true;
}

static RbtValidationResult ValidateRbtAndAugmentation(const Scheduler& s) {
    RbtValidationResult r;

    if (s.nil_ == nullptr || s.root_ == nullptr) {
        r.ok = false;
        r.message = "nil_ or root_ is null";
        return r;
    }
    if (s.nil_->color != Scheduler::Color::BLACK) {
        r.ok = false;
        r.message = "NIL node must be BLACK";
        return r;
    }
    if (s.root_ != s.nil_ && s.root_->parent != s.nil_) {
        r.ok = false;
        r.message = "root_->parent must be nil_";
        return r;
    }
    if (s.root_ != s.nil_ && s.root_->color != Scheduler::Color::BLACK) {
        r.ok = false;
        r.message = "root must be BLACK";
        return r;
    }

    if (!ValidateNoRedRed(s, s.root_)) {
        r.ok = false;
        r.message = "red node has red child";
        return r;
    }

    if (!ValidateParentPointers(s, s.root_)) {
        r.ok = false;
        r.message = "parent pointers inconsistent";
        return r;
    }

    auto [okBH, bh] = ValidateBlackHeight(s, s.root_);
    if (!okBH) {
        r.ok = false;
        r.message = "black-height mismatch across paths";
        return r;
    }
    r.blackHeight = bh;

    bool okMax = true;
    r.subtreeMaxEnd = ComputeAndValidateMaxEnd(s, s.root_, &okMax);
    if (!okMax) {
        r.ok = false;
        r.message = "maxEnd augmentation incorrect for at least one node";
        return r;
    }

    std::vector<Scheduler::Key> keys;
    keys.reserve(s.index_.size());
    InorderKeys(s, s.root_, keys);
    if (keys.size() != s.index_.size()) {
        r.ok = false;
        r.message = "tree node count does not match index_ size";
        return r;
    }
    if (!IsStrictlySortedByKey(keys)) {
        r.ok = false;
        r.message = "inorder traversal is not strictly sorted by (start,id)";
        return r;
    }

    r.nodeCount = static_cast<int>(keys.size());
    r.height = Height(s, s.root_);
    return r;
}

static int HeightBoundForRbt(int n) {
    // RB tree height is <= 2 * log2(n+1). Add small slack for integer rounding.
    if (n <= 0) return 0;
    double bound = 2.0 * std::log2(static_cast<double>(n) + 1.0);
    return static_cast<int>(std::ceil(bound)) + 2;
}

class SchedulerFixture : public ::testing::Test {
protected:
    Scheduler s;

    void AssertValidRbt() const {
        const auto res = ValidateRbtAndAugmentation(s);
        ASSERT_TRUE(res.ok) << res.message;
    }
};

} // namespace

// ==========================
// Test cases (public API)
// ==========================

TEST_F(SchedulerFixture, EmptyScheduler_BasicBehavior) {
    EXPECT_EQ(s.size(), 0u);
    EXPECT_FALSE(s.hasConflict(TimeRange{0, 10}));
    EXPECT_FALSE(s.findAnyConflict(TimeRange{0, 10}).has_value());
    EXPECT_TRUE(s.queryByStartInRange(TimeRange{0, 10}).empty());
    EXPECT_TRUE(s.queryIntersecting(TimeRange{0, 10}).empty());

    auto slots = s.suggestSlots(TimeRange{0, 60}, /*durationMinutes=*/15, /*k=*/5);
    ASSERT_EQ(slots.size(), 1u);
    EXPECT_EQ(slots[0].start, 0);
    EXPECT_EQ(slots[0].end, 60);

    AssertValidRbt();
}

TEST_F(SchedulerFixture, AddEvent_InvalidTimeRange_Rejected) {
    EXPECT_EQ(s.addEvent(MakeEvent(1, 10, 10, "bad")), Status::INVALID_TIME_RANGE);
    EXPECT_EQ(s.addEvent(MakeEvent(1, 20, 10, "bad")), Status::INVALID_TIME_RANGE);
    EXPECT_EQ(s.size(), 0u);
    AssertValidRbt();
}

TEST_F(SchedulerFixture, AddEvent_DuplicateId_Rejected) {
    EXPECT_EQ(s.addEvent(MakeEvent(1, 10, 20, "a")), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(1, 30, 40, "dup")), Status::DUPLICATE_ID);
    EXPECT_EQ(s.size(), 1u);
    AssertValidRbt();
}

TEST_F(SchedulerFixture, HalfOpenIntervals_TouchingBoundaries_DoNotOverlap) {
    EXPECT_EQ(s.addEvent(MakeEvent(1, 10, 20, "a")), Status::OK);
    EXPECT_FALSE(s.hasConflict(TimeRange{20, 30}));
    EXPECT_EQ(s.addEvent(MakeEvent(2, 20, 30, "b")), Status::OK);

    EXPECT_TRUE(s.hasConflict(TimeRange{19, 20}));
    EXPECT_FALSE(s.hasConflict(TimeRange{30, 40}));
    AssertValidRbt();
}

TEST_F(SchedulerFixture, ConflictDetection_CompleteAndPartialOverlap) {
    EXPECT_EQ(s.addEvent(MakeEvent(1, 10, 20, "base")), Status::OK);

    // Complete overlap
    EXPECT_EQ(s.addEvent(MakeEvent(2, 12, 18, "inside")), Status::CONFLICT);
    EXPECT_TRUE(s.hasConflict(TimeRange{12, 18}));

    // Partial overlaps
    EXPECT_EQ(s.addEvent(MakeEvent(3, 15, 25, "right-overlap")), Status::CONFLICT);
    EXPECT_EQ(s.addEvent(MakeEvent(4, 0, 11, "left-overlap")), Status::CONFLICT);

    // Touching but not overlapping
    EXPECT_EQ(s.addEvent(MakeEvent(5, 20, 30, "touch")), Status::OK);
    EXPECT_FALSE(s.hasConflict(TimeRange{30, 35}));
    AssertValidRbt();
}

TEST_F(SchedulerFixture, IdenticalStartTime_DifferentIds_OrderAndStorage) {
    // Overlapping events with same startTime are only possible if allowOverlap=true.
    EXPECT_EQ(s.addEvent(MakeEvent(1, 100, 140, "e1"), /*allowOverlap=*/true), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(2, 100, 110, "e2"), /*allowOverlap=*/true), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(3, 100, 200, "e3"), /*allowOverlap=*/true), Status::OK);

    auto all = s.exportAllEvents();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].id, 1);
    EXPECT_EQ(all[1].id, 2);
    EXPECT_EQ(all[2].id, 3);

    // Removing one should keep others.
    EXPECT_EQ(s.removeEvent(2), Status::OK);
    EXPECT_FALSE(s.getEvent(2).has_value());
    EXPECT_TRUE(s.getEvent(1).has_value());
    EXPECT_TRUE(s.getEvent(3).has_value());
    AssertValidRbt();
}

TEST_F(SchedulerFixture, RemoveEvent_NotFoundAndThenOk) {
    EXPECT_EQ(s.removeEvent(123), Status::NOT_FOUND);
    EXPECT_EQ(s.addEvent(MakeEvent(1, 10, 20, "a")), Status::OK);
    EXPECT_EQ(s.removeEvent(1), Status::OK);
    EXPECT_EQ(s.removeEvent(1), Status::NOT_FOUND);
    EXPECT_EQ(s.size(), 0u);
    AssertValidRbt();
}

TEST_F(SchedulerFixture, RescheduleEvent_ConflictAndRollback) {
    EXPECT_EQ(s.addEvent(MakeEvent(1, 10, 20, "a")), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(2, 30, 40, "b")), Status::OK);

    // Conflicting reschedule should fail AND restore original.
    EXPECT_EQ(s.rescheduleEvent(1, TimeRange{15, 35}, /*allowOverlap=*/false), Status::CONFLICT);
    auto e1 = s.getEvent(1);
    ASSERT_TRUE(e1.has_value());
    EXPECT_EQ(e1->range.start, 10);
    EXPECT_EQ(e1->range.end, 20);

    // Non-conflicting reschedule should succeed.
    EXPECT_EQ(s.rescheduleEvent(1, TimeRange{20, 30}, /*allowOverlap=*/false), Status::OK);
    e1 = s.getEvent(1);
    ASSERT_TRUE(e1.has_value());
    EXPECT_EQ(e1->range.start, 20);
    EXPECT_EQ(e1->range.end, 30);

    AssertValidRbt();
}

TEST_F(SchedulerFixture, NextEvent_LowerBoundByStart) {
    EXPECT_EQ(s.addEvent(MakeEvent(1, 10, 20, "a")), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(2, 30, 40, "b")), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(3, 50, 60, "c")), Status::OK);

    auto n = s.nextEvent(0);
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(n->id, 1);

    n = s.nextEvent(25);
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(n->id, 2);

    n = s.nextEvent(60);
    EXPECT_FALSE(n.has_value());

    AssertValidRbt();
}

TEST_F(SchedulerFixture, RangeQuery_ByStartInRange) {
    EXPECT_EQ(s.addEvent(MakeEvent(1, 10, 20, "a")), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(2, 30, 40, "b")), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(3, 55, 65, "c")), Status::OK);

    auto r = s.queryByStartInRange(TimeRange{0, 25});
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].id, 1);

    r = s.queryByStartInRange(TimeRange{30, 56});
    ASSERT_EQ(r.size(), 2u);
    EXPECT_EQ(r[0].id, 2);
    EXPECT_EQ(r[1].id, 3);

    AssertValidRbt();
}

TEST_F(SchedulerFixture, RangeQuery_Intersecting_HalfOpenSemantics) {
    EXPECT_EQ(s.addEvent(MakeEvent(1, 10, 20, "a")), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(2, 30, 40, "b")), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(3, 55, 65, "c")), Status::OK);

    // Intersecting window hits b and c (b intersects at [35,40), c at [55,65))
    auto r = s.queryIntersecting(TimeRange{35, 67});
    std::set<EventId> ids;
    for (const auto& e : r) ids.insert(e.id);
    EXPECT_EQ(ids, (std::set<EventId>{2, 3}));

    // Touching boundary: [20,30) touches a at 20 and b at 30 -> no intersection
    r = s.queryIntersecting(TimeRange{20, 30});
    EXPECT_TRUE(r.empty());

    AssertValidRbt();
}

TEST_F(SchedulerFixture, SuggestSlots_FindsGapsAndRespectsK) {
    // Busy:
    // A: 10-30, B: 40-50, C: 55-70
    EXPECT_EQ(s.addEvent(MakeEvent(1, 10, 30, "A")), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(2, 40, 50, "B")), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(3, 55, 70, "C")), Status::OK);

    auto slots = s.suggestSlots(TimeRange{0, 80}, /*durationMinutes=*/5, /*k=*/3);
    ASSERT_EQ(slots.size(), 3u);
    EXPECT_EQ(slots[0].start, 0);  EXPECT_EQ(slots[0].end, 10);
    EXPECT_EQ(slots[1].start, 30); EXPECT_EQ(slots[1].end, 40);
    EXPECT_EQ(slots[2].start, 50); EXPECT_EQ(slots[2].end, 55);

    auto longSlots = s.suggestSlots(TimeRange{0, 80}, /*durationMinutes=*/15, /*k=*/10);
    EXPECT_TRUE(longSlots.empty());

    auto exact = s.suggestSlots(TimeRange{50, 55}, /*durationMinutes=*/5, /*k=*/1);
    ASSERT_EQ(exact.size(), 1u);
    EXPECT_EQ(exact[0].start, 50);
    EXPECT_EQ(exact[0].end, 55);

    auto noneInsideBusy = s.suggestSlots(TimeRange{12, 29}, /*durationMinutes=*/5, /*k=*/2);
    EXPECT_TRUE(noneInsideBusy.empty());

    AssertValidRbt();
}

TEST_F(SchedulerFixture, DuplicateEvent_ShiftAndConflictAwareness) {
    EXPECT_EQ(s.addEvent(MakeEvent(10, 100, 120, "base")), Status::OK);

    auto r = s.duplicateEvent(10, /*shiftMinutes=*/30, /*allowOverlap=*/false);
    EXPECT_EQ(r.status, Status::OK);
    ASSERT_NE(r.value, 0);
    ASSERT_NE(r.value, 10);

    auto dup = s.getEvent(r.value);
    ASSERT_TRUE(dup.has_value());
    EXPECT_EQ(dup->range.start, 130);
    EXPECT_EQ(dup->range.end, 150);
    EXPECT_EQ(dup->title, "base");

    // Duplicate that would overlap (shift 10 => [110,130) overlaps [100,120))
    auto r2 = s.duplicateEvent(10, /*shiftMinutes=*/10, /*allowOverlap=*/false);
    EXPECT_EQ(r2.status, Status::CONFLICT);
    AssertValidRbt();
}

TEST_F(SchedulerFixture, WeekBoundaryEvents_Start0_And_LastMinuteSlot) {
    EXPECT_EQ(s.addEvent(MakeEvent(1, 0, 1, "start")), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(2, MINUTES_PER_WEEK - 1, MINUTES_PER_WEEK, "end")), Status::OK);

    EXPECT_TRUE(s.hasConflict(TimeRange{0, 1}));
    EXPECT_FALSE(s.hasConflict(TimeRange{1, 2}));

    EXPECT_TRUE(s.hasConflict(TimeRange{MINUTES_PER_WEEK - 1, MINUTES_PER_WEEK}));
    EXPECT_FALSE(s.hasConflict(TimeRange{MINUTES_PER_WEEK - 2, MINUTES_PER_WEEK - 1}));

    AssertValidRbt();
}

TEST_F(SchedulerFixture, ExportToJson_InOrderArrayWithBasicFields) {
    EXPECT_EQ(s.addEvent(MakeEvent(1, 10, 20, "A")), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(2, 30, 40, "B \"quoted\"")), Status::OK);

    std::string json = s.exportToJson();
    // Basic structure
    ASSERT_FALSE(json.empty());
    EXPECT_EQ(json.front(), '[');
    EXPECT_EQ(json.back(), ']');
    // Contains event ids and title content.
    EXPECT_NE(json.find("\"id\":1"), std::string::npos);
    EXPECT_NE(json.find("\"id\":2"), std::string::npos);
    EXPECT_NE(json.find("B \"quoted\""), std::string::npos);
    // At least one color field is present.
    EXPECT_NE(json.find("\"color\":\""), std::string::npos);
}

TEST_F(SchedulerFixture, SaveAndLoadFromJsonFile_RoundTripsEvents) {
    EXPECT_EQ(s.addEvent(MakeEvent(1, 10, 20, "A")), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(2, 30, 40, "B")), Status::OK);

    std::string filename = "test_schedule.json";
    ASSERT_TRUE(s.saveToFile(filename));

    Scheduler loaded;
    ASSERT_TRUE(loaded.loadFromFile(filename));

    auto original = s.exportAllEvents();
    auto restored = loaded.exportAllEvents();
    ASSERT_EQ(original.size(), restored.size());
    for (std::size_t i = 0; i < original.size(); ++i) {
        EXPECT_EQ(original[i].id, restored[i].id);
        EXPECT_EQ(original[i].title, restored[i].title);
        EXPECT_EQ(original[i].range.start, restored[i].range.start);
        EXPECT_EQ(original[i].range.end, restored[i].range.end);
    }
}

// ==========================
// Test cases (RB + maxEnd)
// ==========================

TEST_F(SchedulerFixture, RbtInvariant_MaxEndCorrect_AfterInsertionsAndDeletions) {
    // Insert overlapping allowed to stress augmentation.
    EXPECT_EQ(s.addEvent(MakeEvent(1, 50, 60, "a"), true), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(2, 10, 100, "b"), true), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(3, 30, 40, "c"), true), Status::OK);
    EXPECT_EQ(s.addEvent(MakeEvent(4, 70, 80, "d"), true), Status::OK);
    AssertValidRbt();

    // Root maxEnd should be the maximum end among all events.
    const auto res1 = ValidateRbtAndAugmentation(s);
    ASSERT_TRUE(res1.ok);
    EXPECT_EQ(res1.subtreeMaxEnd, 100);

    // Delete the event with the maximum end and ensure maxEnd decreases accordingly.
    EXPECT_EQ(s.removeEvent(2), Status::OK);
    AssertValidRbt();
    const auto res2 = ValidateRbtAndAugmentation(s);
    ASSERT_TRUE(res2.ok);
    EXPECT_EQ(res2.subtreeMaxEnd, 80);
}

TEST_F(SchedulerFixture, RbtHeight_IsLogarithmicBound_UnderManyInsertions) {
    // Non-overlapping events to keep addEvent's conflict checks enabled.
    // Insert them in randomized order to avoid any accidental best-case structure.
    const int n = 2000;
    std::vector<int> starts;
    starts.reserve(n);
    for (int i = 0; i < n; i++) starts.push_back(i * 2);

    std::mt19937 rng(12345);
    std::shuffle(starts.begin(), starts.end(), rng);

    for (int i = 0; i < n; i++) {
        const int st = starts[i];
        const int en = st + 1;
        ASSERT_EQ(s.addEvent(MakeEvent(static_cast<EventId>(i + 1), st, en, "e")), Status::OK);
    }

    const auto res = ValidateRbtAndAugmentation(s);
    ASSERT_TRUE(res.ok) << res.message;
    EXPECT_EQ(res.nodeCount, n);
    EXPECT_LE(res.height, HeightBoundForRbt(n)) << "height=" << res.height << " n=" << n;
}

TEST_F(SchedulerFixture, RbtInvariant_HoldsDuringRandomInsertDeleteSequence) {
    std::mt19937 rng(777);
    std::uniform_int_distribution<int> lenDist(1, 120);
    std::uniform_int_distribution<int> startDist(0, MINUTES_PER_WEEK - 1);

    std::vector<EventId> liveIds;
    liveIds.reserve(500);

    EventId nextId = 1;
    for (int i = 0; i < 400; i++) {
        int start = startDist(rng);
        int len = lenDist(rng);
        int end = std::min(start + len, MINUTES_PER_WEEK);
        if (start >= end) continue;

        ASSERT_EQ(s.addEvent(MakeEvent(nextId, start, end, "rnd"), /*allowOverlap=*/true), Status::OK);
        liveIds.push_back(nextId);
        nextId++;

        // Occasionally delete a random live event.
        if (!liveIds.empty() && (i % 7 == 0)) {
            std::uniform_int_distribution<size_t> pick(0, liveIds.size() - 1);
            size_t idx = pick(rng);
            EventId delId = liveIds[idx];
            ASSERT_EQ(s.removeEvent(delId), Status::OK);
            liveIds.erase(liveIds.begin() + static_cast<std::ptrdiff_t>(idx));
        }

        AssertValidRbt();
    }

    // Final validation and a height sanity check.
    const auto res = ValidateRbtAndAugmentation(s);
    ASSERT_TRUE(res.ok) << res.message;
    EXPECT_LE(res.height, HeightBoundForRbt(res.nodeCount));
}


// TODO-heavy skeleton (core-only). Fill in RB-tree + interval logic step by step.

#include "Scheduler.h"

#include <algorithm>  // std::max
#include <utility>    // std::move

namespace rbt {

// ------------------------------
// Small local helpers (optional)
// ------------------------------
static inline int max3(int a, int b, int c) {
    return std::max(a, std::max(b, c));
}

// ------------------------------
// Scheduler: public API
// ------------------------------

/**
 * Initialize an empty scheduler.
 * Allocates the sentinel NIL node and sets up the initial tree state.
*/
Scheduler::Scheduler() : nil_(nullptr), root_(nullptr) {
    nil_ = new Node(Event{0, {0, 0}, ""}); // dummy event for nil
    nil_->color = Node::Color::BLACK;
    nil_->left = nil_;
    nil_->right = nil_;
    nil_->parent = nil_;
    nil_->maxEnd = 0;

    root_ = nil_;
}

Scheduler::~Scheduler() {
    // TODO: free all nodes in the RB-tree, then free nil_
    // Important: avoid recursion if you prefer. Use an explicit stack/vector.
    //
    // Also clear index_.
}

Status Scheduler::addEvent(const Event& e, bool allowOverlap) {
    // TODO:
    // 1) validate e.range.isValid()
    // 2) if id already exists -> DUPLICATE_ID
    // 3) if !allowOverlap and hasConflict(e.range) -> CONFLICT
    // 4) create Node* z = new Node(e), set children/parent to nil_
    // 5) RB-tree insert by Key(start,id)
    // 6) update maxEnd up the path + during rotations
    // 7) add to index_
    (void)e;
    (void)allowOverlap;
    return Status::OK;
}

Status Scheduler::removeEvent(EventId id) {
    // TODO:
    // 1) find key via index_ (if not found -> NOT_FOUND)
    // 2) find Node* z in tree
    // 3) RB-tree delete z (standard RB delete + fixup)
    // 4) maintain maxEnd updates after structural changes
    // 5) erase from index_
    (void)id;
    return Status::NOT_FOUND;
}

Status Scheduler::rescheduleEvent(EventId id, const TimeRange& newRange, bool allowOverlap) {
    // TODO:
    // 1) validate newRange
    // 2) find existing event by id
    // 3) if !allowOverlap:
    //      - temporarily ignore the event itself
    //      - check conflicts for newRange
    // 4) easiest: remove(id) then add(updatedEvent)
    //    (make sure to preserve title/meta)
    (void)id;
    (void)newRange;
    (void)allowOverlap;
    return Status::NOT_FOUND;
}

std::optional<Event> Scheduler::getEvent(EventId id) const {
    // TODO: use index_ -> find Node -> return node->event
    (void)id;
    return std::nullopt;
}

std::vector<Event> Scheduler::queryByStartInRange(const TimeRange& range) const {
    // TODO:
    // 1) validate range
    // 2) start from lowerBoundByStart(range.start)
    // 3) iterate successor until event.start >= range.end
    (void)range;
    return {};
}

std::vector<Event> Scheduler::queryIntersecting(const TimeRange& range) const {
    // TODO: interval query using maxEnd augmentation
    // collectIntersecting(root_, range, out)
    (void)range;
    return {};
}

bool Scheduler::hasConflict(const TimeRange& range) const {
    // TODO:
    // Use interval tree logic with maxEnd:
    // Walk down from root:
    //   - if current overlaps -> true
    //   - if left subtree exists and left->maxEnd > range.start -> go left
    //   - else go right
    (void)range;
    return false;
}

std::optional<Event> Scheduler::findAnyConflict(const TimeRange& range) const {
    // TODO: similar to hasConflict, but return the conflicting Event
    (void)range;
    return std::nullopt;
}

std::vector<Event> Scheduler::listConflicts(const TimeRange& range) const {
    // TODO: collect all intersecting events (could reuse queryIntersecting)
    (void)range;
    return {};
}

std::optional<Event> Scheduler::nextEvent(int t) const {
    // TODO: use lowerBoundByStart(t), return that node’s event if exists
    (void)t;
    return std::nullopt;
}

Result<EventId> Scheduler::duplicateEvent(EventId id, int shiftMinutes, bool allowOverlap) {
    // TODO:
    // 1) fetch event by id
    // 2) create new event with new id (you decide policy: e.g. max+1)
    // 3) shift range by shiftMinutes
    // 4) addEvent(newEvent)
    Result<EventId> r;
    r.status = Status::NOT_FOUND;
    r.value = 0;
    (void)shiftMinutes;
    (void)allowOverlap;
    return r;
}

std::vector<TimeRange> Scheduler::suggestSlots(const TimeRange& window, int durationMinutes, int k) const {
    // TODO (simple approach):
    // 1) validate window + duration
    // 2) cursor = window.start
    // 3) iterate events by start time within window:
    //    - if cursor + duration <= event.start -> record [cursor, cursor+duration]
    //    - cursor = max(cursor, event.end)
    // 4) after loop: if cursor + duration <= window.end -> record
    // Stop when collected k slots.
    (void)window;
    (void)durationMinutes;
    (void)k;
    return {};
}

Week Scheduler::toWeekView() const {
    // TODO:
    // 1) init Week with dayIndex 0..6
    // 2) iterate all events in order, map start day to days[day].eventIds
    Week w;
    for (int i = 0; i < DAYS_IN_WEEK; i++) {
        w.days[i].dayIndex = i;
    }
    return w;
}

void Scheduler::clear() {
    // TODO: delete all nodes, reset root_ = nil_, clear index_
}

std::size_t Scheduler::size() const {
    return index_.size();
}

std::vector<Event> Scheduler::exportAllEvents() const {
    // TODO: in-order traversal, push all events
    return {};
}

// ------------------------------
// RB-tree internals
// ------------------------------

/**
 * Compare two keys for ordering
 * 
 * @return true if a < b, false otherwise.
 */
bool Scheduler::keyLess(const Key& a, const Key& b) {
    if (a.start != b.start) return a.start < b.start;
    return a.id < b.id; // tie-breaker
}

/**
 * Rotate node x to the left.
 */
void Scheduler::rotateLeft(Node* node) {
    // TODO: standard RB rotate left
    Node* b = node->right;
    node->right = b->left;

    if (b->left != nil_) {
        b->left->parent = node;
    }

    b->parent = node->parent;

    Node* p = node->parent;

    if (p == nil_) {
        root_ = b;
    } else if (node == node->parent->left) {
        p->left = b;
    } else {
        p->right = b;
    }

    b->left = node;
    node->parent = b;

    // Also: update maxEnd for affected nodes (x and its new parent)
    updateNode(node);
    updateNode(b);
    updateUpwards(b->parent);
}

/**
 * Rotate node y to the right.
 */
void Scheduler::rotateRight(Node* node) {
    // TODO: standard RB rotate right
    Node* a = node->left;
    node->left = a->right;

    if (a->right != nil_) {
        a->right->parent = node;
    }

    a->parent = node->parent;

    Node* p = node->parent;

    if (p == nil_) {
        root_ = a;
    } else if (p->left == node) {
        p->left = a;
    } else {
        p->right = a;
    }

    a->right = node;
    node->parent = a;
    // Also: update maxEnd for affected nodes
    updateNode(node);
    updateNode(a);
    updateUpwards(a->parent);
}

/**
 * Fix the red-black tree properties after direct insertion (red node).
 */
void Scheduler::insertFixup(Node* z) {
    // TODO: standard RB insert fixup
    (void)z;
}

/**
 * Fix the red-black tree properties after deletion (potentially double-black).
 */
void Scheduler::deleteFixup(Node* x) {
    // TODO: standard RB delete fixup
    (void)x;
}

/**
 * Insert a new node into the tree. (call insertFixup in process/ after)
 */
Scheduler::Node* Scheduler::treeInsert(Node* z) {
    // TODO:
    // 1) BST insert by Key(z->event.range.start, z->event.id)
    // 2) set z->left/right = nil_
    // 3) set z->color = RED
    // 4) updateUpwards(z) as needed
    (void)z;
    return nullptr;
}

/**
 * Delete a node from the tree. (call deleteFixup in process/ after)
 */
void Scheduler::treeDelete(Node* z) {
    // TODO: standard RB delete (transplant, track original color, fixup)
    (void)z;
}

/*
 * Find the minimum node in a subtree.
 */
Scheduler::Node* Scheduler::minimum(Node* x) const {
    // TODO: walk left until nil_
    (void)x;
    return nullptr;
}

/**
 * Find the successor of a node.
 */
Scheduler::Node* Scheduler::successor(Node* x) const {
    // TODO:
    // if right subtree exists -> minimum(right)
    // else go up until you come from left
    (void)x;
    return nullptr;
}

/*
 * Find a node by its key.
 */
Scheduler::Node* Scheduler::findNodeByKey(const Key& key) const {
    // TODO: BST search by key
    (void)key;
    return nullptr;
}

/*
 * Find the smallest node with key.start >= start.
 */
Scheduler::Node* Scheduler::lowerBoundByStart(int start) const {
    // TODO: find smallest node with key.start >= start
    (void)start;
    return nullptr;
}

/*
 * Update the maxEnd value of a node based on its event and children.
 */
void Scheduler::updateNode(Node* node) {
    node->maxEnd = max(node->event.range.end, node->left->maxEnd, node->right->maxEnd);
}

/*
 * Update maxEnd values up the path from node to root.
 */
void Scheduler::updateUpwards(Node* node) {
    if(node == nil_) return; // base case

    while (node != nil_) {
        updateNode(node);
        node = node->parent;
    }
}

/*
 * Check if two time ranges overlap.
 */
bool Scheduler::overlaps(const TimeRange& a, const TimeRange& b) const {
    // half-open overlap check
    return a.start < b.end && b.start < a.end;
}

/*
 * Collect all events in the subtree rooted at x that intersect with the given range.
 */
void Scheduler::collectIntersecting(Node* x, const TimeRange& range, std::vector<Event>& out) const {
    // TODO:
    // Use maxEnd pruning:
    // - If x == nil_ return
    // - If left subtree exists and left->maxEnd > range.start -> search left
    // - If x overlaps -> add
    // - If x->event.range.start < range.end -> search right
    //
    // If you want to avoid recursion, rewrite using an explicit stack.
    (void)x;
    (void)range;
    (void)out;
}

} // namespace rbt
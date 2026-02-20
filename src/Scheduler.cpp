// TODO-heavy skeleton (core-only). Fill in RB-tree + interval logic step by step.

#include "Scheduler.h"

#include <algorithm>  // std::max
#include <utility>    // std::move
#include <iostream>

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
    nil_ = new Node(Event{}); // dummy event for nil
    nil_->color = Color::BLACK;
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
    if (e.range.start >= e.range.end) return Status::INVALID_TIME_RANGE;

    // 2) if id already exists -> DUPLICATE_ID
    if (index_.count(e.id) > 0) return Status::DUPLICATE_ID;

    // 3) if !allowOverlap and hasConflict(e.range) -> CONFLICT
    if (!allowOverlap && hasConflict(e.range)) return Status::CONFLICT;

    // 4) create Node* z = new Node(e), set children/parent to nil_
        Node* node = new Node(e);
        node->left = nil_;
        node->right = nil_;
        node->parent = nil_;

    // 5) RB-tree insert by Key(start,id)
    Node* insertedNode = treeInsert(node);

    // 6) update maxEnd up the path + during rotations
    updateMaxEnd(insertedNode);

    // 7) add to index_
    index_[e.id] = insertedNode;

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

/**
 * Check if any event overlaps with the given range.
 *
 * @return true if there is a conflict, false otherwise.
 */
bool Scheduler::hasConflict(const TimeRange& range) const {
    return findAnyConflict(range).has_value();
}

/**
 * Find any single event that conflicts with the given range.
 *
 * @return an optional Event that overlaps with the range, or std::nullopt if no conflict.
 */
std::optional<Event> Scheduler::findAnyConflict(const TimeRange& range) const {
    Node* curr = root_;

    while (curr != nil_) {
        if (overlaps(curr->event.range, range)) {
            return curr->event; // found a conflict
        }

        if (curr->left != nil_ && curr->left->maxEnd > range.start) {
            curr = curr->left; // potential conflicts in left subtree
        } else {
            curr = curr->right; // go right
        }
    }

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
    (void) id;
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
void Scheduler::insertFixup(Node* node) {
    // TODO: standard RB insert fixup
    while (node->parent->color == Color::RED) {
        Node* p = node->parent; // parent
        Node* g = p->parent;    // grandparent

        if (p == g->left) {
            Node* u = g->right; // uncle

            if (u->color == Color::RED) {
                // Case 1: Uncle is red -> recolor
                p->color = Color::BLACK;
                u->color = Color::BLACK;
                g->color = Color::RED;
                node = g; // move up to fix potential violations at grandparent
            } else {
                // Case 2 & 3: Uncle is black
                if (node == p->right) {
                    // Case 2: node is right child -> rotate left
                    rotateLeft(p);
                    node = p;
                    p = node->parent; // update parent after rotation
                    g = p->parent;    // update grandparent after rotation
                }
                // Case 3: node is left child -> rotate right
                p->color = Color::BLACK;
                g->color = Color::RED;
                rotateRight(g);
            }
        } else {
            // Symmetric cases for when parent is right child
            Node* u = g->left; // uncle

            if (u->color == Color::RED) {
                // Case 1: Uncle is red -> recolor
                p->color = Color::BLACK;
                u->color = Color::BLACK;
                g->color = Color::RED;
                node = g; // move up to fix potential violations at grandparent
            } else {
                // Case 2 & 3: Uncle is black
                if (node == p->left) {
                    // Case 2: node is left child -> rotate right
                    rotateRight(p);
                    node = p;
                    p = node->parent; // update parent after rotation
                    g = p->parent;    // update grandparent after rotation
                }
                // Case 3: node is right child -> rotate left
                p->color = Color::BLACK;
                g->color = Color::RED;
                rotateLeft(g);
            }
        }
    }

    root_->color = Color::BLACK; // Ensure root is always black
}

/**
 * Replace subtree rooted at u with subtree rooted at v.
 * Used in delete operation.
 */
void Scheduler::transplant(Node* u, Node* v) {
    if (u->parent == nil_) {
        root_ = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

/**
 * Fix the red-black tree properties after deletion (potentially double-black).
 */
void Scheduler::deleteFixup(Node* node) {
    while (node != root_ && node->color == Color::BLACK) {

        if (node == node->parent->left) {
            Node* sibling = node->parent->right;

            // Case 1: sibling is red -> recolor and rotate
            if (sibling->color == Color::RED) {
                sibling->color = Color::BLACK;
                node->parent->color = Color::RED;
                rotateLeft(node->parent);
                sibling = node->parent->right;
            }

            // Case 2: sibling is black and both children are black -> recolor sibling and move up
            if (sibling->left->color == Color::BLACK && sibling->right->color == Color::BLACK) {
                sibling->color = Color::RED;
                node = node->parent;

            } else {

                // Case 3: sibling is black and sibling's right child is black -> recolor and rotate sibling
                if (sibling->right->color == Color::BLACK) {
                    sibling->left->color = Color::BLACK;
                    sibling->color = Color::RED;
                    rotateRight(sibling);
                    sibling = node->parent->right;
                }

                sibling->color = node->parent->color;
                node->parent->color = Color::BLACK;
                sibling->right->color = Color::BLACK;
                rotateLeft(node->parent);
                node = root_;
            }

        } else { // mirror
            Node* sibling = node->parent->left;

            if (sibling->color == Color::RED) {
                sibling->color = Color::BLACK;
                node->parent->color = Color::RED;
                rotateRight(node->parent);
                sibling = node->parent->left;
            }

            if (sibling->right->color == Color::BLACK && sibling->left->color == Color::BLACK) {
                sibling->color = Color::RED;
                node = node->parent;

            } else {

                if (sibling->left->color == Color::BLACK) {
                    sibling->right->color = Color::BLACK;
                    sibling->color = Color::RED;
                    rotateLeft(sibling);
                    sibling = node->parent->left;
                }

                sibling->color = node->parent->color;
                node->parent->color = Color::BLACK;
                sibling->left->color = Color::BLACK;
                rotateRight(node->parent);
                node = root_;
            }
        }
    }
    node->color = Color::BLACK;
}

/**
 * Insert a new node into the tree. (call insertFixup in process/ after)
 */
Scheduler::Node* Scheduler::treeInsert(Node* node) {

    Key key{node->event.range.start, node->event.id};

    // initialize new node
    node->left = nil_;
    node->right = nil_;
    node->color = Color::RED;

    node->maxEnd = node->event.range.end;

    Node* p = nil_;
    Node* curr = root_;

    while (curr != nil_) {
        p = curr;
        if (keyLess(key, Key{curr->event.range.start, curr->event.id})) {
            curr = curr->left;
        } else {
            curr = curr->right;
        }
    }

    node->parent = p;

    if (curr == nil_) {
        root_ = node;
    } else if (keyLess(key, Key{p->event.range.start, p->event.id})) {
        p->left = node;
    } else {
        p->right = node;
    }
    

    updateUpwards(node);

    insertFixup(node);
    
    return node;
}

/**
 * Delete a node from the tree. (call deleteFixup in process/ after)
 */
void Scheduler::treeDelete(Node* node) {
    Node* temp = node;
    Color originalColor = temp->color;

    Node* x = nil_; // moves to temp's original position
    Node* updateStart = nil_; // node to start maxEnd updates from

    if (node->left == nil_) {
        x = node->right;
        transplant(node, node->right);
        updateStart = node->parent;
    } else if (node->right == nil_) {
        x = node->left;
        transplant(node, node->left);
        updateStart = node->parent;
    } else {
        temp = minimum(node->right); // successor
        originalColor = temp->color;
        x = temp->right;

        if (temp->parent == node) {
            x->parent = temp; // important for fixup
            updateStart = temp; // maxEnd updates start from successor's position
        } else {
            transplant(temp, temp->right);
            temp->right = node->right;
            temp->right->parent = temp;
            updateStart = temp->parent; // maxEnd updates start from successor's original parent
        }

        transplant(node, temp);
        temp->left = node->left;
        temp->left->parent = temp;
        temp->color = node->color;

        updateStart = temp; // maxEnd updates start from successor's new position
    }

    if (updateStart != nil_) {
        updateUpwards(updateStart);
    } else {
        // If updateStart is nil_, we need to update from x's parent (which could be nil_)
        updateUpwards(x->parent);
    }

    if (originalColor == Color::BLACK) {
        deleteFixup(x);
    }

    if (x != nil_) {
        updateUpwards(x->parent);
    } else {
        updateUpwards(root_);
    }
}

/*
 * Find the minimum node in a subtree.
 */
Scheduler::Node* Scheduler::minimum(Node* node) const {
    while (node->left != nil_) {
        node = node->left;
    }

    return node;
}

/**
 * Find the successor of a node.
 */
Scheduler::Node* Scheduler::successor(Node* node) const {
    // right subtree exists
    if (node->right != nil_) {
        return minimum(node->right);
    }

    Node* p = node->parent;

    // walk up until find a node that is a left child of its parent
    while (p != nil_ && node == p->right) {
        node = p;
        p = p->parent;
    }
    return p;
}

/*
 * Find a node by its key.
 */
Scheduler::Node* Scheduler::findNodeByKey(const Key& key) const {
    Node* curr = root_;

    while (curr != nil_) {
        Key currKey{curr->event.range.start, curr->event.id};

        if(keyLess(key, currKey)) {
            curr = curr->left;
        } else if (keyLess(currKey, key)) {
            curr = curr->right;
        } else {
            return curr; // found
        }
    }
    return nullptr; // not found
}

/*
 * Find the smallest node with key.start >= start.
 */
Scheduler::Node* Scheduler::lowerBoundByStart(int start) const {
    Node* curr = root_;
    Node* candidate = nil_;

    while (curr != nil_) {

        if (curr->event.range.start >= start) {
            candidate = curr; // potential lower bound
            curr = curr->left; // try to find smaller start
        } else {
            curr = curr->right; // need larger start
        }
    }
    return (candidate == nil_) ? nullptr : candidate;
}

/*
 * Update the maxEnd value of a node based on its event and children.
 */
void Scheduler::updateNode(Node* node) {
    node->maxEnd = max3(node->event.range.end, node->left->maxEnd, node->right->maxEnd);
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
    if (x == nil_) return;

    if (x->left != nil_ && x->left->maxEnd > range.start) {
        collectIntersecting(x->left, range, out);
    }

    if (overlaps(x->event.range, range)) {
        out.push_back(x->event);
    }

    if (x->event.range.start < range.end) {
        collectIntersecting(x->right, range, out);
    }
}

void Scheduler::debugInsert(const Event& e) {
    cerr << "    debugInsert id = " << e.id << "\n";
    Node* node = new Node(e);

    cerr << "    before treeInsert" << "\n";
    treeInsert(node);
    cerr << "    after treeInsert" << "\n";

    cerr << "    before insertFixup" << "\n";
    insertFixup(node);
    cerr << "    after insertFixup" << "\n";
}

} // namespace rbt
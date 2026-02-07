#include "scheduler.h"

#include <algorithm>
#include <cstddef>
using namespace std;

namespace rbt {

/* ========= Scheduler::Impl =========
 * 真正的数据结构都藏在这里
 * 你之后可以把这里从 vector 换成红黑树
 */
struct Scheduler::Impl {

  // TODO: 选择底层存储结构
  // 方案 A（先跑通）：std::vector<Event>
  // 方案 B（目标）：红黑树，key = DateTime / EventId
  //
  std::vector<Event> events;
  
  EventId nextId{1};

  // TODO: 红黑树根节点指针（如果你之后实现）
  // Node* root{nullptr};
};

/* ========= constructor ========= */

Scheduler::Scheduler()
  : impl_(new Impl()) {
  // TODO: 初始化需要的状态
  Scheduler s;
  EventId id = s.addEvent(e);
  auto ev = s.getEvent(id);
}

Scheduler::~Scheduler() {
  // TODO: 释放红黑树 / 资源
  delete impl_;
}

/* ========= basic CRUD ========= */

EventId Scheduler::addEvent(const Event& e) {
  // validate time span
  if(e.when.end <= e.when.start) {
    return 0; // invalid time span, return 0 as error code (or throw exception)
  }

  // check conflict
  if(hasConflict(e.when)) {
    return 0;
  }
  
  // generate new id
  EventId newId = impl_->nextId;
  impl_->nextId++;

  //copy even and assign id
  EventId copy = e;
  copy.id = newId;

  // insert
  impl_->events.push_back(copy);

  // sort by start time
  sort(impl_->events.begin(), impl_->events.end(), [](const Event& a, const Event& b) {
    return a.when.start < b.when.start;
  });

  return newId; // placeholder
}

bool Scheduler::removeEvent(EventId id) {
  // TODO:
  // 1. Find id in underlying structure
  // 2. Delete
  // 3. Return whether successful

  // validate time span


  return false;
}

bool Scheduler::rescheduleEvent(EventId id, const TimeSpan& newWhen) {
  // TODO:
  // 1. Find event
  // 2. Modify TimeSpan
  // 3. If key is time, need to delete then insert
  // 4. Return whether successful

  return false;
}

std::optional<Event> Scheduler::getEvent(EventId id) const {
  // TODO:
  // 1. Find event
  // 2. If found, return Event
  // 3. Otherwise return std::nullopt

  return std::nullopt;
}

/* ========= search ========= */

std::vector<Event>
Scheduler::queryRange(const DateTime& from, const DateTime& to) const {
  // TODO:
  // 1. Iterate/search all events that may fall within the range
  // 2. Check if they intersect with [from, to)
  // 3. Collect and return

  return {};
}

std::vector<Event>
Scheduler::queryDay(const Date& d) const {
  // TODO:
  // 1. Construct the day's [00:00, 24:00) DateTime
  // 2. Call queryRange

  return {};
}

std::vector<Event>
Scheduler::searchText(const std::string& keyword) const {
  // TODO:
  // 1. Iterate all events
  // 2. Check keyword in title / location / notes
  // 3. Return matching results

  return {};
}

/* ========= extra ========= */

std::optional<EventId>
Scheduler::duplicateEvent(EventId id, const DateTime& newStart) {
  // TODO:
  // 1. Find event by id
  // 2. Calculate duration
  // 3. Construct new TimeSpan
  // 4. addEvent
  // 5. Return new id

  return std::nullopt;
}

// check if a candidate time span conflicts with existing events
bool Scheduler::hasConflict(const TimeSpan& candidate) const {
  // validate candidate itself
  if(candidate.end <= candidate.start) {
    return true; 
  }

  // loop though events and check for overlap
  for(const auto& e: impl_->events) {
    if(!(candidate.end <= e.start || candidate.start >= e.end)) {
      return true; // found overlapping event
    }
  }

  return false;
}

} // namespace rbt

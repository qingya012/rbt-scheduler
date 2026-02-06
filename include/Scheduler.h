#ifndef RBT_SCHEDULER_H
#define RBT_SCHEDULER_H

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace rbt {

/* ========= basic types ========= */

struct Date {
  int year{1970};
  int month{1}; // 1-12
  int day{1};   // 1-31

  // basic comparison operators for sorting/searching
  friend bool operator==(const Date& a, const Date& b) {
    return a.year == b.year && a.month == b.month && a.day == b.day;
  }
  friend bool operator<(const Date& a, const Date& b) {
    if (a.year != b.year) return a.year < b.year;
    if (a.month != b.month) return a.month < b.month;
    return a.day < b.day;
  }
};

struct TimeOfDay {
  int hour{0};   // 0-23
  int minute{0}; // 0-59

  friend bool operator==(const TimeOfDay& a, const TimeOfDay& b) {
    return a.hour == b.hour && a.minute == b.minute;
  }
  friend bool operator<(const TimeOfDay& a, const TimeOfDay& b) {
    if (a.hour != b.hour) return a.hour < b.hour;
    return a.minute < b.minute;
  }
};

struct DateTime {
  Date date{};
  TimeOfDay time{};

  friend bool operator==(const DateTime& a, const DateTime& b) {
    return a.date == b.date && a.time == b.time;
  }
  friend bool operator<(const DateTime& a, const DateTime& b) {
    if (a.date < b.date) return true;
    if (b.date < a.date) return false;
    return a.time < b.time;
  }
};

struct TimeSpan {
  DateTime start{};
  DateTime end{}; // end > start
};

/* ========= events ========= */

using EventId = std::uint64_t;

enum class RepeatRule {
  None,
  Daily,
  Weekly,
  Monthly
  // ignore recurrence by now
};

struct Event {
  EventId id{0};
  std::string title{};
  std::string location{};
  std::string notes{};

  TimeSpan when{};
  RepeatRule repeat{RepeatRule::None};

  // "conflict/ priority"
  int priority{0};
};

/* ========= search/ result ========= */

struct SearchResult {
  std::vector<Event> events{};
};

/* ========= Scheduler ========= */

class Scheduler {
public:
  Scheduler();
  ~Scheduler();

  // basic CRUD
  EventId addEvent(const Event& e);              // return distributed id（or e.id）
  bool removeEvent(EventId id);
  bool rescheduleEvent(EventId id, const TimeSpan& newWhen);
  std::optional<Event> getEvent(EventId id) const;

  // query: get events by time range (week/month are just range wrappers)
  std::vector<Event> queryRange(const DateTime& from,
                               const DateTime& to) const;

  // query: get events by date (day view)
  std::vector<Event> queryDay(const Date& d) const;

  // search: get events by keyword (title/location/notes)
  std::vector<Event> searchText(const std::string& keyword) const;

  // duplicate: copy an event to a new start time (keep duration)
  std::optional<EventId> duplicateEvent(EventId id, const DateTime& newStart);

  // conflict detection: check if a candidate time span conflicts with existing events
  bool hasConflict(const TimeSpan& candidate) const;

private:
  struct Impl;   // Pimpl: hide red-black tree nodes/rotation/coloring, etc.
  Impl* impl_;   
};

} // namespace rbt

#endif // RBT_SCHEDULER_H

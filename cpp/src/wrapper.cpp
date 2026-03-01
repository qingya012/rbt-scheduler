/**
 * wrapper.cpp — Node-Addon-API bridge for rbt::Scheduler
 *
 * Exported JS functions
 * ─────────────────────
 *  insertEvent(id, title, startMin, endMin)  → { ok: bool, conflict: bool }
 *  deleteEvent(id)                           → { ok: bool }
 *  checkConflict(startMin, endMin)           → { conflict: bool, events: Event[] }
 *  clearTree()                               → void
 *  getAll()                                  → Event[]
 *
 * Event shape returned to JS
 * ──────────────────────────
 *  { id: number, title: string, startTime: number, endTime: number }
 */

#include <napi.h>
#include "Scheduler.h"

using namespace rbt;

// ── Module-level singleton ────────────────────────────────────────
// One Scheduler instance per addon load; persists for the lifetime
// of the Node process that required the .node binary.
static Scheduler g_scheduler;

// ── Helpers ──────────────────────────────────────────────────────

static Napi::Object eventToJS(Napi::Env env, const Event& ev) {
    auto obj = Napi::Object::New(env);
    obj.Set("id",        Napi::Number::New(env, static_cast<double>(ev.id)));
    obj.Set("title",     Napi::String::New(env, ev.title));
    obj.Set("startTime", Napi::Number::New(env, ev.range.start));
    obj.Set("endTime",   Napi::Number::New(env, ev.range.end));
    return obj;
}

// ── insertEvent(id, title, startMin, endMin) ─────────────────────
// Returns { ok: bool, conflict: bool }
// If id == 0 the scheduler auto-assigns one (nextId_ internal).
// Pass id > 0 to pin a specific ID (e.g. to match a frontend UUID).
Napi::Value InsertEvent(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 4
        || !info[0].IsNumber()
        || !info[1].IsString()
        || !info[2].IsNumber()
        || !info[3].IsNumber()) {
        Napi::TypeError::New(env,
            "insertEvent(id: number, title: string, startMin: number, endMin: number)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Event ev;
    ev.id          = static_cast<EventId>(info[0].As<Napi::Number>().Int64Value());
    ev.title       = info[1].As<Napi::String>().Utf8Value();
    ev.range.start = info[2].As<Napi::Number>().Int32Value();
    ev.range.end   = info[3].As<Napi::Number>().Int32Value();

    auto result = Napi::Object::New(env);

    if (!ev.range.isValid()) {
        result.Set("ok",       Napi::Boolean::New(env, false));
        result.Set("conflict", Napi::Boolean::New(env, false));
        result.Set("error",    Napi::String::New(env, "Invalid time range"));
        return result;
    }

    Status s = g_scheduler.addEvent(ev, /*allowOverlap=*/false);

    result.Set("ok",       Napi::Boolean::New(env, s == Status::OK));
    result.Set("conflict", Napi::Boolean::New(env, s == Status::CONFLICT));
    if (s == Status::DUPLICATE_ID)
        result.Set("error", Napi::String::New(env, "Duplicate ID"));
    return result;
}

// ── forceInsertEvent(id, title, startMin, endMin) ─────────────────
// Same as insertEvent but with allowOverlap=true.
// Used by the server when it needs the event in the tree for conflict
// detection even though it overlaps — the C++ tree stores it anyway.
// Returns { ok: bool }
Napi::Value ForceInsertEvent(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 4
        || !info[0].IsNumber()
        || !info[1].IsString()
        || !info[2].IsNumber()
        || !info[3].IsNumber()) {
        Napi::TypeError::New(env,
            "forceInsertEvent(id: number, title: string, startMin: number, endMin: number)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Event ev;
    ev.id          = static_cast<EventId>(info[0].As<Napi::Number>().Int64Value());
    ev.title       = info[1].As<Napi::String>().Utf8Value();
    ev.range.start = info[2].As<Napi::Number>().Int32Value();
    ev.range.end   = info[3].As<Napi::Number>().Int32Value();

    auto result = Napi::Object::New(env);

    if (!ev.range.isValid()) {
        result.Set("ok",    Napi::Boolean::New(env, false));
        result.Set("error", Napi::String::New(env, "Invalid time range"));
        return result;
    }

    Status s = g_scheduler.addEvent(ev, /*allowOverlap=*/true);

    result.Set("ok", Napi::Boolean::New(env, s == Status::OK));
    if (s != Status::OK)
        result.Set("error", Napi::String::New(env, "Force insert failed"));
    return result;
}

// ── deleteEvent(id) ──────────────────────────────────────────────
// Returns { ok: bool }
Napi::Value DeleteEvent(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsNumber()) {
        Napi::TypeError::New(env, "deleteEvent(id: number)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }

    EventId id = static_cast<EventId>(info[0].As<Napi::Number>().Int64Value());
    Status  s  = g_scheduler.removeEvent(id);

    auto result = Napi::Object::New(env);
    result.Set("ok", Napi::Boolean::New(env, s == Status::OK));
    if (s == Status::NOT_FOUND)
        result.Set("error", Napi::String::New(env, "Event not found"));
    return result;
}

// ── checkConflict(startMin, endMin) ──────────────────────────────
// Returns { conflict: bool, events: Event[] }
// events[] contains all events that overlap [startMin, endMin).
Napi::Value CheckConflict(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        Napi::TypeError::New(env, "checkConflict(startMin: number, endMin: number)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }

    TimeRange range;
    range.start = info[0].As<Napi::Number>().Int32Value();
    range.end   = info[1].As<Napi::Number>().Int32Value();

    auto overlapping = g_scheduler.listConflicts(range);

    auto jsArr = Napi::Array::New(env, overlapping.size());
    for (size_t i = 0; i < overlapping.size(); ++i)
        jsArr.Set(i, eventToJS(env, overlapping[i]));

    auto result = Napi::Object::New(env);
    result.Set("conflict", Napi::Boolean::New(env, !overlapping.empty()));
    result.Set("events",   jsArr);
    return result;
}

// ── findFirstFreeSlot(durationMinutes, searchStart) ──────────────
// Returns the earliest gap in the RBT that fits durationMinutes, starting
// the search at searchStart (defaults to 0 if omitted or invalid).
// Returns { found: bool, startTime: number, endTime: number }
// When found=true the slot is exactly [startTime, startTime+duration].
Napi::Value FindFirstFreeSlot(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsNumber()) {
        Napi::TypeError::New(env,
            "findFirstFreeSlot(durationMinutes: number, searchStart?: number)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }

    int duration    = info[0].As<Napi::Number>().Int32Value();
    int searchStart = (info.Length() >= 2 && info[1].IsNumber())
                        ? info[1].As<Napi::Number>().Int32Value()
                        : 0;

    // Clamp inputs
    if (duration    <= 0)                  duration    = 1;
    if (searchStart <  0)                  searchStart = 0;
    if (searchStart >= MINUTES_PER_WEEK)   searchStart = 0;

    // Search window: [searchStart, MINUTES_PER_WEEK)
    // Ask for only the first slot (k=1).
    TimeRange window;
    window.start = searchStart;
    window.end   = MINUTES_PER_WEEK;

    auto slots = g_scheduler.suggestSlots(window, duration, /*k=*/1);

    auto result = Napi::Object::New(env);
    if (slots.empty()) {
        fprintf(stderr, "[rbt-cpp] findFirstFreeSlot: no gap found "
                        "(duration=%d, searchStart=%d, treeSize=%zu)\n",
                duration, searchStart, g_scheduler.size());
        result.Set("found",     Napi::Boolean::New(env, false));
        result.Set("startTime", Napi::Number::New(env, -1));
        result.Set("endTime",   Napi::Number::New(env, -1));
    } else {
        // Trim the gap to exactly [gap.start, gap.start + duration]
        int slotStart = slots[0].start;
        int slotEnd   = slotStart + duration;
        fprintf(stderr, "[rbt-cpp] findFirstFreeSlot: found gap [%d, %d) "
                        "(duration=%d, searchStart=%d)\n",
                slotStart, slotEnd, duration, searchStart);
        result.Set("found",     Napi::Boolean::New(env, true));
        result.Set("startTime", Napi::Number::New(env, slotStart));
        result.Set("endTime",   Napi::Number::New(env, slotEnd));
    }
    return result;
}

// ── clearTree() ──────────────────────────────────────────────────
Napi::Value ClearTree(const Napi::CallbackInfo& info) {
    g_scheduler.clear();
    return info.Env().Undefined();
}

// ── getAll() ─────────────────────────────────────────────────────
// Returns all stored events as a JS array — useful for re-hydrating
// the frontend after a page reload without reading data.json.
Napi::Value GetAll(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    auto all   = g_scheduler.exportAllEvents();
    auto jsArr = Napi::Array::New(env, all.size());
    for (size_t i = 0; i < all.size(); ++i)
        jsArr.Set(i, eventToJS(env, all[i]));
    return jsArr;
}

// ── Module registration ───────────────────────────────────────────
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("insertEvent",       Napi::Function::New(env, InsertEvent));
    exports.Set("forceInsertEvent",  Napi::Function::New(env, ForceInsertEvent));
    exports.Set("deleteEvent",       Napi::Function::New(env, DeleteEvent));
    exports.Set("checkConflict",     Napi::Function::New(env, CheckConflict));
    exports.Set("clearTree",         Napi::Function::New(env, ClearTree));
    exports.Set("getAll",            Napi::Function::New(env, GetAll));
    exports.Set("findFirstFreeSlot", Napi::Function::New(env, FindFirstFreeSlot));
    return exports;
}

NODE_API_MODULE(rbt_scheduler, Init)

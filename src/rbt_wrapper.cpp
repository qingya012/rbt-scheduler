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
    exports.Set("insertEvent",   Napi::Function::New(env, InsertEvent));
    exports.Set("deleteEvent",   Napi::Function::New(env, DeleteEvent));
    exports.Set("checkConflict", Napi::Function::New(env, CheckConflict));
    exports.Set("clearTree",     Napi::Function::New(env, ClearTree));
    exports.Set("getAll",        Napi::Function::New(env, GetAll));
    return exports;
}

NODE_API_MODULE(rbt_scheduler, Init)

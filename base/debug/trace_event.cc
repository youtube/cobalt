// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"

#if defined(OS_WIN)
#include "base/debug/trace_event_win.h"
#endif
#include "base/format_macros.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/time.h"

#define USE_UNRELIABLE_NOW

namespace base {
namespace debug {

// Controls the number of trace events we will buffer in-memory
// before throwing them away.
const size_t kTraceEventBufferSize = 500000;
const size_t kTraceEventBatchSize = 1000;

#define TRACE_EVENT_MAX_CATEGORIES 42

static TraceCategory g_categories[TRACE_EVENT_MAX_CATEGORIES] = {
  { "tracing already shutdown", false },
  { "tracing categories exhausted; must increase TRACE_EVENT_MAX_CATEGORIES",
    false }
};
static const TraceCategory* const g_category_already_shutdown =
    &g_categories[0];
static const TraceCategory* const g_category_categories_exhausted =
    &g_categories[1];
static int g_category_index = 2; // skip initial 2 error categories

////////////////////////////////////////////////////////////////////////////////
//
// TraceValue
//
////////////////////////////////////////////////////////////////////////////////

void TraceValue::Destroy() {
  if (type_ == TRACE_TYPE_STRING) {
    value_.as_string_refptr->Release();
    value_.as_string_refptr = NULL;
  }
  type_ = TRACE_TYPE_UNDEFINED;
  value_.as_uint = 0ull;
}

TraceValue& TraceValue::operator=(const TraceValue& rhs) {
  if (this == &rhs)
    return *this;

  Destroy();

  type_ = rhs.type_;
  switch(type_) {
    case TRACE_TYPE_UNDEFINED:
      return *this;
    case TRACE_TYPE_BOOL:
      value_.as_bool = rhs.value_.as_bool;
      return *this;
    case TRACE_TYPE_UINT:
      value_.as_uint = rhs.value_.as_uint;
      return *this;
    case TRACE_TYPE_INT:
      value_.as_int = rhs.value_.as_int;
      return *this;
    case TRACE_TYPE_DOUBLE:
      value_.as_double = rhs.value_.as_double;
      return *this;
    case TRACE_TYPE_POINTER:
      value_.as_pointer = rhs.value_.as_pointer;
      return *this;
    case TRACE_TYPE_STRING:
      value_.as_string_refptr = rhs.value_.as_string_refptr;
      value_.as_string_refptr->AddRef();
      return *this;
    default:
      NOTREACHED();
      return *this;
  }
}

void TraceValue::AppendAsJSON(std::string* out) const {
  char temp_string[128];
  std::string::size_type start_pos;
  switch (type_) {
    case TRACE_TYPE_BOOL:
      *out += as_bool()? "true" : "false";
      break;
    case TRACE_TYPE_UINT:
      base::snprintf(temp_string, arraysize(temp_string), "%llu",
                     static_cast<unsigned long long>(as_uint()));
      *out += temp_string;
      break;
    case TRACE_TYPE_INT:
      base::snprintf(temp_string, arraysize(temp_string), "%lld",
                     static_cast<long long>(as_int()));
      *out += temp_string;
      break;
    case TRACE_TYPE_DOUBLE:
      base::snprintf(temp_string, arraysize(temp_string), "%f", as_double());
      *out += temp_string;
      break;
    case TRACE_TYPE_POINTER:
      base::snprintf(temp_string, arraysize(temp_string), "%llu",
                     static_cast<unsigned long long>(
                       reinterpret_cast<intptr_t>(
                         as_pointer())));
      *out += temp_string;
      break;
    case TRACE_TYPE_STRING:
      *out += "\"";
      start_pos = out->size();
      *out += as_string();
      // replace " character with '
      while ((start_pos = out->find_first_of('\"', start_pos)) !=
             std::string::npos)
        (*out)[start_pos] = '\'';
      *out += "\"";
      break;
    default:
      NOTREACHED() << "Don't know how to print this value";
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceEvent
//
////////////////////////////////////////////////////////////////////////////////

namespace {

const char* GetPhaseStr(TraceEventPhase phase) {
  switch(phase) {
    case TRACE_EVENT_PHASE_BEGIN:
      return "B";
    case TRACE_EVENT_PHASE_INSTANT:
      return "I";
    case TRACE_EVENT_PHASE_END:
      return "E";
    default:
      NOTREACHED() << "Invalid phase argument";
      return "?";
  }
}

}  // namespace

TraceEvent::TraceEvent()
    : process_id_(0),
      thread_id_(0),
      phase_(TRACE_EVENT_PHASE_BEGIN),
      category_(NULL),
      name_(NULL) {
  arg_names_[0] = NULL;
  arg_names_[1] = NULL;
}

TraceEvent::TraceEvent(unsigned long process_id,
                       unsigned long thread_id,
                       TimeTicks timestamp,
                       TraceEventPhase phase,
                       const TraceCategory* category,
                       const char* name,
                       const char* arg1_name, const TraceValue& arg1_val,
                       const char* arg2_name, const TraceValue& arg2_val)
    : process_id_(process_id),
      thread_id_(thread_id),
      timestamp_(timestamp),
      phase_(phase),
      category_(category),
      name_(name) {
  COMPILE_ASSERT(kTraceMaxNumArgs == 2, TraceEvent_arg_count_out_of_sync);
  arg_names_[0] = arg1_name;
  arg_names_[1] = arg2_name;
  arg_values_[0] = arg1_val;
  arg_values_[1] = arg2_val;
}

TraceEvent::~TraceEvent() {
}

void TraceEvent::AppendEventsAsJSON(const std::vector<TraceEvent>& events,
                                    size_t start,
                                    size_t count,
                                    std::string* out) {
  *out += "[";
  for (size_t i = 0; i < count && start + i < events.size(); ++i) {
    if (i > 0)
      *out += ",";
    events[i + start].AppendAsJSON(out);
  }
  *out += "]";
}

void TraceEvent::AppendAsJSON(std::string* out) const {
  const char* phase_str = GetPhaseStr(phase_);
  int64 time_int64 = timestamp_.ToInternalValue();
  // Category name checked at category creation time.
  DCHECK(!strchr(name_, '"'));
  StringAppendF(out,
      "{\"cat\":\"%s\",\"pid\":%i,\"tid\":%i,\"ts\":%lld,"
      "\"ph\":\"%s\",\"name\":\"%s\",\"args\":{",
      category_->name,
      static_cast<int>(process_id_),
      static_cast<int>(thread_id_),
      static_cast<long long>(time_int64),
      phase_str,
      name_);

  // Output argument names and values, stop at first NULL argument name.
  for (size_t i = 0; i < kTraceMaxNumArgs && arg_names_[i]; ++i) {
    if (i > 0)
      *out += ",";
    *out += "\"";
    *out += arg_names_[i];
    *out += "\":";
    arg_values_[i].AppendAsJSON(out);
  }
  *out += "}}";
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceLog
//
////////////////////////////////////////////////////////////////////////////////

// static
TraceLog* TraceLog::GetInstance() {
  return Singleton<TraceLog, StaticMemorySingletonTraits<TraceLog> >::get();
}

TraceLog::TraceLog()
    : enabled_(false) {
}

TraceLog::~TraceLog() {
}

const TraceCategory* TraceLog::GetCategory(const char* name) {
  TraceLog* tracelog = GetInstance();
  if (!tracelog){
    CHECK(!g_category_already_shutdown->enabled);
    return g_category_already_shutdown;
  }
  return tracelog->GetCategoryInternal(name);
}

const TraceCategory* TraceLog::GetCategoryInternal(const char* name) {
  AutoLock lock(lock_);
  DCHECK(!strchr(name, '"')) << "Names may not contain double quote char";

  // Search for pre-existing category matching this name
  for (int i = 0; i < g_category_index; i++) {
    if (strcmp(g_categories[i].name, name) == 0)
      return &g_categories[i];
  }

  // Create a new category
  DCHECK(g_category_index < TRACE_EVENT_MAX_CATEGORIES) <<
      "must increase TRACE_EVENT_MAX_CATEGORIES";
  if (g_category_index < TRACE_EVENT_MAX_CATEGORIES) {
    int new_index = g_category_index++;
    g_categories[new_index].name = name;
    DCHECK(!g_categories[new_index].enabled);
    g_categories[new_index].enabled = enabled_;
    return &g_categories[new_index];
  } else {
    return g_category_categories_exhausted;
  }
}

void TraceLog::SetEnabled(bool enabled) {
  {
    AutoLock lock(lock_);
    if (enabled == enabled_)
      return;
    logged_events_.reserve(1024);
    enabled_ = enabled;
    for (int i = 0; i < g_category_index; i++) {
      //TODO(scheib): If changed to enable specific categories instead of all
      // check GetCategoryInternal creation code that users TraceLog::enabled_
      g_categories[i].enabled = enabled;
    }
  } // release lock
  if (!enabled)
    Flush();
}

float TraceLog::GetBufferPercentFull() const {
  return (float)((double)logged_events_.size()/(double)kTraceEventBufferSize);
}

void TraceLog::SetOutputCallback(const TraceLog::OutputCallback& cb) {
  AutoLock lock(lock_);
  output_callback_ = cb;
  logged_events_.clear();
}

void TraceLog::SetBufferFullCallback(const TraceLog::BufferFullCallback& cb) {
  AutoLock lock(lock_);
  buffer_full_callback_ = cb;
}

void TraceLog::Flush() {
  std::vector<TraceEvent> previous_logged_events;
  OutputCallback output_callback_copy;
  {
    AutoLock lock(lock_);
    previous_logged_events.swap(logged_events_);
    output_callback_copy = output_callback_;
  } // release lock

  if (output_callback_copy.is_null())
    return;

  for (size_t i = 0;
       i < previous_logged_events.size();
       i += kTraceEventBatchSize) {
    scoped_refptr<RefCountedString> json_events_str_ptr =
      new RefCountedString();
    TraceEvent::AppendEventsAsJSON(previous_logged_events,
                                   i,
                                   kTraceEventBatchSize,
                                   &(json_events_str_ptr->data));
    output_callback_copy.Run(json_events_str_ptr.get());
  }
}

int TraceLog::AddTraceEvent(TraceEventPhase phase,
                            const TraceCategory* category,
                            const char* name,
                            const char* arg1_name, TraceValue arg1_val,
                            const char* arg2_name, TraceValue arg2_val,
                            int threshold_begin_id,
                            int64 threshold) {
  DCHECK(name);
#ifdef USE_UNRELIABLE_NOW
  TimeTicks now = TimeTicks::HighResNow();
#else
  TimeTicks now = TimeTicks::Now();
#endif
  BufferFullCallback buffer_full_callback_copy;
  int ret_begin_id = -1;
  {
    AutoLock lock(lock_);
    if (!enabled_ || !category->enabled)
      return -1;
    if (logged_events_.size() >= kTraceEventBufferSize)
      return -1;
    if (threshold_begin_id > -1) {
      DCHECK(phase == base::debug::TRACE_EVENT_PHASE_END);
      size_t begin_i = static_cast<size_t>(threshold_begin_id);
      // Return now if there has been a flush since the begin event was posted.
      if (begin_i >= logged_events_.size())
        return -1;
      // Determine whether to drop the begin/end pair.
      TimeDelta elapsed = now - logged_events_[begin_i].timestamp();
      if (elapsed < TimeDelta::FromMicroseconds(threshold)) {
        // Remove begin event and do not add end event.
        // This will be expensive if there have been other events in the
        // mean time (should be rare).
        logged_events_.erase(logged_events_.begin() + begin_i);
        return -1;
      }
    }
    ret_begin_id = static_cast<int>(logged_events_.size());
    logged_events_.push_back(
        TraceEvent(static_cast<unsigned long>(base::GetCurrentProcId()),
                   PlatformThread::CurrentId(),
                   now, phase, category, name,
                   arg1_name, arg1_val,
                   arg2_name, arg2_val));

    if (logged_events_.size() == kTraceEventBufferSize) {
      buffer_full_callback_copy = buffer_full_callback_;
    }
  } // release lock

  if (!buffer_full_callback_copy.is_null())
    buffer_full_callback_copy.Run();

  return ret_begin_id;
}

void TraceLog::AddTraceEventEtw(TraceEventPhase phase,
                                const char* name,
                                const void* id,
                                const char* extra) {
  // Legacy trace points on windows called to ETW
#if defined(OS_WIN)
  TraceEventETWProvider::Trace(name, phase, id, extra);
#endif

  // Also add new trace event behavior
  static const TraceCategory* category = GetCategory("ETW Trace Event");
  if (category->enabled) {
    TraceLog* tracelog = TraceLog::GetInstance();
    if (!tracelog)
      return;
    tracelog->AddTraceEvent(phase, category, name,
                            "id", id,
                            "extra", extra ? extra : "",
                            -1, 0);
  }
}

void TraceLog::Resurrect() {
  StaticMemorySingletonTraits<TraceLog>::Resurrect();
}

namespace internal {

void TraceEndOnScopeClose::Initialize(const TraceCategory* category,
                                      const char* name) {
  data_.category = category;
  data_.name = name;
  p_data_ = &data_;
}

void TraceEndOnScopeClose::AddEventIfEnabled() {
  // Only called when p_data_ is non-null.
  if (p_data_->category->enabled) {
    base::debug::TraceLog::GetInstance()->AddTraceEvent(
        base::debug::TRACE_EVENT_PHASE_END,
        p_data_->category,
        p_data_->name,
        NULL, 0, NULL, 0,
        -1, 0);
  }
}

void TraceEndOnScopeCloseThreshold::Initialize(const TraceCategory* category,
                                               const char* name,
                                               int threshold_begin_id,
                                               int64 threshold) {
  data_.category = category;
  data_.name = name;
  data_.threshold_begin_id = threshold_begin_id;
  data_.threshold = threshold;
  p_data_ = &data_;
}

void TraceEndOnScopeCloseThreshold::AddEventIfEnabled() {
  // Only called when p_data_ is non-null.
  if (p_data_->category->enabled) {
    base::debug::TraceLog::GetInstance()->AddTraceEvent(
        base::debug::TRACE_EVENT_PHASE_END,
        p_data_->category,
        p_data_->name,
        NULL, 0, NULL, 0,
        p_data_->threshold_begin_id, p_data_->threshold);
  }
}

} // namespace internal

}  // namespace debug
}  // namespace base

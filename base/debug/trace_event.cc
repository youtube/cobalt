// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"

#include <algorithm>

#if defined(OS_WIN)
#include "base/debug/trace_event_win.h"
#endif
#include "base/format_macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_local.h"
#include "base/utf_string_conversions.h"
#include "base/stl_util.h"
#include "base/time.h"

#define USE_UNRELIABLE_NOW

class DeleteTraceLogForTesting {
 public:
  static void Delete() {
    Singleton<base::debug::TraceLog,
              StaticMemorySingletonTraits<base::debug::TraceLog> >::OnExit(0);
  }
};

namespace base {
namespace debug {

// Controls the number of trace events we will buffer in-memory
// before throwing them away.
const size_t kTraceEventBufferSize = 500000;
const size_t kTraceEventBatchSize = 1000;

#define TRACE_EVENT_MAX_CATEGORIES 100

static TraceCategory g_categories[TRACE_EVENT_MAX_CATEGORIES] = {
  { "tracing already shutdown", false },
  { "tracing categories exhausted; must increase TRACE_EVENT_MAX_CATEGORIES",
    false },
  { "__metadata",
    false }
};
static const TraceCategory* const g_category_already_shutdown =
    &g_categories[0];
static const TraceCategory* const g_category_categories_exhausted =
    &g_categories[1];
static const TraceCategory* const g_category_metadata =
    &g_categories[2];
static int g_category_index = 3; // skip initial 3 categories

// Flag to indicate whether we captured the current thread name
static ThreadLocalBoolean g_current_thread_name_captured;

////////////////////////////////////////////////////////////////////////////////
//
// TraceValue
//
////////////////////////////////////////////////////////////////////////////////

void TraceValue::AppendAsJSON(std::string* out) const {
  char temp_string[128];
  std::string::size_type start_pos;
  switch (type_) {
    case TRACE_TYPE_BOOL:
      *out += as_bool() ? "true" : "false";
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
    case TRACE_TYPE_STATIC_STRING:
      *out += "\"";
      start_pos = out->size();
      *out += as_string() ? as_string() : "NULL";
      // insert backslash before special characters for proper json format.
      while ((start_pos = out->find_first_of("\\\"", start_pos)) !=
             std::string::npos) {
        out->insert(start_pos, 1, '\\');
        // skip inserted escape character and following character.
        start_pos += 2;
      }
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
    case TRACE_EVENT_PHASE_METADATA:
      return "M";
    default:
      NOTREACHED() << "Invalid phase argument";
      return "?";
  }
}

size_t GetAllocLength(const char* str) { return str ? strlen(str) + 1 : 0; }

// Copies |*member| into |*buffer|, sets |*member| to point to this new
// location, and then advances |*buffer| by the amount written.
void CopyTraceEventParameter(char** buffer,
                             const char** member,
                             const char* end) {
  if (*member) {
    size_t written = strlcpy(*buffer, *member, end - *buffer) + 1;
    DCHECK_LE(static_cast<int>(written), end - *buffer);
    *member = *buffer;
    *buffer += written;
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
                       const char* arg2_name, const TraceValue& arg2_val,
                       bool copy)
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

  size_t alloc_size = 0;
  if (copy) {
    alloc_size += GetAllocLength(name);
    alloc_size += GetAllocLength(arg1_name);
    alloc_size += GetAllocLength(arg2_name);
  }

  bool arg1_is_copy = (arg1_val.type() == TraceValue::TRACE_TYPE_STRING);
  bool arg2_is_copy = (arg2_val.type() == TraceValue::TRACE_TYPE_STRING);

  // We only take a copy of arg_vals if they are of type string (not static
  // string), regardless of the |copy| flag.
  if (arg1_is_copy)
    alloc_size += GetAllocLength(arg1_val.as_string());
  if (arg2_is_copy)
    alloc_size += GetAllocLength(arg2_val.as_string());

  if (alloc_size) {
    parameter_copy_storage_ = new base::RefCountedString;
    parameter_copy_storage_->data().resize(alloc_size);
    char* ptr = string_as_array(&parameter_copy_storage_->data());
    const char* end = ptr + alloc_size;
    if (copy) {
      CopyTraceEventParameter(&ptr, &name_, end);
      CopyTraceEventParameter(&ptr, &arg_names_[0], end);
      CopyTraceEventParameter(&ptr, &arg_names_[1], end);
    }
    if (arg1_is_copy)
      CopyTraceEventParameter(&ptr, arg_values_[0].as_assignable_string(), end);
    if (arg2_is_copy)
      CopyTraceEventParameter(&ptr, arg_values_[1].as_assignable_string(), end);
    DCHECK_EQ(end, ptr) << "Overrun by " << ptr - end;
  }
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

static void EnableMatchingCategory(int category_index,
                                   const std::vector<std::string>& patterns,
                                   bool is_included) {
  std::vector<std::string>::const_iterator ci = patterns.begin();
  bool is_match = false;
  for (; ci != patterns.end(); ++ci) {
    is_match = MatchPattern(g_categories[category_index].name, ci->c_str());
    if (is_match)
      break;
  }
  g_categories[category_index].enabled = is_match? is_included : !is_included;
}

// Enable/disable each category based on the category filters in |patterns|.
// If the category name matches one of the patterns, its enabled status is set
// to |is_included|. Otherwise its enabled status is set to !|is_included|.
static void EnableMatchingCategories(const std::vector<std::string>& patterns,
                                     bool is_included) {
  for (int i = 0; i < g_category_index; i++)
    EnableMatchingCategory(i, patterns, is_included);
}

const TraceCategory* TraceLog::GetCategoryInternal(const char* name) {
  AutoLock lock(lock_);
  DCHECK(!strchr(name, '"')) << "Category names may not contain double quote";

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
    if (enabled_) {
      // Note that if both included and excluded_categories are empty, the else
      // clause below excludes nothing, thereby enabling this category.
      if (!included_categories_.empty())
        EnableMatchingCategory(new_index, included_categories_, true);
      else
        EnableMatchingCategory(new_index, excluded_categories_, false);
    } else {
      g_categories[new_index].enabled = false;
    }
    return &g_categories[new_index];
  } else {
    return g_category_categories_exhausted;
  }
}

void TraceLog::GetKnownCategories(std::vector<std::string>* categories) {
  AutoLock lock(lock_);
  for (int i = 0; i < g_category_index; i++)
    categories->push_back(g_categories[i].name);
}

void TraceLog::SetEnabled(const std::vector<std::string>& included_categories,
                          const std::vector<std::string>& excluded_categories) {
  AutoLock lock(lock_);
  if (enabled_)
    return;
  logged_events_.reserve(1024);
  enabled_ = true;
  included_categories_ = included_categories;
  excluded_categories_ = excluded_categories;
  // Note that if both included and excluded_categories are empty, the else
  // clause below excludes nothing, thereby enabling all categories.
  if (!included_categories_.empty())
    EnableMatchingCategories(included_categories_, true);
  else
    EnableMatchingCategories(excluded_categories_, false);
}

void TraceLog::SetDisabled() {
  {
    AutoLock lock(lock_);
    if (!enabled_)
      return;
    enabled_ = false;
    included_categories_.clear();
    excluded_categories_.clear();
    for (int i = 0; i < g_category_index; i++)
      g_categories[i].enabled = false;
    AddCurrentMetadataEvents();
  }  // release lock
  Flush();
}

void TraceLog::SetEnabled(bool enabled) {
  if (enabled)
    SetEnabled(std::vector<std::string>(), std::vector<std::string>());
  else
    SetDisabled();
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
  }  // release lock

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
    output_callback_copy.Run(json_events_str_ptr);
  }
}

int TraceLog::AddTraceEvent(TraceEventPhase phase,
                            const TraceCategory* category,
                            const char* name,
                            const char* arg1_name, TraceValue arg1_val,
                            const char* arg2_name, TraceValue arg2_val,
                            int threshold_begin_id,
                            int64 threshold,
                            EventFlags flags) {
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
    if (!category->enabled)
      return -1;
    if (logged_events_.size() >= kTraceEventBufferSize)
      return -1;

    PlatformThreadId thread_id = PlatformThread::CurrentId();

    // Record the name of the calling thread, if not done already.
    if (!g_current_thread_name_captured.Get()) {
      const char* cur_name = PlatformThread::GetName();
      base::hash_map<PlatformThreadId, std::string>::iterator existing_name =
          thread_names_.find(thread_id);
      if (existing_name == thread_names_.end()) {
        // This is a new thread id, and a new name.
        thread_names_[thread_id] = cur_name ? cur_name : "";
      } else if(cur_name != NULL) {
        // This is a thread id that we've seen before, but potentially with a
        // new name.
        std::vector<std::string> existing_names;
        Tokenize(existing_name->second, std::string(","), &existing_names);
        bool found = std::find(existing_names.begin(),
                               existing_names.end(),
                               cur_name) != existing_names.end();
        if (!found) {
          existing_names.push_back(cur_name);
          thread_names_[thread_id] =
              JoinString(existing_names, ',');
        }
      }
    }

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
                   thread_id,
                   now, phase, category, name,
                   arg1_name, arg1_val,
                   arg2_name, arg2_val,
                   flags & EVENT_FLAG_COPY));

    if (logged_events_.size() == kTraceEventBufferSize) {
      buffer_full_callback_copy = buffer_full_callback_;
    }
  }  // release lock

  if (!buffer_full_callback_copy.is_null())
    buffer_full_callback_copy.Run();

  return ret_begin_id;
}

void TraceLog::AddTraceEventEtw(TraceEventPhase phase,
                                const char* name,
                                const void* id,
                                const char* extra) {
#if defined(OS_WIN)
  TraceEventETWProvider::Trace(name, phase, id, extra);
#endif
  INTERNAL_TRACE_EVENT_ADD(phase,
      "ETW Trace Event", name, "id", id, "extra", TRACE_STR_COPY(extra),
      base::debug::TraceLog::EVENT_FLAG_NONE);
}

void TraceLog::AddTraceEventEtw(TraceEventPhase phase,
                                const char* name,
                                const void* id,
                                const std::string& extra)
{
#if defined(OS_WIN)
  TraceEventETWProvider::Trace(name, phase, id, extra);
#endif
  INTERNAL_TRACE_EVENT_ADD(phase,
      "ETW Trace Event", name, "id", id, "extra", extra,
      base::debug::TraceLog::EVENT_FLAG_NONE);
}

void TraceLog::AddCurrentMetadataEvents() {
  lock_.AssertAcquired();
  for(base::hash_map<PlatformThreadId, std::string>::iterator it =
          thread_names_.begin();
      it != thread_names_.end();
      it++) {
    if (!it->second.empty())
      logged_events_.push_back(
          TraceEvent(static_cast<unsigned long>(base::GetCurrentProcId()),
                     it->first,
                     TimeTicks(), base::debug::TRACE_EVENT_PHASE_METADATA,
                     g_category_metadata, "thread_name",
                     "name", it->second,
                     NULL, 0,
                     false));
  }
}

void TraceLog::DeleteForTesting() {
  DeleteTraceLogForTesting::Delete();
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
        -1, 0, TraceLog::EVENT_FLAG_NONE);
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
        p_data_->threshold_begin_id, p_data_->threshold,
        TraceLog::EVENT_FLAG_NONE);
  }
}

}  // namespace internal

}  // namespace debug
}  // namespace base

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"

#include <algorithm>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/string_tokenizer.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local.h"
#include "base/utf_string_conversions.h"
#include "base/stl_util.h"
#include "base/time.h"

#if defined(OS_WIN)
#include "base/debug/trace_event_win.h"
#endif

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

namespace {

// Specify these values when the corresponding argument of AddTraceEvent is not
// used.
static const char* kNoArgName = NULL;
static const int kNoArgValue = 0;
static const int kNoThreshholdBeginId = -1;
static const int64 kNoThresholdValue = 0;
static const int kNoEventId = 0;

TraceCategory g_categories[TRACE_EVENT_MAX_CATEGORIES] = {
  { "tracing already shutdown", false },
  { "tracing categories exhausted; must increase TRACE_EVENT_MAX_CATEGORIES",
    false },
  { "__metadata",
    false }
};
const TraceCategory* const g_category_already_shutdown =
    &g_categories[0];
const TraceCategory* const g_category_categories_exhausted =
    &g_categories[1];
const TraceCategory* const g_category_metadata =
    &g_categories[2];
int g_category_index = 3; // skip initial 3 categories

// The most-recently captured name of the current thread
LazyInstance<ThreadLocalPointer<const char>,
             LeakyLazyInstanceTraits<ThreadLocalPointer<const char> > >
    g_current_thread_name = LAZY_INSTANCE_INITIALIZER;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// TraceValue
//
////////////////////////////////////////////////////////////////////////////////

void TraceValue::AppendAsJSON(std::string* out) const {
  std::string::size_type start_pos;
  switch (type_) {
    case TRACE_TYPE_BOOL:
      *out += as_bool() ? "true" : "false";
      break;
    case TRACE_TYPE_UINT:
      StringAppendF(out, "%" PRIu64, as_uint());
      break;
    case TRACE_TYPE_INT:
      StringAppendF(out, "%" PRId64, as_int());
      break;
    case TRACE_TYPE_DOUBLE:
      StringAppendF(out, "%f", as_double());
      break;
    case TRACE_TYPE_POINTER:
      // JSON only supports double and int numbers.
      // So as not to lose bits from a 64-bit pointer, output as a hex string.
      StringAppendF(out, "\"%" PRIx64 "\"", static_cast<uint64>(
                                     reinterpret_cast<intptr_t>(
                                     as_pointer())));
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
// TraceID
//
////////////////////////////////////////////////////////////////////////////////

TraceID::TraceID(void* rhs) {
  data_ = base::debug::TraceLog::GetInstance()->GetIntraProcessID(
      static_cast<uint64>(reinterpret_cast<uintptr_t>(rhs)));
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceEvent
//
////////////////////////////////////////////////////////////////////////////////

namespace {

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
    : id_(0u),
      category_(NULL),
      name_(NULL),
      thread_id_(0),
      phase_(TRACE_EVENT_PHASE_BEGIN),
      flags_(0) {
  arg_names_[0] = NULL;
  arg_names_[1] = NULL;
}

TraceEvent::TraceEvent(int thread_id,
                       TimeTicks timestamp,
                       TraceEventPhase phase,
                       const TraceCategory* category,
                       const char* name,
                       TraceID id,
                       const char* arg1_name, const TraceValue& arg1_val,
                       const char* arg2_name, const TraceValue& arg2_val,
                       TraceEventFlags flags)
    : timestamp_(timestamp),
      id_(id),
      category_(category),
      name_(name),
      thread_id_(thread_id),
      phase_(phase),
      flags_(flags) {
  COMPILE_ASSERT(kTraceMaxNumArgs == 2, TraceEvent_arg_count_out_of_sync);
  arg_names_[0] = arg1_name;
  arg_names_[1] = arg2_name;
  arg_values_[0] = arg1_val;
  arg_values_[1] = arg2_val;

  bool copy = !!(flags & TRACE_EVENT_FLAG_COPY);
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
  for (size_t i = 0; i < count && start + i < events.size(); ++i) {
    if (i > 0)
      *out += ",";
    events[i + start].AppendAsJSON(out);
  }
}

void TraceEvent::AppendAsJSON(std::string* out) const {
  const char phase_char = GetPhaseChar(phase_);
  int64 time_int64 = timestamp_.ToInternalValue();
  int process_id = TraceLog::GetInstance()->process_id();
  // Category name checked at category creation time.
  DCHECK(!strchr(name_, '"'));
  StringAppendF(out,
      "{\"cat\":\"%s\",\"pid\":%i,\"tid\":%i,\"ts\":%" PRId64 ","
      "\"ph\":\"%c\",\"name\":\"%s\",\"args\":{",
      category_->name,
      process_id,
      thread_id_,
      time_int64,
      phase_char,
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
  *out += "}";

  // If id_ is set, print it out as a hex string so we don't loose any
  // bits (it might be a 64-bit pointer).
  if (flags_ & TRACE_EVENT_FLAG_HAS_ID)
    StringAppendF(out, ",\"id\":\"%" PRIx64 "\"", id_.data());
  *out += "}";
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceResultBuffer
//
////////////////////////////////////////////////////////////////////////////////

TraceResultBuffer::OutputCallback
    TraceResultBuffer::SimpleOutput::GetCallback() {
  return base::Bind(&SimpleOutput::Append, base::Unretained(this));
}

void TraceResultBuffer::SimpleOutput::Append(
    const std::string& json_trace_output) {
  json_output += json_trace_output;
}

TraceResultBuffer::TraceResultBuffer() : append_comma_(false) {
}

TraceResultBuffer::~TraceResultBuffer() {
}

void TraceResultBuffer::SetOutputCallback(OutputCallback json_chunk_callback) {
  output_callback_ = json_chunk_callback;
}

void TraceResultBuffer::Start() {
  append_comma_ = false;
  output_callback_.Run("[");
}

void TraceResultBuffer::AddFragment(const std::string& trace_fragment) {
  if (append_comma_)
    output_callback_.Run(",");
  append_comma_ = true;
  output_callback_.Run(trace_fragment);
}

void TraceResultBuffer::Finish() {
  output_callback_.Run("]");
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
  SetProcessID(static_cast<int>(base::GetCurrentProcId()));
}

TraceLog::~TraceLog() {
}

const TraceCategory* TraceLog::GetCategory(const char* name) {
  TraceLog* tracelog = GetInstance();
  if (!tracelog){
    DCHECK(!g_category_already_shutdown->enabled);
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
  ANNOTATE_BENIGN_RACE(&g_categories[category_index].enabled,
                       "trace_event category enabled");
  g_categories[category_index].enabled = is_match ? is_included : !is_included;
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
      ANNOTATE_BENIGN_RACE(&g_categories[new_index].enabled,
                           "trace_event category enabled");
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

void TraceLog::SetEnabled(const std::string& categories) {
  std::vector<std::string> included, excluded;
  // Tokenize list of categories, delimited by ','.
  StringTokenizer tokens(categories, ",");
  while (tokens.GetNext()) {
    bool is_included = true;
    std::string category = tokens.token();
    // Excluded categories start with '-'.
    if (category.at(0) == '-') {
      // Remove '-' from category string.
      category = category.substr(1);
      is_included = false;
    }
    if (is_included)
      included.push_back(category);
    else
      excluded.push_back(category);
  }
  SetEnabled(included, excluded);
}

void TraceLog::GetEnabledTraceCategories(
    std::vector<std::string>* included_out,
    std::vector<std::string>* excluded_out) {
  AutoLock lock(lock_);
  if (enabled_) {
    *included_out = included_categories_;
    *excluded_out = excluded_categories_;
  }
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
                            TraceID id,
                            const char* arg1_name, TraceValue arg1_val,
                            const char* arg2_name, TraceValue arg2_val,
                            int threshold_begin_id,
                            int64 threshold,
                            TraceEventFlags flags) {
  DCHECK(name);
  TimeTicks now = TimeTicks::HighResNow();
  BufferFullCallback buffer_full_callback_copy;
  int ret_begin_id = -1;
  {
    AutoLock lock(lock_);
    if (!category->enabled)
      return -1;
    if (logged_events_.size() >= kTraceEventBufferSize)
      return -1;

    int thread_id = static_cast<int>(PlatformThread::CurrentId());

    const char* new_name = PlatformThread::GetName();
    // Check if the thread name has been set or changed since the previous
    // call (if any), but don't bother if the new name is empty. Note this will
    // not detect a thread name change within the same char* buffer address: we
    // favor common case performance over corner case correctness.
    if (new_name != g_current_thread_name.Get().Get() &&
        new_name && *new_name) {
      g_current_thread_name.Get().Set(new_name);
      base::hash_map<int, std::string>::iterator existing_name =
          thread_names_.find(thread_id);
      if (existing_name == thread_names_.end()) {
        // This is a new thread id, and a new name.
        thread_names_[thread_id] = new_name;
      } else {
        // This is a thread id that we've seen before, but potentially with a
        // new name.
        std::vector<base::StringPiece> existing_names;
        Tokenize(existing_name->second, ",", &existing_names);
        bool found = std::find(existing_names.begin(),
                               existing_names.end(),
                               new_name) != existing_names.end();
        if (!found) {
          existing_name->second.push_back(',');
          existing_name->second.append(new_name);
        }
      }
    }

    if (threshold_begin_id > -1) {
      DCHECK(phase == TRACE_EVENT_PHASE_END);
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
        TraceEvent(thread_id,
                   now, phase, category, name, id,
                   arg1_name, arg1_val,
                   arg2_name, arg2_val,
                   flags));

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
      TRACE_EVENT_FLAG_COPY);
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
      TRACE_EVENT_FLAG_COPY);
}

int TraceLog::AddCounterEvent(const TraceCategory* category,
                              const char* name,
                              const char* value1_name, int32 value1_val,
                              const char* value2_name, int32 value2_val,
                              TraceEventFlags flags) {
  return AddTraceEvent(TRACE_EVENT_PHASE_COUNTER,
                       category,
                       name,
                       0,
                       value1_name, value1_val,
                       value2_name, value2_val,
                       -1, 0,
                       flags);
}

void TraceLog::AddCurrentMetadataEvents() {
  lock_.AssertAcquired();
  for(base::hash_map<int, std::string>::iterator it = thread_names_.begin();
      it != thread_names_.end();
      it++) {
    if (!it->second.empty())
      logged_events_.push_back(
          TraceEvent(it->first,
                     TimeTicks(), TRACE_EVENT_PHASE_METADATA,
                     g_category_metadata, "thread_name", 0,
                     "name", it->second,
                     NULL, 0,
                     TRACE_EVENT_FLAG_NONE));
  }
}

void TraceLog::DeleteForTesting() {
  DeleteTraceLogForTesting::Delete();
}

void TraceLog::Resurrect() {
  StaticMemorySingletonTraits<TraceLog>::Resurrect();
}

void TraceLog::SetProcessID(int process_id) {
  process_id_ = process_id;
  // Create a FNV hash from the process ID for XORing.
  // See http://isthe.com/chongo/tech/comp/fnv/ for algorithm details.
  uint64 offset_basis = 14695981039346656037ull;
  uint64 fnv_prime = 1099511628211ull;
  unsigned long long pid = static_cast<unsigned long long>(process_id_);
  process_id_hash_ = (offset_basis ^ pid) * fnv_prime;
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
        TRACE_EVENT_PHASE_END,
        p_data_->category,
        p_data_->name, kNoEventId,
        kNoArgName, kNoArgValue, kNoArgName, kNoArgValue,
        kNoThreshholdBeginId, kNoThresholdValue, TRACE_EVENT_FLAG_NONE);
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
        TRACE_EVENT_PHASE_END,
        p_data_->category,
        p_data_->name, kNoEventId,
        kNoArgName, kNoArgValue, kNoArgName, kNoArgValue,
        p_data_->threshold_begin_id, p_data_->threshold,
        TRACE_EVENT_FLAG_NONE);
  }
}

}  // namespace internal

}  // namespace debug
}  // namespace base

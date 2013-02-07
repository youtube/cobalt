// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/trace_event.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"

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

// Parallel arrays g_categories and g_category_enabled are separate so that
// a pointer to a member of g_category_enabled can be easily converted to an
// index into g_categories. This allows macros to deal only with char enabled
// pointers from g_category_enabled, and we can convert internally to determine
// the category name from the char enabled pointer.
const char* g_categories[TRACE_EVENT_MAX_CATEGORIES] = {
  "tracing already shutdown",
  "tracing categories exhausted; must increase TRACE_EVENT_MAX_CATEGORIES",
  "__metadata",
};
// The enabled flag is char instead of bool so that the API can be used from C.
unsigned char g_category_enabled[TRACE_EVENT_MAX_CATEGORIES] = { 0 };
const int g_category_already_shutdown = 0;
const int g_category_categories_exhausted = 1;
const int g_category_metadata = 2;
int g_category_index = 3; // skip initial 3 categories

// The most-recently captured name of the current thread
LazyInstance<ThreadLocalPointer<const char> >::Leaky
    g_current_thread_name = LAZY_INSTANCE_INITIALIZER;

}  // namespace

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
      category_enabled_(NULL),
      name_(NULL),
      thread_id_(0),
      phase_(TRACE_EVENT_PHASE_BEGIN),
      flags_(0) {
  arg_names_[0] = NULL;
  arg_names_[1] = NULL;
  memset(arg_values_, 0, sizeof(arg_values_));
}

TraceEvent::TraceEvent(int thread_id,
                       TimeTicks timestamp,
                       char phase,
                       const unsigned char* category_enabled,
                       const char* name,
                       unsigned long long id,
                       int num_args,
                       const char** arg_names,
                       const unsigned char* arg_types,
                       const unsigned long long* arg_values,
                       unsigned char flags)
    : timestamp_(timestamp),
      id_(id),
      category_enabled_(category_enabled),
      name_(name),
      thread_id_(thread_id),
      phase_(phase),
      flags_(flags) {
  // Clamp num_args since it may have been set by a third_party library.
  num_args = (num_args > kTraceMaxNumArgs) ? kTraceMaxNumArgs : num_args;
  int i = 0;
  for (; i < num_args; ++i) {
    arg_names_[i] = arg_names[i];
    arg_values_[i].as_uint = arg_values[i];
    arg_types_[i] = arg_types[i];
  }
  for (; i < kTraceMaxNumArgs; ++i) {
    arg_names_[i] = NULL;
    arg_values_[i].as_uint = 0u;
    arg_types_[i] = TRACE_VALUE_TYPE_UINT;
  }

  bool copy = !!(flags & TRACE_EVENT_FLAG_COPY);
  size_t alloc_size = 0;
  if (copy) {
    alloc_size += GetAllocLength(name);
    for (i = 0; i < num_args; ++i) {
      alloc_size += GetAllocLength(arg_names_[i]);
      if (arg_types_[i] == TRACE_VALUE_TYPE_STRING)
        arg_types_[i] = TRACE_VALUE_TYPE_COPY_STRING;
    }
  }

  bool arg_is_copy[kTraceMaxNumArgs];
  for (i = 0; i < num_args; ++i) {
    // We only take a copy of arg_vals if they are of type COPY_STRING.
    arg_is_copy[i] = (arg_types_[i] == TRACE_VALUE_TYPE_COPY_STRING);
    if (arg_is_copy[i])
      alloc_size += GetAllocLength(arg_values_[i].as_string);
  }

  if (alloc_size) {
    parameter_copy_storage_ = new base::RefCountedString;
    parameter_copy_storage_->data().resize(alloc_size);
    char* ptr = string_as_array(&parameter_copy_storage_->data());
    const char* end = ptr + alloc_size;
    if (copy) {
      CopyTraceEventParameter(&ptr, &name_, end);
      for (i = 0; i < num_args; ++i)
        CopyTraceEventParameter(&ptr, &arg_names_[i], end);
    }
    for (i = 0; i < num_args; ++i) {
      if (arg_is_copy[i])
        CopyTraceEventParameter(&ptr, &arg_values_[i].as_string, end);
    }
    DCHECK_EQ(end, ptr) << "Overrun by " << ptr - end;
  }
}

TraceEvent::~TraceEvent() {
}

// static
void TraceEvent::AppendValueAsJSON(unsigned char type,
                                   TraceEvent::TraceValue value,
                                   std::string* out) {
  std::string::size_type start_pos;
  switch (type) {
    case TRACE_VALUE_TYPE_BOOL:
      *out += value.as_bool ? "true" : "false";
      break;
    case TRACE_VALUE_TYPE_UINT:
      StringAppendF(out, "%" PRIu64, static_cast<uint64>(value.as_uint));
      break;
    case TRACE_VALUE_TYPE_INT:
      StringAppendF(out, "%" PRId64, static_cast<int64>(value.as_int));
      break;
    case TRACE_VALUE_TYPE_DOUBLE:
      StringAppendF(out, "%f", value.as_double);
      break;
    case TRACE_VALUE_TYPE_POINTER:
      // JSON only supports double and int numbers.
      // So as not to lose bits from a 64-bit pointer, output as a hex string.
      StringAppendF(out, "\"%" PRIx64 "\"", static_cast<uint64>(
                                     reinterpret_cast<intptr_t>(
                                     value.as_pointer)));
      break;
    case TRACE_VALUE_TYPE_STRING:
    case TRACE_VALUE_TYPE_COPY_STRING:
      *out += "\"";
      start_pos = out->size();
      *out += value.as_string ? value.as_string : "NULL";
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
  int64 time_int64 = timestamp_.ToInternalValue();
  int process_id = TraceLog::GetInstance()->process_id();
  // Category name checked at category creation time.
  DCHECK(!strchr(name_, '"'));
  StringAppendF(out,
      "{\"cat\":\"%s\",\"pid\":%i,\"tid\":%i,\"ts\":%" PRId64 ","
      "\"ph\":\"%c\",\"name\":\"%s\",\"args\":{",
      TraceLog::GetCategoryName(category_enabled_),
      process_id,
      thread_id_,
      time_int64,
      phase_,
      name_);

  // Output argument names and values, stop at first NULL argument name.
  for (int i = 0; i < kTraceMaxNumArgs && arg_names_[i]; ++i) {
    if (i > 0)
      *out += ",";
    *out += "\"";
    *out += arg_names_[i];
    *out += "\":";
    AppendValueAsJSON(arg_types_[i], arg_values_[i], out);
  }
  *out += "}";

  // If id_ is set, print it out as a hex string so we don't loose any
  // bits (it might be a 64-bit pointer).
  if (flags_ & TRACE_EVENT_FLAG_HAS_ID)
    StringAppendF(out, ",\"id\":\"%" PRIx64 "\"", static_cast<uint64>(id_));
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

void TraceResultBuffer::SetOutputCallback(
    const OutputCallback& json_chunk_callback) {
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

TraceLog::NotificationHelper::NotificationHelper(TraceLog* trace_log)
    : trace_log_(trace_log),
      notification_(0) {
}

TraceLog::NotificationHelper::~NotificationHelper() {
}

void TraceLog::NotificationHelper::AddNotificationWhileLocked(
    int notification) {
  if (trace_log_->notification_callback_.is_null())
    return;
  if (notification_ == 0)
    callback_copy_ = trace_log_->notification_callback_;
  notification_ |= notification;
}

void TraceLog::NotificationHelper::SendNotificationIfAny() {
  if (notification_)
    callback_copy_.Run(notification_);
}

// static
TraceLog* TraceLog::GetInstance() {
  return Singleton<TraceLog, StaticMemorySingletonTraits<TraceLog> >::get();
}

TraceLog::TraceLog()
    : enabled_(false),
      dispatching_to_observer_list_(false),
      watch_category_(NULL) {
  // Trace is enabled or disabled on one thread while other threads are
  // accessing the enabled flag. We don't care whether edge-case events are
  // traced or not, so we allow races on the enabled flag to keep the trace
  // macros fast.
  // TODO(jbates): ANNOTATE_BENIGN_RACE_SIZED crashes windows TSAN bots:
  // ANNOTATE_BENIGN_RACE_SIZED(g_category_enabled, sizeof(g_category_enabled),
  //                            "trace_event category enabled");
  for (int i = 0; i < TRACE_EVENT_MAX_CATEGORIES; ++i) {
    ANNOTATE_BENIGN_RACE(&g_category_enabled[i],
                         "trace_event category enabled");
  }
#if defined(OS_NACL)  // NaCl shouldn't expose the process id.
  SetProcessID(0);
#else
  SetProcessID(static_cast<int>(base::GetCurrentProcId()));
#endif
}

TraceLog::~TraceLog() {
}

const unsigned char* TraceLog::GetCategoryEnabled(const char* name) {
  TraceLog* tracelog = GetInstance();
  if (!tracelog) {
    DCHECK(!g_category_enabled[g_category_already_shutdown]);
    return &g_category_enabled[g_category_already_shutdown];
  }
  return tracelog->GetCategoryEnabledInternal(name);
}

const char* TraceLog::GetCategoryName(const unsigned char* category_enabled) {
  // Calculate the index of the category by finding category_enabled in
  // g_category_enabled array.
  uintptr_t category_begin = reinterpret_cast<uintptr_t>(g_category_enabled);
  uintptr_t category_ptr = reinterpret_cast<uintptr_t>(category_enabled);
  DCHECK(category_ptr >= category_begin &&
         category_ptr < reinterpret_cast<uintptr_t>(g_category_enabled +
                                               TRACE_EVENT_MAX_CATEGORIES)) <<
      "out of bounds category pointer";
  uintptr_t category_index =
      (category_ptr - category_begin) / sizeof(g_category_enabled[0]);
  return g_categories[category_index];
}

static void EnableMatchingCategory(int category_index,
                                   const std::vector<std::string>& patterns,
                                   unsigned char matched_value,
                                   unsigned char unmatched_value) {
  std::vector<std::string>::const_iterator ci = patterns.begin();
  bool is_match = false;
  for (; ci != patterns.end(); ++ci) {
    is_match = MatchPattern(g_categories[category_index], ci->c_str());
    if (is_match)
      break;
  }
  g_category_enabled[category_index] = is_match ?
      matched_value : unmatched_value;
}

// Enable/disable each category based on the category filters in |patterns|.
// If the category name matches one of the patterns, its enabled status is set
// to |matched_value|. Otherwise its enabled status is set to |unmatched_value|.
static void EnableMatchingCategories(const std::vector<std::string>& patterns,
                                     unsigned char matched_value,
                                     unsigned char unmatched_value) {
  for (int i = 0; i < g_category_index; i++)
    EnableMatchingCategory(i, patterns, matched_value, unmatched_value);
}

const unsigned char* TraceLog::GetCategoryEnabledInternal(const char* name) {
  AutoLock lock(lock_);
  DCHECK(!strchr(name, '"')) << "Category names may not contain double quote";

  unsigned char* category_enabled = NULL;
  // Search for pre-existing category matching this name
  for (int i = 0; i < g_category_index; i++) {
    if (strcmp(g_categories[i], name) == 0) {
      category_enabled = &g_category_enabled[i];
      break;
    }
  }

  if (!category_enabled) {
    // Create a new category
    DCHECK(g_category_index < TRACE_EVENT_MAX_CATEGORIES) <<
        "must increase TRACE_EVENT_MAX_CATEGORIES";
    if (g_category_index < TRACE_EVENT_MAX_CATEGORIES) {
      int new_index = g_category_index++;
      // Don't hold on to the name pointer, so that we can create categories
      // with strings not known at compile time (this is required by
      // SetWatchEvent).
      const char* new_name = base::strdup(name);
      ANNOTATE_LEAKING_OBJECT_PTR(new_name);
      g_categories[new_index] = new_name;
      DCHECK(!g_category_enabled[new_index]);
      if (enabled_) {
        // Note that if both included and excluded_categories are empty, the
        // else clause below excludes nothing, thereby enabling this category.
        if (!included_categories_.empty()) {
          EnableMatchingCategory(new_index, included_categories_,
                                 CATEGORY_ENABLED, 0);
        } else {
          EnableMatchingCategory(new_index, excluded_categories_,
                                 0, CATEGORY_ENABLED);
        }
      } else {
        g_category_enabled[new_index] = 0;
      }
      category_enabled = &g_category_enabled[new_index];
    } else {
      category_enabled = &g_category_enabled[g_category_categories_exhausted];
    }
  }
#if defined(OS_ANDROID)
  ApplyATraceEnabledFlag(category_enabled);
#endif
  return category_enabled;
}

void TraceLog::GetKnownCategories(std::vector<std::string>* categories) {
  AutoLock lock(lock_);
  for (int i = 0; i < g_category_index; i++)
    categories->push_back(g_categories[i]);
}

void TraceLog::SetEnabled(const std::vector<std::string>& included_categories,
                          const std::vector<std::string>& excluded_categories) {
  AutoLock lock(lock_);
  if (enabled_)
    return;

  if (dispatching_to_observer_list_) {
    DLOG(ERROR) <<
        "Cannot manipulate TraceLog::Enabled state from an observer.";
    return;
  }

  dispatching_to_observer_list_ = true;
  FOR_EACH_OBSERVER(EnabledStateChangedObserver, enabled_state_observer_list_,
                    OnTraceLogWillEnable());
  dispatching_to_observer_list_ = false;

  logged_events_.reserve(1024);
  enabled_ = true;
  included_categories_ = included_categories;
  excluded_categories_ = excluded_categories;
  // Note that if both included and excluded_categories are empty, the else
  // clause below excludes nothing, thereby enabling all categories.
  if (!included_categories_.empty())
    EnableMatchingCategories(included_categories_, CATEGORY_ENABLED, 0);
  else
    EnableMatchingCategories(excluded_categories_, 0, CATEGORY_ENABLED);
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
  AutoLock lock(lock_);
  if (!enabled_)
    return;

  if (dispatching_to_observer_list_) {
    DLOG(ERROR)
        << "Cannot manipulate TraceLog::Enabled state from an observer.";
    return;
  }

  dispatching_to_observer_list_ = true;
  FOR_EACH_OBSERVER(EnabledStateChangedObserver, enabled_state_observer_list_,
                    OnTraceLogWillDisable());
  dispatching_to_observer_list_ = false;

  enabled_ = false;
  included_categories_.clear();
  excluded_categories_.clear();
  watch_category_ = NULL;
  watch_event_name_ = "";
  for (int i = 0; i < g_category_index; i++)
    g_category_enabled[i] = 0;
  AddThreadNameMetadataEvents();
#if defined(OS_ANDROID)
  AddClockSyncMetadataEvents();
#endif
}

void TraceLog::SetEnabled(bool enabled) {
  if (enabled)
    SetEnabled(std::vector<std::string>(), std::vector<std::string>());
  else
    SetDisabled();
}

void TraceLog::AddEnabledStateObserver(EnabledStateChangedObserver* listener) {
  enabled_state_observer_list_.AddObserver(listener);
}

void TraceLog::RemoveEnabledStateObserver(
    EnabledStateChangedObserver* listener) {
  enabled_state_observer_list_.RemoveObserver(listener);
}

float TraceLog::GetBufferPercentFull() const {
  return (float)((double)logged_events_.size()/(double)kTraceEventBufferSize);
}

void TraceLog::SetNotificationCallback(
    const TraceLog::NotificationCallback& cb) {
  AutoLock lock(lock_);
  notification_callback_ = cb;
}

void TraceLog::Flush(const TraceLog::OutputCallback& cb) {
  std::vector<TraceEvent> previous_logged_events;
  {
    AutoLock lock(lock_);
    previous_logged_events.swap(logged_events_);
  }  // release lock

  for (size_t i = 0;
       i < previous_logged_events.size();
       i += kTraceEventBatchSize) {
    scoped_refptr<RefCountedString> json_events_str_ptr =
        new RefCountedString();
    TraceEvent::AppendEventsAsJSON(previous_logged_events,
                                   i,
                                   kTraceEventBatchSize,
                                   &(json_events_str_ptr->data()));
    cb.Run(json_events_str_ptr);
  }
}

void TraceLog::AddTraceEvent(char phase,
                            const unsigned char* category_enabled,
                            const char* name,
                            unsigned long long id,
                            int num_args,
                            const char** arg_names,
                            const unsigned char* arg_types,
                            const unsigned long long* arg_values,
                            unsigned char flags) {
  DCHECK(name);

#if defined(OS_ANDROID)
  SendToATrace(phase, GetCategoryName(category_enabled), name,
               num_args, arg_names, arg_types, arg_values);
#endif

  TimeTicks now = TimeTicks::NowFromSystemTraceTime() - time_offset_;
  NotificationHelper notifier(this);
  {
    AutoLock lock(lock_);
    if (*category_enabled != CATEGORY_ENABLED)
      return;
    if (logged_events_.size() >= kTraceEventBufferSize)
      return;

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

    if (flags & TRACE_EVENT_FLAG_MANGLE_ID)
      id ^= process_id_hash_;

    logged_events_.push_back(
        TraceEvent(thread_id,
                   now, phase, category_enabled, name, id,
                   num_args, arg_names, arg_types, arg_values,
                   flags));

    if (logged_events_.size() == kTraceEventBufferSize)
      notifier.AddNotificationWhileLocked(TRACE_BUFFER_FULL);

    if (watch_category_ == category_enabled && watch_event_name_ == name)
      notifier.AddNotificationWhileLocked(EVENT_WATCH_NOTIFICATION);
  }  // release lock

  notifier.SendNotificationIfAny();
}

void TraceLog::AddTraceEventEtw(char phase,
                                const char* name,
                                const void* id,
                                const char* extra) {
#if defined(OS_WIN)
  TraceEventETWProvider::Trace(name, phase, id, extra);
#endif
  INTERNAL_TRACE_EVENT_ADD(phase, "ETW Trace Event", name,
                           TRACE_EVENT_FLAG_COPY, "id", id, "extra", extra);
}

void TraceLog::AddTraceEventEtw(char phase,
                                const char* name,
                                const void* id,
                                const std::string& extra)
{
#if defined(OS_WIN)
  TraceEventETWProvider::Trace(name, phase, id, extra);
#endif
  INTERNAL_TRACE_EVENT_ADD(phase, "ETW Trace Event", name,
                           TRACE_EVENT_FLAG_COPY, "id", id, "extra", extra);
}

void TraceLog::SetWatchEvent(const std::string& category_name,
                             const std::string& event_name) {
  const unsigned char* category = GetCategoryEnabled(category_name.c_str());
  int notify_count = 0;
  {
    AutoLock lock(lock_);
    watch_category_ = category;
    watch_event_name_ = event_name;

    // First, search existing events for watch event because we want to catch it
    // even if it has already occurred.
    for (size_t i = 0u; i < logged_events_.size(); ++i) {
      if (category == logged_events_[i].category_enabled() &&
          strcmp(event_name.c_str(), logged_events_[i].name()) == 0) {
        ++notify_count;
      }
    }
  }  // release lock

  // Send notification for each event found.
  for (int i = 0; i < notify_count; ++i) {
    NotificationHelper notifier(this);
    lock_.Acquire();
    notifier.AddNotificationWhileLocked(EVENT_WATCH_NOTIFICATION);
    lock_.Release();
    notifier.SendNotificationIfAny();
  }
}

void TraceLog::CancelWatchEvent() {
  AutoLock lock(lock_);
  watch_category_ = NULL;
  watch_event_name_ = "";
}

void TraceLog::AddThreadNameMetadataEvents() {
  lock_.AssertAcquired();
  for(base::hash_map<int, std::string>::iterator it = thread_names_.begin();
      it != thread_names_.end();
      it++) {
    if (!it->second.empty()) {
      int num_args = 1;
      const char* arg_name = "name";
      unsigned char arg_type;
      unsigned long long arg_value;
      trace_event_internal::SetTraceValue(it->second, &arg_type, &arg_value);
      logged_events_.push_back(
          TraceEvent(it->first,
                     TimeTicks(), TRACE_EVENT_PHASE_METADATA,
                     &g_category_enabled[g_category_metadata],
                     "thread_name", trace_event_internal::kNoEventId,
                     num_args, &arg_name, &arg_type, &arg_value,
                     TRACE_EVENT_FLAG_NONE));
    }
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
  unsigned long long offset_basis = 14695981039346656037ull;
  unsigned long long fnv_prime = 1099511628211ull;
  unsigned long long pid = static_cast<unsigned long long>(process_id_);
  process_id_hash_ = (offset_basis ^ pid) * fnv_prime;
}

void TraceLog::SetTimeOffset(TimeDelta offset) {
  time_offset_ = offset;
}

}  // namespace debug
}  // namespace base

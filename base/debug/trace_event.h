// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Trace events are for tracking application performance and resource usage.
// Macros are provided to track:
//    Begin and end of function calls
//    Counters
//
// Events are issued against categories. Whereas LOG's
// categories are statically defined, TRACE categories are created
// implicitly with a string. For example:
//   TRACE_EVENT_INSTANT0("MY_SUBSYSTEM", "SomeImportantEvent")
//
// Events can be INSTANT, or can be pairs of BEGIN and END:
//   TRACE_EVENT_BEGIN0("MY_SUBSYSTEM", "SomethingCostly")
//   doSomethingCostly()
//   TRACE_EVENT_END0("MY_SUBSYSTEM", "SomethingCostly")
//
// A common use case is to trace entire function scopes. This
// issues a trace BEGIN and END automatically:
//   void doSomethingCostly() {
//     TRACE_EVENT0("MY_SUBSYSTEM", "doSomethingCostly");
//     ...
//   }
//
// Additional parameters can be associated with an event:
//   void doSomethingCostly2(int howMuch) {
//     TRACE_EVENT1("MY_SUBSYSTEM", "doSomethingCostly",
//         "howMuch", howMuch);
//     ...
//   }
//
// The trace system will automatically add to this information the
// current process id, thread id, and a timestamp in microseconds.
//
// Trace event also supports counters, which is a way to track a quantity
// as it varies over time. Counters are created with the following macro:
//   TRACE_COUNTER1("MY_SUBSYSTEM", "myCounter", g_myCounterValue);
//
// Counters are process-specific. The macro itself can be issued from any
// thread, however.
//
// Sometimes, you want to track two counters at once. You can do this with two
// counter macros:
//   TRACE_COUNTER1("MY_SUBSYSTEM", "myCounter0", g_myCounterValue[0]);
//   TRACE_COUNTER1("MY_SUBSYSTEM", "myCounter1", g_myCounterValue[1]);
// Or you can do it with a combined macro:
//   TRACE_COUNTER2("MY_SUBSYSTEM", "myCounter",
//       "bytesPinned", g_myCounterValue[0],
//       "bytesAllocated", g_myCounterValue[1]);
// This indicates to the tracing UI that these counters should be displayed
// in a single graph, as a summed area chart.
//
// By default, trace collection is compiled in, but turned off at runtime.
// Collecting trace data is the responsibility of the embedding
// application. In Chrome's case, navigating to about:tracing will turn on
// tracing and display data collected across all active processes.
//
//
// Memory scoping note:
// Tracing copies the pointers, not the string content, of the strings passed
// in for category, name, and arg_names.  Thus, the following code will
// cause problems:
//     char* str = strdup("impprtantName");
//     TRACE_EVENT_INSTANT0("SUBSYSTEM", str);  // BAD!
//     free(str);                   // Trace system now has dangling pointer
//
// To avoid this issue with the |name| and |arg_name| parameters, use the
// TRACE_EVENT_COPY_XXX overloads of the macros at additional runtime overhead.
// Notes: The category must always be in a long-lived char* (i.e. static const).
//        The |arg_values|, when used, are always deep copied with the _COPY
//        macros.
//
// When are string argument values copied:
// const char* arg_values are only referenced by default:
//     TRACE_EVENT1("category", "name",
//                  "arg1", "literal string is only referenced");
// Use TRACE_STR_COPY to force copying of a const char*:
//     TRACE_EVENT1("category", "name",
//                  "arg1", TRACE_STR_COPY("string will be copied"));
// std::string arg_values are always copied:
//     TRACE_EVENT1("category", "name",
//                  "arg1", std::string("string will be copied"));
//
//
// Thread Safety:
// A thread safe singleton and mutex are used for thread safety. Category
// enabled flags are used to limit the performance impact when the system
// is not enabled.
//
// TRACE_EVENT macros first cache a pointer to a category. The categories are
// statically allocated and safe at all times, even after exit. Fetching a
// category is protected by the TraceLog::lock_. Multiple threads initializing
// the static variable is safe, as they will be serialized by the lock and
// multiple calls will return the same pointer to the category.
//
// Then the category.enabled flag is checked. This is a volatile bool, and
// not intended to be multithread safe. It optimizes access to AddTraceEvent
// which is threadsafe internally via TraceLog::lock_. The enabled flag may
// cause some threads to incorrectly call or skip calling AddTraceEvent near
// the time of the system being enabled or disabled. This is acceptable as
// we tolerate some data loss while the system is being enabled/disabled and
// because AddTraceEvent is threadsafe internally and checks the enabled state
// again under lock.
//
// Without the use of these static category pointers and enabled flags all
// trace points would carry a significant performance cost of aquiring a lock
// and resolving the category.
//
// ANNOTATE_BENIGN_RACE is used to suppress the warning on the static category
// pointers.


#ifndef BASE_DEBUG_TRACE_EVENT_H_
#define BASE_DEBUG_TRACE_EVENT_H_
#pragma once

#include "build/build_config.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/timer.h"

// By default, const char* argument values are assumed to have long-lived scope
// and will not be copied. Use this macro to force a const char* to be copied.
#define TRACE_STR_COPY(str) base::debug::TraceValue::StringWithCopy(str)

// Older style trace macros with explicit id and extra data
// Only these macros result in publishing data to ETW as currently implemented.
#define TRACE_EVENT_BEGIN_ETW(name, id, extra) \
    base::debug::TraceLog::AddTraceEventEtw( \
        base::debug::TRACE_EVENT_PHASE_BEGIN, \
        name, reinterpret_cast<const void*>(id), extra)

#define TRACE_EVENT_END_ETW(name, id, extra) \
    base::debug::TraceLog::AddTraceEventEtw( \
        base::debug::TRACE_EVENT_PHASE_END, \
        name, reinterpret_cast<const void*>(id), extra)

#define TRACE_EVENT_INSTANT_ETW(name, id, extra) \
    base::debug::TraceLog::AddTraceEventEtw( \
        base::debug::TRACE_EVENT_PHASE_INSTANT, \
        name, reinterpret_cast<const void*>(id), extra)

// Records a pair of begin and end events called "name" for the current
// scope, with 0, 1 or 2 associated arguments. If the category is not
// enabled, then this does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT0(category, name) \
    TRACE_EVENT1(category, name, NULL, 0)
#define TRACE_EVENT1(category, name, arg1_name, arg1_val) \
    TRACE_EVENT2(category, name, arg1_name, arg1_val, NULL, 0)
#define TRACE_EVENT2(category, name, arg1_name, arg1_val, arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_SCOPED( \
        category, name, arg1_name, arg1_val, arg2_name, arg2_val)

// Same as TRACE_EVENT except that they are not included in official builds.
#ifdef OFFICIAL_BUILD
#define UNSHIPPED_TRACE_EVENT0(category, name) (void)0
#define UNSHIPPED_TRACE_EVENT1(category, name, arg1_name, arg1_val) (void)0
#define UNSHIPPED_TRACE_EVENT2(category, name, arg1_name, arg1_val, \
                               arg2_name, arg2_val) (void)0
#else
#define UNSHIPPED_TRACE_EVENT0(category, name) \
    TRACE_EVENT0(category, name)
#define UNSHIPPED_TRACE_EVENT1(category, name, arg1_name, arg1_val) \
    TRACE_EVENT1(category, name, arg1_name, arg1_val)
#define UNSHIPPED_TRACE_EVENT2(category, name, arg1_name, arg1_val, \
                               arg2_name, arg2_val) \
    TRACE_EVENT2(category, name, arg1_name, arg1_val, arg2_name, arg2_val)
#endif

// Records a single event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_INSTANT0(category, name) \
    TRACE_EVENT_INSTANT1(category, name, NULL, 0)
#define TRACE_EVENT_INSTANT1(category, name, arg1_name, arg1_val) \
    TRACE_EVENT_INSTANT2(category, name, arg1_name, arg1_val, NULL, 0)
#define TRACE_EVENT_INSTANT2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(base::debug::TRACE_EVENT_PHASE_INSTANT, \
        category, name, arg1_name, arg1_val, arg2_name, arg2_val, \
        base::debug::TraceLog::EVENT_FLAG_NONE)
#define TRACE_EVENT_COPY_INSTANT0(category, name) \
    TRACE_EVENT_COPY_INSTANT1(category, name, NULL, 0)
#define TRACE_EVENT_COPY_INSTANT1(category, name, arg1_name, arg1_val) \
    TRACE_EVENT_COPY_INSTANT2(category, name, arg1_name, arg1_val, NULL, 0)
#define TRACE_EVENT_COPY_INSTANT2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(base::debug::TRACE_EVENT_PHASE_INSTANT, \
        category, name, \
        arg1_name, base::debug::TraceValue::ForceCopy(arg1_val), \
        arg2_name, base::debug::TraceValue::ForceCopy(arg2_val), \
        base::debug::TraceLog::EVENT_FLAG_COPY)

// Records a single BEGIN event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_BEGIN0(category, name) \
    TRACE_EVENT_BEGIN1(category, name, NULL, 0)
#define TRACE_EVENT_BEGIN1(category, name, arg1_name, arg1_val) \
    TRACE_EVENT_BEGIN2(category, name, arg1_name, arg1_val, NULL, 0)
#define TRACE_EVENT_BEGIN2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(base::debug::TRACE_EVENT_PHASE_BEGIN, \
        category, name, arg1_name, arg1_val, arg2_name, arg2_val, \
        base::debug::TraceLog::EVENT_FLAG_NONE)
#define TRACE_EVENT_COPY_BEGIN0(category, name) \
    TRACE_EVENT_COPY_BEGIN1(category, name, NULL, 0)
#define TRACE_EVENT_COPY_BEGIN1(category, name, arg1_name, arg1_val) \
    TRACE_EVENT_COPY_BEGIN2(category, name, arg1_name, arg1_val, NULL, 0)
#define TRACE_EVENT_COPY_BEGIN2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(base::debug::TRACE_EVENT_PHASE_BEGIN, \
        category, name, \
        arg1_name, base::debug::TraceValue::ForceCopy(arg1_val), \
        arg2_name, base::debug::TraceValue::ForceCopy(arg2_val), \
        base::debug::TraceLog::EVENT_FLAG_COPY)

// Records a single END event for "name" immediately. If the category
// is not enabled, then this does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_END0(category, name) \
    TRACE_EVENT_END1(category, name, NULL, 0)
#define TRACE_EVENT_END1(category, name, arg1_name, arg1_val) \
    TRACE_EVENT_END2(category, name, arg1_name, arg1_val, NULL, 0)
#define TRACE_EVENT_END2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(base::debug::TRACE_EVENT_PHASE_END, \
        category, name, arg1_name, arg1_val, arg2_name, arg2_val, \
        base::debug::TraceLog::EVENT_FLAG_NONE)
#define TRACE_EVENT_COPY_END0(category, name) \
    TRACE_EVENT_COPY_END1(category, name, NULL, 0)
#define TRACE_EVENT_COPY_END1(category, name, arg1_name, arg1_val) \
    TRACE_EVENT_COPY_END2(category, name, arg1_name, arg1_val, NULL, 0)
#define TRACE_EVENT_COPY_END2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(base::debug::TRACE_EVENT_PHASE_END, \
        category, name, \
        arg1_name, base::debug::TraceValue::ForceCopy(arg1_val), \
        arg2_name, base::debug::TraceValue::ForceCopy(arg2_val), \
        base::debug::TraceLog::EVENT_FLAG_COPY)

// Time threshold event:
// Only record the event if the duration is greater than the specified
// threshold_us (time in microseconds).
// Records a pair of begin and end events called "name" for the current
// scope, with 0, 1 or 2 associated arguments. If the category is not
// enabled, then this does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_IF_LONGER_THAN0(threshold_us, category, name) \
    TRACE_EVENT_IF_LONGER_THAN1(threshold_us, category, name, NULL, 0)
#define TRACE_EVENT_IF_LONGER_THAN1( \
        threshold_us, category, name, arg1_name, arg1_val) \
    TRACE_EVENT_IF_LONGER_THAN2(threshold_us, category, name, \
                           arg1_name, arg1_val, NULL, 0)
#define TRACE_EVENT_IF_LONGER_THAN2( \
    threshold_us, category, name, arg1_name, arg1_val, arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_SCOPED_IF_LONGER_THAN(threshold_us, \
        category, name, arg1_name, arg1_val, arg2_name, arg2_val)

// Records the value of a counter called "name" immediately. Value
// must be representable as a 32 bit integer.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_COUNTER1(category, name, value) \
    TRACE_COUNTER2(category, name, "value", value, NULL, 0)
#define TRACE_COPY_COUNTER1(category, name, value) \
    TRACE_COPY_COUNTER2(category, name, "value", value, NULL, 0)

// Records the values of a multi-parted counter called "name" immediately.
// The UI will treat value1 and value2 as parts of a whole, displaying their
// values as a stacked-bar chart.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_COUNTER2(category, name, value1_name, value1_val, \
        value2_name, value2_val) \
    INTERNAL_TRACE_EVENT_ADD_COUNTER( \
        category, name, value1_name, value1_val, value2_name, value2_val, \
        base::debug::TraceLog::EVENT_FLAG_NONE)
#define TRACE_COPY_COUNTER2(category, name, value1_name, value1_val, \
        value2_name, value2_val) \
    INTERNAL_TRACE_EVENT_ADD_COUNTER( \
        category, name, \
        value1_name, value1_val, \
        value2_name, value2_val, \
        base::debug::TraceLog::EVENT_FLAG_COPY)


// Implementation detail: trace event macros create temporary variables
// to keep instrumentation overhead low. These macros give each temporary
// variable a unique name based on the line number to prevent name collissions.
#define INTERNAL_TRACE_EVENT_UID3(a,b) \
    trace_event_unique_##a##b
#define INTERNAL_TRACE_EVENT_UID2(a,b) \
    INTERNAL_TRACE_EVENT_UID3(a,b)
#define INTERNAL_TRACE_EVENT_UID(name_prefix) \
    INTERNAL_TRACE_EVENT_UID2(name_prefix, __LINE__)

// Implementation detail: internal macro to create static category.
// - ANNOTATE_BENIGN_RACE, see Thread Safety above.
#define INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category) \
    static const base::debug::TraceCategory* \
        INTERNAL_TRACE_EVENT_UID(catstatic) = NULL; \
    ANNOTATE_BENIGN_RACE(&INTERNAL_TRACE_EVENT_UID(catstatic), \
                         "trace_event category"); \
    if (!INTERNAL_TRACE_EVENT_UID(catstatic)) \
      INTERNAL_TRACE_EVENT_UID(catstatic) = \
          base::debug::TraceLog::GetCategory(category);

// Implementation detail: internal macro to create static category and add begin
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD( \
      phase, category, name, arg1_name, arg1_val, arg2_name, arg2_val, flags) \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
    if (INTERNAL_TRACE_EVENT_UID(catstatic)->enabled) { \
      base::debug::TraceLog::GetInstance()->AddTraceEvent( \
          phase, INTERNAL_TRACE_EVENT_UID(catstatic), \
          name, arg1_name, arg1_val, arg2_name, arg2_val, -1, 0, flags); \
    }

// Implementation detail: internal macro to create static category and
// add the counter event if it is enabled.
#define INTERNAL_TRACE_EVENT_ADD_COUNTER( \
      category, name, arg1_name, arg1_val, arg2_name, arg2_val, flags) \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
    if (INTERNAL_TRACE_EVENT_UID(catstatic)->enabled) { \
      base::debug::TraceLog::GetInstance()->AddCounterEvent( \
          INTERNAL_TRACE_EVENT_UID(catstatic), \
          name, arg1_name, arg1_val, arg2_name, arg2_val, flags); \
    }

// Implementation detail: internal macro to create static category and add begin
// event if the category is enabled. Also adds the end event when the scope
// ends.
#define INTERNAL_TRACE_EVENT_ADD_SCOPED( \
      category, name, arg1_name, arg1_val, arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
    base::debug::internal::TraceEndOnScopeClose \
        INTERNAL_TRACE_EVENT_UID(profileScope); \
    if (INTERNAL_TRACE_EVENT_UID(catstatic)->enabled) { \
      base::debug::TraceLog::GetInstance()->AddTraceEvent( \
          base::debug::TRACE_EVENT_PHASE_BEGIN, \
          INTERNAL_TRACE_EVENT_UID(catstatic), \
          name, arg1_name, arg1_val, arg2_name, arg2_val, -1, 0, \
          base::debug::TraceLog::EVENT_FLAG_NONE); \
      INTERNAL_TRACE_EVENT_UID(profileScope).Initialize( \
          INTERNAL_TRACE_EVENT_UID(catstatic), name); \
    }

// Implementation detail: internal macro to create static category and add begin
// event if the category is enabled. Also adds the end event when the scope
// ends. If the elapsed time is < threshold time, the begin/end pair is erased.
#define INTERNAL_TRACE_EVENT_ADD_SCOPED_IF_LONGER_THAN(threshold, \
      category, name, arg1_name, arg1_val, arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
    base::debug::internal::TraceEndOnScopeCloseThreshold \
        INTERNAL_TRACE_EVENT_UID(profileScope); \
    if (INTERNAL_TRACE_EVENT_UID(catstatic)->enabled) { \
      int INTERNAL_TRACE_EVENT_UID(begin_event_id) = \
        base::debug::TraceLog::GetInstance()->AddTraceEvent( \
            base::debug::TRACE_EVENT_PHASE_BEGIN, \
            INTERNAL_TRACE_EVENT_UID(catstatic), \
            name, arg1_name, arg1_val, arg2_name, arg2_val, -1, 0, \
            base::debug::TraceLog::EVENT_FLAG_NONE); \
      INTERNAL_TRACE_EVENT_UID(profileScope).Initialize( \
          INTERNAL_TRACE_EVENT_UID(catstatic), name, \
          INTERNAL_TRACE_EVENT_UID(begin_event_id), threshold); \
    }

template <typename Type>
struct StaticMemorySingletonTraits;

namespace base {

class RefCountedString;

namespace debug {

// Categories allow enabling/disabling of streams of trace events
struct TraceCategory {
  const char* name;
  volatile bool enabled;
};

const size_t kTraceMaxNumArgs = 2;

// Phase indicates the nature of an event entry. E.g. part of a begin/end pair.
enum TraceEventPhase {
  TRACE_EVENT_PHASE_BEGIN,
  TRACE_EVENT_PHASE_END,
  TRACE_EVENT_PHASE_INSTANT,
  TRACE_EVENT_PHASE_METADATA,
  TRACE_EVENT_PHASE_COUNTER
};

// Simple union of values. This is much lighter weight than base::Value, which
// requires dynamic allocation and a vtable. To keep the trace runtime overhead
// low, we want constant size storage here.
class BASE_EXPORT TraceValue {
 public:
  enum Type {
    TRACE_TYPE_UNDEFINED,
    TRACE_TYPE_BOOL,
    TRACE_TYPE_UINT,
    TRACE_TYPE_INT,
    TRACE_TYPE_DOUBLE,
    TRACE_TYPE_POINTER,
    TRACE_TYPE_STRING,
    TRACE_TYPE_STATIC_STRING
  };

  TraceValue() : type_(TRACE_TYPE_UNDEFINED) {
    value_.as_uint = 0ull;
  }
  TraceValue(bool rhs) : type_(TRACE_TYPE_BOOL) {
    value_.as_bool = rhs;
  }
  TraceValue(uint64 rhs) : type_(TRACE_TYPE_UINT) {
    value_.as_uint = rhs;
  }
  TraceValue(uint32 rhs) : type_(TRACE_TYPE_UINT) {
    value_.as_uint = rhs;
  }
  TraceValue(uint16 rhs) : type_(TRACE_TYPE_UINT) {
    value_.as_uint = rhs;
  }
  TraceValue(uint8 rhs) : type_(TRACE_TYPE_UINT) {
    value_.as_uint = rhs;
  }
  TraceValue(int64 rhs) : type_(TRACE_TYPE_INT) {
    value_.as_int = rhs;
  }
  TraceValue(int32 rhs) : type_(TRACE_TYPE_INT) {
    value_.as_int = rhs;
  }
  TraceValue(int16 rhs) : type_(TRACE_TYPE_INT) {
    value_.as_int = rhs;
  }
  TraceValue(int8 rhs) : type_(TRACE_TYPE_INT) {
    value_.as_int = rhs;
  }
  TraceValue(double rhs) : type_(TRACE_TYPE_DOUBLE) {
    value_.as_double = rhs;
  }
  TraceValue(const void* rhs) : type_(TRACE_TYPE_POINTER) {
    value_.as_pointer = rhs;
  }
  TraceValue(const std::string& rhs) : type_(TRACE_TYPE_STRING) {
    value_.as_string = rhs.c_str();
  }
  TraceValue(const char* rhs) : type_(TRACE_TYPE_STATIC_STRING) {
    value_.as_string = rhs;
  }

  static TraceValue StringWithCopy(const char* rhs) {
    TraceValue value(rhs);
    if (rhs)
      value.type_ = TRACE_TYPE_STRING;
    return value;
  }

  static TraceValue ForceCopy(const TraceValue& rhs) {
    TraceValue value(rhs);
    if (value.type_ == TRACE_TYPE_STATIC_STRING && value.as_string())
      value.type_ = TRACE_TYPE_STRING;
    return value;
  }

  bool is_string() const {
    return type_ == TRACE_TYPE_STRING || type_ == TRACE_TYPE_STATIC_STRING;
  }

  void AppendAsJSON(std::string* out) const;

  Type type() const {
    return type_;
  }
  uint64 as_uint() const {
    DCHECK_EQ(TRACE_TYPE_UINT, type_);
    return value_.as_uint;
  }
  bool as_bool() const {
    DCHECK_EQ(TRACE_TYPE_BOOL, type_);
    return value_.as_bool;
  }
  int64 as_int() const {
    DCHECK_EQ(TRACE_TYPE_INT, type_);
    return value_.as_int;
  }
  double as_double() const {
    DCHECK_EQ(TRACE_TYPE_DOUBLE, type_);
    return value_.as_double;
  }
  const void* as_pointer() const {
    DCHECK_EQ(TRACE_TYPE_POINTER, type_);
    return value_.as_pointer;
  }
  const char* as_string() const {
    DCHECK(is_string());
    return value_.as_string;
  }
  const char** as_assignable_string() {
    DCHECK_EQ(TRACE_TYPE_STRING, type_);
    return &value_.as_string;
  }

 private:
  union Value {
    bool as_bool;
    uint64 as_uint;
    int64 as_int;
    double as_double;
    const void* as_pointer;
    const char* as_string;
  };

  Type type_;
  Value value_;
};

// Output records are "Events" and can be obtained via the
// OutputCallback whenever the tracing system decides to flush. This
// can happen at any time, on any thread, or you can programatically
// force it to happen.
class BASE_EXPORT TraceEvent {
 public:
  TraceEvent();
  TraceEvent(unsigned long process_id,
             unsigned long thread_id,
             TimeTicks timestamp,
             TraceEventPhase phase,
             const TraceCategory* category,
             const char* name,
             const char* arg1_name, const TraceValue& arg1_val,
             const char* arg2_name, const TraceValue& arg2_val,
             bool copy);
  ~TraceEvent();

  static const char* GetPhaseString(TraceEventPhase phase);
  static TraceEventPhase GetPhase(const char* phase);

  // Serialize event data to JSON
  static void AppendEventsAsJSON(const std::vector<TraceEvent>& events,
                                 size_t start,
                                 size_t count,
                                 std::string* out);
  void AppendAsJSON(std::string* out) const;

  TimeTicks timestamp() const { return timestamp_; }

  // Exposed for unittesting:

  const base::RefCountedString* parameter_copy_storage() const {
    return parameter_copy_storage_.get();
  }

  const char* name() const { return name_; }

 private:
  unsigned long process_id_;
  unsigned long thread_id_;
  TimeTicks timestamp_;
  TraceEventPhase phase_;
  const TraceCategory* category_;
  const char* name_;
  const char* arg_names_[kTraceMaxNumArgs];
  TraceValue arg_values_[kTraceMaxNumArgs];
  scoped_refptr<base::RefCountedString> parameter_copy_storage_;
};


// TraceResultBuffer collects and converts trace fragments returned by TraceLog
// to JSON output.
class BASE_EXPORT TraceResultBuffer {
 public:
  typedef base::Callback<void(const std::string&)> OutputCallback;

  // If you don't need to stream JSON chunks out efficiently, and just want to
  // get a complete JSON string after calling Finish, use this struct to collect
  // JSON trace output.
  struct BASE_EXPORT SimpleOutput {
    OutputCallback GetCallback();
    void Append(const std::string& json_string);

    // Do what you want with the json_output_ string after calling
    // TraceResultBuffer::Finish.
    std::string json_output;
  };

  TraceResultBuffer();
  ~TraceResultBuffer();

  // Set callback. The callback will be called during Start with the initial
  // JSON output and during AddFragment and Finish with following JSON output
  // chunks. The callback target must live past the last calls to
  // TraceResultBuffer::Start/AddFragment/Finish.
  void SetOutputCallback(OutputCallback json_chunk_callback);

  // Start JSON output. This resets all internal state, so you can reuse
  // the TraceResultBuffer by calling Start.
  void Start();

  // Call AddFragment 0 or more times to add trace fragments from TraceLog.
  void AddFragment(const std::string& trace_fragment);

  // When all fragments have been added, call Finish to complete the JSON
  // formatted output.
  void Finish();

 private:
  OutputCallback output_callback_;
  bool append_comma_;
};


class BASE_EXPORT TraceLog {
 public:
  // Flags for passing to AddTraceEvent.
  enum EventFlags {
    EVENT_FLAG_NONE = 0,
    EVENT_FLAG_COPY = 1<<0
  };

  static TraceLog* GetInstance();

  // Get set of known categories. This can change as new code paths are reached.
  // The known categories are inserted into |categories|.
  void GetKnownCategories(std::vector<std::string>* categories);

  // Enable tracing for provided list of categories. If tracing is already
  // enabled, this method does nothing -- changing categories during trace is
  // not supported.
  // If both included_categories and excluded_categories are empty,
  //   all categories are traced.
  // Else if included_categories is non-empty, only those are traced.
  // Else if excluded_categories is non-empty, everything but those are traced.
  // Wildcards * and ? are supported (see MatchPattern in string_util.h).
  void SetEnabled(const std::vector<std::string>& included_categories,
                  const std::vector<std::string>& excluded_categories);

  // |categories| is a comma-delimited list of category wildcards.
  // A category can have an optional '-' prefix to make it an excluded category.
  // All the same rules apply above, so for example, having both included and
  // excluded categories in the same list would not be supported.
  //
  // Example: SetEnabled("test_MyTest*");
  // Example: SetEnabled("test_MyTest*,test_OtherStuff");
  // Example: SetEnabled("-excluded_category1,-excluded_category2");
  void SetEnabled(const std::string& categories);

  // Retieves the categories set via a prior call to SetEnabled(). Only
  // meaningful if |IsEnabled()| is true.
  void GetEnabledTraceCategories(std::vector<std::string>* included_out,
                                 std::vector<std::string>* excluded_out);

  // Disable tracing for all categories.
  void SetDisabled();
  // Helper method to enable/disable tracing for all categories.
  void SetEnabled(bool enabled);
  bool IsEnabled() { return enabled_; }

  float GetBufferPercentFull() const;

  // When enough events are collected, they are handed (in bulk) to
  // the output callback. If no callback is set, the output will be
  // silently dropped. The callback must be thread safe. The string format is
  // undefined. Use TraceResultBuffer to convert one or more trace strings to
  // JSON.
  typedef RefCountedData<std::string> RefCountedString;
  typedef base::Callback<void(scoped_refptr<RefCountedString>)> OutputCallback;
  void SetOutputCallback(const OutputCallback& cb);

  // The trace buffer does not flush dynamically, so when it fills up,
  // subsequent trace events will be dropped. This callback is generated when
  // the trace buffer is full. The callback must be thread safe.
  typedef base::Callback<void(void)> BufferFullCallback;
  void SetBufferFullCallback(const BufferFullCallback& cb);

  // Flushes all logged data to the callback.
  void Flush();

  // Called by TRACE_EVENT* macros, don't call this directly.
  static const TraceCategory* GetCategory(const char* name);

  // Called by TRACE_EVENT* macros, don't call this directly.
  // Returns the index in the internal vector of the event if it was added, or
  //         -1 if the event was not added.
  // On end events, the return value of the begin event can be specified along
  // with a threshold in microseconds. If the elapsed time between begin and end
  // is less than the threshold, the begin/end event pair is dropped.
  // If |copy| is set, |name|, |arg_name1| and |arg_name2| will be deep copied
  // into the event; see "Memory scoping note" and TRACE_EVENT_COPY_XXX above.
  int AddTraceEvent(TraceEventPhase phase,
                    const TraceCategory* category,
                    const char* name,
                    const char* arg1_name, TraceValue arg1_val,
                    const char* arg2_name, TraceValue arg2_val,
                    int threshold_begin_id,
                    int64 threshold,
                    EventFlags flags);
  static void AddTraceEventEtw(TraceEventPhase phase,
                               const char* name,
                               const void* id,
                               const char* extra);
  static void AddTraceEventEtw(TraceEventPhase phase,
                               const char* name,
                               const void* id,
                               const std::string& extra);

  // A wrapper around AddTraceEvent used by TRACE_COUNTERx macros
  // that allows only integer values for the counters.
  int AddCounterEvent(const TraceCategory* category,
                      const char* name,
                      const char* arg1_name, int32 arg1_val,
                      const char* arg2_name, int32 arg2_val,
                      EventFlags flags);

  // Exposed for unittesting:

  // Allows deleting our singleton instance.
  static void DeleteForTesting();

  // Allows resurrecting our singleton instance post-AtExit processing.
  static void Resurrect();

  // Allow tests to inspect TraceEvents.
  size_t GetEventsSize() const { return logged_events_.size(); }
  const TraceEvent& GetEventAt(size_t index) const {
    DCHECK(index < logged_events_.size());
    return logged_events_[index];
  }

 private:
  // This allows constructor and destructor to be private and usable only
  // by the Singleton class.
  friend struct StaticMemorySingletonTraits<TraceLog>;

  TraceLog();
  ~TraceLog();
  const TraceCategory* GetCategoryInternal(const char* name);
  void AddCurrentMetadataEvents();

  // TODO(nduca): switch to per-thread trace buffers to reduce thread
  // synchronization.
  Lock lock_;
  bool enabled_;
  OutputCallback output_callback_;
  BufferFullCallback buffer_full_callback_;
  std::vector<TraceEvent> logged_events_;
  std::vector<std::string> included_categories_;
  std::vector<std::string> excluded_categories_;

  base::hash_map<PlatformThreadId, std::string> thread_names_;

  DISALLOW_COPY_AND_ASSIGN(TraceLog);
};

namespace internal {

// Used by TRACE_EVENTx macro. Do not use directly.
class BASE_EXPORT TraceEndOnScopeClose {
 public:
  // Note: members of data_ intentionally left uninitialized. See Initialize.
  TraceEndOnScopeClose() : p_data_(NULL) {}
  ~TraceEndOnScopeClose() {
    if (p_data_)
      AddEventIfEnabled();
  }

  void Initialize(const TraceCategory* category,
                  const char* name);

 private:
  // Add the end event if the category is still enabled.
  void AddEventIfEnabled();

  // This Data struct workaround is to avoid initializing all the members
  // in Data during construction of this object, since this object is always
  // constructed, even when tracing is disabled. If the members of Data were
  // members of this class instead, compiler warnings occur about potential
  // uninitialized accesses.
  struct Data {
    const TraceCategory* category;
    const char* name;
  };
  Data* p_data_;
  Data data_;
};

// Used by TRACE_EVENTx macro. Do not use directly.
class BASE_EXPORT TraceEndOnScopeCloseThreshold {
 public:
  // Note: members of data_ intentionally left uninitialized. See Initialize.
  TraceEndOnScopeCloseThreshold() : p_data_(NULL) {}
  ~TraceEndOnScopeCloseThreshold() {
    if (p_data_)
      AddEventIfEnabled();
  }

  // Called by macros only when tracing is enabled at the point when the begin
  // event is added.
  void Initialize(const TraceCategory* category,
                  const char* name,
                  int threshold_begin_id,
                  int64 threshold);

 private:
  // Add the end event if the category is still enabled.
  void AddEventIfEnabled();

  // This Data struct workaround is to avoid initializing all the members
  // in Data during construction of this object, since this object is always
  // constructed, even when tracing is disabled. If the members of Data were
  // members of this class instead, compiler warnings occur about potential
  // uninitialized accesses.
  struct Data {
    int64 threshold;
    const TraceCategory* category;
    const char* name;
    int threshold_begin_id;
  };
  Data* p_data_;
  Data data_;
};

}  // namespace internal

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_TRACE_EVENT_H_

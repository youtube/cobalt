// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Trace events are for tracking application performance.
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
//         "howMuch", StringPrintf("%i", howMuch).c_str());
//     ...
//   }
//
// The trace system will automatically add to this information the
// current process id, thread id, a timestamp down to the
// microsecond, as well as the file and line number of the calling location.
//
// By default, trace collection is compiled in, but turned off at runtime.
// Collecting trace data is the responsibility of the embedding
// application. In Chrome's case, navigating to about:gpu will turn on
// tracing and display data collected across all active processes.
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
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/timer.h"

// Older style trace macros with explicit id and extra data
// Only these macros result in publishing data to ETW as currently implemented.
#define TRACE_EVENT_BEGIN_ETW(name, id, extra) \
  base::debug::TraceLog::AddTraceEventEtw( \
      base::debug::TRACE_EVENT_PHASE_BEGIN, \
      __FILE__, __LINE__, name, reinterpret_cast<const void*>(id), extra);

#define TRACE_EVENT_END_ETW(name, id, extra) \
  base::debug::TraceLog::AddTraceEventEtw( \
      base::debug::TRACE_EVENT_PHASE_END, \
      __FILE__, __LINE__, name, reinterpret_cast<const void*>(id), extra);

#define TRACE_EVENT_INSTANT_ETW(name, id, extra) \
  base::debug::TraceLog::AddTraceEventEtw( \
      base::debug::TRACE_EVENT_PHASE_INSTANT, \
      __FILE__, __LINE__, name, reinterpret_cast<const void*>(id), extra);


// Implementation detail: trace event macros create temporary variables
// to keep instrumentation overhead low. These macros give each temporary
// variable a unique name based on the line number to prevent name collissions.
#define TRACE_EVENT_UNIQUE_IDENTIFIER3(a,b) a##b
#define TRACE_EVENT_UNIQUE_IDENTIFIER2(a,b) \
  TRACE_EVENT_UNIQUE_IDENTIFIER3(a,b)
#define TRACE_EVENT_UNIQUE_IDENTIFIER(name_prefix) \
  TRACE_EVENT_UNIQUE_IDENTIFIER2(name_prefix, __LINE__)

// Records a pair of begin and end events called "name" for the current
// scope, with 0, 1 or 2 associated arguments. If the category is not
// enabled, then this does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - ANNOTATE_BENIGN_RACE, see Thread Safety above.
#define TRACE_EVENT0(category, name) \
  TRACE_EVENT1(category, name, NULL, 0)
#define TRACE_EVENT1(category, name, arg1_name, arg1_val) \
  TRACE_EVENT2(category, name, arg1_name, arg1_val, NULL, 0)
#define TRACE_EVENT2(category, name, arg1_name, arg1_val, arg2_name, arg2_val) \
  static const base::debug::TraceCategory* \
      TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic) = NULL; \
  ANNOTATE_BENIGN_RACE(&TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), \
                       "trace_event category"); \
  if (!TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic)) \
    TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic) = \
        base::debug::TraceLog::GetCategory(category); \
  if (TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic)->enabled) { \
    base::debug::TraceLog::GetInstance()->AddTraceEvent( \
        base::debug::TRACE_EVENT_PHASE_BEGIN, \
        __FILE__, __LINE__, \
        TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), \
        name, \
        arg1_name, arg1_val, \
        arg2_name, arg2_val); \
  } \
  base::debug::internal::TraceEndOnScopeClose \
      TRACE_EVENT_UNIQUE_IDENTIFIER(profileScope) ( \
          __FILE__, __LINE__, \
          TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), name);

// Records a single event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - ANNOTATE_BENIGN_RACE, see Thread Safety above.
#define TRACE_EVENT_INSTANT0(category, name) \
  TRACE_EVENT_INSTANT1(category, name, NULL, 0)
#define TRACE_EVENT_INSTANT1(category, name, arg1_name, arg1_val) \
  TRACE_EVENT_INSTANT2(category, name, arg1_name, arg1_val, NULL, 0)
#define TRACE_EVENT_INSTANT2(category, name, arg1_name, arg1_val, \
    arg2_name, arg2_val) \
  static const base::debug::TraceCategory* \
      TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic) = NULL; \
  ANNOTATE_BENIGN_RACE(&TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), \
                       "trace_event category"); \
  if (!TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic)) \
    TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic) = \
        base::debug::TraceLog::GetCategory(category); \
  if (TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic)->enabled) { \
    base::debug::TraceLog::GetInstance()->AddTraceEvent( \
        base::debug::TRACE_EVENT_PHASE_INSTANT, \
        __FILE__, __LINE__, \
        TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), \
        name, \
        arg1_name, arg1_val, \
        arg2_name, arg2_val); \
  }

// Records a single BEGIN event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - ANNOTATE_BENIGN_RACE, see Thread Safety above.
#define TRACE_EVENT_BEGIN0(category, name) \
  TRACE_EVENT_BEGIN1(category, name, NULL, 0)
#define TRACE_EVENT_BEGIN1(category, name, arg1_name, arg1_val) \
  TRACE_EVENT_BEGIN2(category, name, arg1_name, arg1_val, NULL, 0)
#define TRACE_EVENT_BEGIN2(category, name, arg1_name, arg1_val, \
    arg2_name, arg2_val) \
  static const base::debug::TraceCategory* \
      TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic) = NULL; \
  ANNOTATE_BENIGN_RACE(&TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), \
                       "trace_event category"); \
  if (!TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic)) \
    TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic) = \
        base::debug::TraceLog::GetCategory(category); \
  if (TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic)->enabled) { \
    base::debug::TraceLog::GetInstance()->AddTraceEvent( \
        base::debug::TRACE_EVENT_PHASE_BEGIN, \
        __FILE__, __LINE__, \
        TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), \
        name, \
        arg1_name, arg1_val, \
        arg2_name, arg2_val); \
  }

// Records a single END event for "name" immediately. If the category
// is not enabled, then this does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - ANNOTATE_BENIGN_RACE, see Thread Safety above.
#define TRACE_EVENT_END0(category, name) \
  TRACE_EVENT_END1(category, name, NULL, 0)
#define TRACE_EVENT_END1(category, name, arg1_name, arg1_val) \
  TRACE_EVENT_END2(category, name, arg1_name, arg1_val, NULL, 0)
#define TRACE_EVENT_END2(category, name, arg1_name, arg1_val, \
    arg2_name, arg2_val) \
  static const base::debug::TraceCategory* \
      TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic) = NULL; \
  ANNOTATE_BENIGN_RACE(&TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), \
                       "trace_event category"); \
  if (!TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic)) \
    TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic) = \
        base::debug::TraceLog::GetCategory(category); \
  if (TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic)->enabled) { \
    base::debug::TraceLog::GetInstance()->AddTraceEvent( \
        base::debug::TRACE_EVENT_PHASE_END, \
        __FILE__, __LINE__, \
        TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), \
        name, \
        arg1_name, arg1_val, \
        arg2_name, arg2_val); \
  }

namespace base {

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
  TRACE_EVENT_PHASE_INSTANT
};

// Simple union of values. This is much lighter weight than base::Value, which
// requires dynamic allocation and a vtable. To keep the trace runtime overhead
// low, we want constant size storage here.
class BASE_API TraceValue {
 public:
  enum Type {
    TRACE_TYPE_UNDEFINED,
    TRACE_TYPE_BOOL,
    TRACE_TYPE_UINT,
    TRACE_TYPE_INT,
    TRACE_TYPE_DOUBLE,
    TRACE_TYPE_POINTER,
    TRACE_TYPE_STRING
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
  TraceValue(const char* rhs) : type_(TRACE_TYPE_STRING) {
    value_.as_string_refptr = new RefCountedString();
    value_.as_string_refptr->AddRef();
    DCHECK(value_.as_string_refptr->HasOneRef());
    value_.as_string_refptr->data = rhs;
  }
  TraceValue(const TraceValue& rhs) : type_(TRACE_TYPE_UNDEFINED) {
    operator=(rhs);
  }
  ~TraceValue() {
    Destroy();
  }

  TraceValue& operator=(const TraceValue& rhs);

  void Destroy();

  void AppendAsJSON(std::string* out) const;

  Type type() const {
    return type_;
  }
  uint64 as_uint() const {
    return value_.as_uint;
  }
  bool as_bool() const {
    return value_.as_bool;
  }
  int64 as_int() const {
    return value_.as_int;
  }
  double as_double() const {
    return value_.as_double;
  }
  const void* as_pointer() const {
    return value_.as_pointer;
  }
  const char* as_string() const {
    return value_.as_string_refptr->data.c_str();
  }

 typedef RefCountedData<std::string> RefCountedString;

 private:
  union Value {
    bool as_bool;
    uint64 as_uint;
    int64 as_int;
    double as_double;
    const void* as_pointer;
    RefCountedString* as_string_refptr;
  };

  Type type_;
  Value value_;
};

// Output records are "Events" and can be obtained via the
// OutputCallback whenever the tracing system decides to flush. This
// can happen at any time, on any thread, or you can programatically
// force it to happen.
struct TraceEvent {
  TraceEvent();
  ~TraceEvent();

  // Serialize event data to JSON
  static void AppendEventsAsJSON(const std::vector<TraceEvent>& events,
                                 size_t start,
                                 size_t count,
                                 std::string* out);
  void AppendAsJSON(std::string* out) const;


  unsigned long process_id;
  unsigned long thread_id;
  TimeTicks timestamp;
  TraceEventPhase phase;
  const TraceCategory* category;
  const char* name;
  const char* arg_names[kTraceMaxNumArgs];
  TraceValue arg_values[kTraceMaxNumArgs];
};


class BASE_API TraceLog {
 public:
  static TraceLog* GetInstance();

  // Global enable of tracing. Currently enables all categories or not.
  // TODO(scheib): Replace with an Enable/DisableCategory() that
  // implicitly controls the global logging state.
  void SetEnabled(bool enabled);
  bool IsEnabled() { return enabled_; }

  float GetBufferPercentFull() const;

  // When enough events are collected, they are handed (in bulk) to
  // the output callback. If no callback is set, the output will be
  // silently dropped. The callback must be thread safe.
  typedef RefCountedData<std::string> RefCountedString;
  typedef base::Callback<void(RefCountedString*)> OutputCallback;
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
  void AddTraceEvent(TraceEventPhase phase,
      const char* file, int line,
      const TraceCategory* category,
      const char* name,
      const char* arg1_name, TraceValue arg1_val,
      const char* arg2_name, TraceValue arg2_val);
  static void AddTraceEventEtw(TraceEventPhase phase,
      const char* file, int line,
      const char* name,
      const void* id,
      const char* extra);
  static void AddTraceEventEtw(TraceEventPhase phase,
      const char* file, int line,
      const char* name,
      const void* id,
      const std::string& extra) {
    AddTraceEventEtw(phase, file, line, name, id, extra.c_str());
  }

  // Exposed for unittesting only, allows resurrecting our
  // singleton instance post-AtExit processing.
  static void Resurrect();

 private:
  // This allows constructor and destructor to be private and usable only
  // by the Singleton class.
  friend struct StaticMemorySingletonTraits<TraceLog>;

  TraceLog();
  ~TraceLog();
  const TraceCategory* GetCategoryInternal(const char* name);

  // TODO(nduca): switch to per-thread trace buffers to reduce thread
  // synchronization.
  Lock lock_;
  bool enabled_;
  OutputCallback output_callback_;
  BufferFullCallback buffer_full_callback_;
  std::vector<TraceEvent> logged_events_;

  DISALLOW_COPY_AND_ASSIGN(TraceLog);
};

namespace internal {

// Used by TRACE_EVENTx macro. Do not use directly.
class BASE_API TraceEndOnScopeClose {
 public:
  TraceEndOnScopeClose(const char* file, int line,
                       const TraceCategory* category,
                       const char* name)
      : file_(file)
      , line_(line)
      , category_(category)
      , name_(name) { }

  ~TraceEndOnScopeClose() {
    if (category_->enabled)
      base::debug::TraceLog::GetInstance()->AddTraceEvent(
          base::debug::TRACE_EVENT_PHASE_END,
          file_, line_,
          category_,
          name_,
          NULL, 0, NULL, 0);
  }

 private:
  const char* file_;
  int line_;
  const TraceCategory* category_;
  const char* name_;
};

}  // namespace internal

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_TRACE_EVENT_H_

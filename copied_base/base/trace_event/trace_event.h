// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACE_EVENT_H_
#define BASE_TRACE_EVENT_TRACE_EVENT_H_

// This header file defines implementation details of how the trace macros in
// trace_event_common.h collect and store trace events. Anything not
// implementation-specific should go in trace_event_common.h instead of here.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/debug/debugging_buildflags.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/trace_event/builtin_categories.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_arguments.h"
#include "base/trace_event/trace_category.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/traced_value_support.h"
#include "base/tracing_buildflags.h"

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

// By default, const char* argument values are assumed to have long-lived scope
// and will not be copied. Use this macro to force a const char* to be copied.
#define TRACE_STR_COPY(str) ::base::trace_event::TraceStringWithCopy(str)

// By default, trace IDs are eventually converted to a single 64-bit number. Use
// this macro to add a scope string. For example,
//
// TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
//     "network", "ResourceLoad",
//     TRACE_ID_WITH_SCOPE("BlinkResourceID", resourceID));
#define TRACE_ID_WITH_SCOPE(scope, ...) \
  trace_event_internal::TraceID::WithScope(scope, ##__VA_ARGS__)

// Use this for ids that are unique across processes. This allows different
// processes to use the same id to refer to the same event.
#define TRACE_ID_GLOBAL(id) trace_event_internal::TraceID::GlobalId(id)

// Use this for ids that are unique within a single process. This allows
// different processes to use the same id to refer to different events.
#define TRACE_ID_LOCAL(id) trace_event_internal::TraceID::LocalId(id)

#define TRACE_EVENT_API_CURRENT_THREAD_ID base::PlatformThread::CurrentId()

#define INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE() \
  UNLIKELY(*INTERNAL_TRACE_EVENT_UID(category_group_enabled) &           \
           (base::trace_event::TraceCategory::ENABLED_FOR_RECORDING |    \
            base::trace_event::TraceCategory::ENABLED_FOR_ETW_EXPORT))

#define INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()                  \
  UNLIKELY(*INTERNAL_TRACE_EVENT_UID(category_group_enabled) &         \
           (base::trace_event::TraceCategory::ENABLED_FOR_RECORDING |  \
            base::trace_event::TraceCategory::ENABLED_FOR_ETW_EXPORT | \
            base::trace_event::TraceCategory::ENABLED_FOR_FILTERING))

////////////////////////////////////////////////////////////////////////////////
// Implementation specific tracing API definitions.

// Get a pointer to the enabled state of the given trace category. Only
// long-lived literal strings should be given as the category group. The
// returned pointer can be held permanently in a local static for example. If
// the unsigned char is non-zero, tracing is enabled. If tracing is enabled,
// TRACE_EVENT_API_ADD_TRACE_EVENT can be called. It's OK if tracing is disabled
// between the load of the tracing state and the call to
// TRACE_EVENT_API_ADD_TRACE_EVENT, because this flag only provides an early out
// for best performance when tracing is disabled.
// const unsigned char*
//     TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(const char* category_group)
#define TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED \
    base::trace_event::TraceLog::GetCategoryGroupEnabled

// Get the number of times traces have been recorded. This is used to implement
// the TRACE_EVENT_IS_NEW_TRACE facility.
// unsigned int TRACE_EVENT_API_GET_NUM_TRACES_RECORDED()
#define TRACE_EVENT_API_GET_NUM_TRACES_RECORDED \
  trace_event_internal::GetNumTracesRecorded

#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

// Legacy TRACE_EVENT_API entrypoints. Do not use from new code.

// Add a trace event to the platform tracing system.
// base::trace_event::TraceEventHandle TRACE_EVENT_API_ADD_TRACE_EVENT(
//                    char phase,
//                    const unsigned char* category_group_enabled,
//                    const char* name,
//                    const char* scope,
//                    uint64_t id,
//                    base::trace_event::TraceArguments* args,
//                    unsigned int flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT trace_event_internal::AddTraceEvent

// Add a trace event to the platform tracing system.
// base::trace_event::TraceEventHandle
// TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_BIND_ID(
//                    char phase,
//                    const unsigned char* category_group_enabled,
//                    const char* name,
//                    const char* scope,
//                    uint64_t id,
//                    uint64_t bind_id,
//                    base::trace_event::TraceArguments* args,
//                    unsigned int flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_BIND_ID \
  trace_event_internal::AddTraceEventWithBindId

// Add a trace event to the platform tracing system overriding the pid.
// The resulting event will have tid = pid == (process_id passed here).
// base::trace_event::TraceEventHandle
// TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID(
//                    char phase,
//                    const unsigned char* category_group_enabled,
//                    const char* name,
//                    const char* scope,
//                    uint64_t id,
//                    base::ProcessId process_id,
//                    base::trace_event::TraceArguments* args,
//                    unsigned int flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID \
  trace_event_internal::AddTraceEventWithProcessId

// Add a trace event to the platform tracing system.
// base::trace_event::TraceEventHandle
// TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
//                    char phase,
//                    const unsigned char* category_group_enabled,
//                    const char* name,
//                    const char* scope,
//                    uint64_t id,
//                    uint64_t bind_id,
//                    base::PlatformThreadId thread_id,
//                    const TimeTicks& timestamp,
//                    base::trace_event::TraceArguments* args,
//                    unsigned int flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP \
  trace_event_internal::AddTraceEventWithThreadIdAndTimestamp

// Set the duration field of a COMPLETE trace event.
// void TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(
//     const unsigned char* category_group_enabled,
//     const char* name,
//     base::trace_event::TraceEventHandle id)
#define TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION \
  trace_event_internal::UpdateTraceEventDuration

// Adds a metadata event to the trace log. The |AppendValueAsTraceFormat| method
// on the convertable value will be called at flush time.
// TRACE_EVENT_API_ADD_METADATA_EVENT(
//     const unsigned char* category_group_enabled,
//     const char* event_name,
//     const char* arg_name,
//     std::unique_ptr<ConvertableToTraceFormat> arg_value)
#define TRACE_EVENT_API_ADD_METADATA_EVENT \
    trace_event_internal::AddMetadataEvent

// Defines atomic operations used internally by the tracing system.
// Acquire/release barriers are important here: crbug.com/1330114#c8.
#define TRACE_EVENT_API_ATOMIC_WORD base::subtle::AtomicWord
#define TRACE_EVENT_API_ATOMIC_LOAD(var) base::subtle::Acquire_Load(&(var))
#define TRACE_EVENT_API_ATOMIC_STORE(var, value) \
  base::subtle::Release_Store(&(var), (value))

// Defines visibility for classes in trace_event.h
#define TRACE_EVENT_API_CLASS_EXPORT BASE_EXPORT

////////////////////////////////////////////////////////////////////////////////

// Implementation detail: trace event macros create temporary variables
// to keep instrumentation overhead low. These macros give each temporary
// variable a unique name based on the line number to prevent name collisions.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
#define INTERNAL_TRACE_EVENT_UID3(a, b) trace_event_unique_##a##b
#define INTERNAL_TRACE_EVENT_UID2(a, b) INTERNAL_TRACE_EVENT_UID3(a, b)
#define INTERNAL_TRACE_EVENT_UID(name_prefix) \
    INTERNAL_TRACE_EVENT_UID2(name_prefix, __LINE__)

// Implementation detail: internal macro to create static category.
// No barriers are needed, because this code is designed to operate safely
// even when the unsigned char* points to garbage data (which may be the case
// on processors without cache coherency).
#define INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO_CUSTOM_VARIABLES(    \
    category_group, atomic, category_group_enabled)                 \
  category_group_enabled = reinterpret_cast<const unsigned char*>(  \
      TRACE_EVENT_API_ATOMIC_LOAD(atomic));                         \
  if (UNLIKELY(!category_group_enabled)) {                          \
    category_group_enabled =                                        \
        TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(category_group); \
    TRACE_EVENT_API_ATOMIC_STORE(                                   \
        atomic, reinterpret_cast<TRACE_EVENT_API_ATOMIC_WORD>(      \
                    category_group_enabled));                       \
  }

#define INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO_MAYBE_AT_COMPILE_TIME(        \
    category_group, k_category_group_enabled, category_group_enabled)        \
  if (k_category_group_enabled) {                                            \
    category_group_enabled = k_category_group_enabled;                       \
  } else {                                                                   \
    static TRACE_EVENT_API_ATOMIC_WORD INTERNAL_TRACE_EVENT_UID(atomic) = 0; \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO_CUSTOM_VARIABLES(                 \
        category_group, INTERNAL_TRACE_EVENT_UID(atomic),                    \
        category_group_enabled);                                             \
  }

#define INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group)                 \
  static_assert(                                                               \
      base::trace_event::BuiltinCategories::IsAllowedCategory(category_group), \
      "Unknown tracing category is used. Please register your "                \
      "category in base/trace_event/builtin_categories.h");                    \
  constexpr const unsigned char* INTERNAL_TRACE_EVENT_UID(                     \
      k_category_group_enabled) =                                              \
      base::trace_event::TraceLog::GetBuiltinCategoryEnabled(category_group);  \
  const unsigned char* INTERNAL_TRACE_EVENT_UID(category_group_enabled);       \
  INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO_MAYBE_AT_COMPILE_TIME(                \
      category_group, INTERNAL_TRACE_EVENT_UID(k_category_group_enabled),      \
      INTERNAL_TRACE_EVENT_UID(category_group_enabled));

// Implementation detail: internal macro to return unoverridden
// base::TimeTicks::Now(). This is important because in headless VirtualTime can
// override base:TimeTicks::Now().
#define INTERNAL_TRACE_TIME_TICKS_NOW() \
  base::subtle::TimeTicksNowIgnoringOverride()

// Implementation detail: internal macro to return unoverridden
// base::Time::Now(). This is important because in headless VirtualTime can
// override base:TimeTicks::Now().
#define INTERNAL_TRACE_TIME_NOW() base::subtle::TimeNowIgnoringOverride()

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD(phase, category_group, name, flags, ...)  \
  do {                                                                     \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                   \
      trace_event_internal::AddTraceEvent(                                 \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,   \
          trace_event_internal::kGlobalScope, trace_event_internal::kNoId, \
          flags, trace_event_internal::kNoId, ##__VA_ARGS__);              \
    }                                                                      \
  } while (0)

// Implementation detail: internal macro to create static category and add begin
// event if the category is enabled. Also adds the end event when the scope
// ends.
#define INTERNAL_TRACE_EVENT_ADD_SCOPED(category_group, name, ...)           \
  INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                    \
  trace_event_internal::ScopedTracer INTERNAL_TRACE_EVENT_UID(tracer);       \
  if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                       \
    base::trace_event::TraceEventHandle INTERNAL_TRACE_EVENT_UID(h) =        \
        trace_event_internal::AddTraceEvent(                                 \
            TRACE_EVENT_PHASE_COMPLETE,                                      \
            INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,          \
            trace_event_internal::kGlobalScope, trace_event_internal::kNoId, \
            TRACE_EVENT_FLAG_NONE, trace_event_internal::kNoId,              \
            ##__VA_ARGS__);                                                  \
    INTERNAL_TRACE_EVENT_UID(tracer).Initialize(                             \
        INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,              \
        INTERNAL_TRACE_EVENT_UID(h));                                        \
  }

#define INTERNAL_TRACE_EVENT_ADD_SCOPED_WITH_FLAGS(category_group, name,     \
                                                   flags, ...)               \
  INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                    \
  trace_event_internal::ScopedTracer INTERNAL_TRACE_EVENT_UID(tracer);       \
  if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                       \
    base::trace_event::TraceEventHandle INTERNAL_TRACE_EVENT_UID(h) =        \
        trace_event_internal::AddTraceEvent(                                 \
            TRACE_EVENT_PHASE_COMPLETE,                                      \
            INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,          \
            trace_event_internal::kGlobalScope, trace_event_internal::kNoId, \
            flags, trace_event_internal::kNoId, ##__VA_ARGS__);              \
    INTERNAL_TRACE_EVENT_UID(tracer).Initialize(                             \
        INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,              \
        INTERNAL_TRACE_EVENT_UID(h));                                        \
  }

#define INTERNAL_TRACE_EVENT_ADD_SCOPED_WITH_FLOW(category_group, name,      \
                                                  bind_id, flow_flags, ...)  \
  INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                    \
  trace_event_internal::ScopedTracer INTERNAL_TRACE_EVENT_UID(tracer);       \
  if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                       \
    trace_event_internal::TraceID trace_event_bind_id((bind_id));            \
    unsigned int trace_event_flags =                                         \
        flow_flags | trace_event_bind_id.id_flags();                         \
    base::trace_event::TraceEventHandle INTERNAL_TRACE_EVENT_UID(h) =        \
        trace_event_internal::AddTraceEvent(                                 \
            TRACE_EVENT_PHASE_COMPLETE,                                      \
            INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,          \
            trace_event_internal::kGlobalScope, trace_event_internal::kNoId, \
            trace_event_flags, trace_event_bind_id.raw_id(), ##__VA_ARGS__); \
    INTERNAL_TRACE_EVENT_UID(tracer).Initialize(                             \
        INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,              \
        INTERNAL_TRACE_EVENT_UID(h));                                        \
  }

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID(phase, category_group, name, id, \
                                         flags, ...)                      \
  do {                                                                    \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);               \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                  \
      trace_event_internal::TraceID trace_event_trace_id((id));           \
      unsigned int trace_event_flags =                                    \
          flags | trace_event_trace_id.id_flags();                        \
      trace_event_internal::AddTraceEvent(                                \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,  \
          trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),    \
          trace_event_flags, trace_event_internal::kNoId, ##__VA_ARGS__); \
    }                                                                     \
  } while (0)

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_TIMESTAMP(phase, category_group, name, \
                                                timestamp, flags, ...)       \
  do {                                                                       \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                  \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                     \
      trace_event_internal::AddTraceEventWithThreadIdAndTimestamp(           \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,     \
          trace_event_internal::kGlobalScope, trace_event_internal::kNoId,   \
          TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp,                      \
          flags | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,                       \
          trace_event_internal::kNoId, ##__VA_ARGS__);                       \
    }                                                                        \
  } while (0)

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(              \
    phase, category_group, name, id, thread_id, timestamp, flags, ...)   \
  do {                                                                   \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);              \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                 \
      trace_event_internal::TraceID trace_event_trace_id((id));          \
      unsigned int trace_event_flags =                                   \
          flags | trace_event_trace_id.id_flags();                       \
      trace_event_internal::AddTraceEventWithThreadIdAndTimestamp(       \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name, \
          trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),   \
          thread_id, timestamp,                                          \
          trace_event_flags | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,       \
          trace_event_internal::kNoId, ##__VA_ARGS__);                   \
    }                                                                    \
  } while (0)

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMPS(                 \
    phase, category_group, name, id, thread_id, timestamp, thread_timestamp, \
    flags)                                                                   \
  do {                                                                       \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                  \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                     \
      trace_event_internal::TraceID trace_event_trace_id((id));              \
      unsigned int trace_event_flags =                                       \
          flags | trace_event_trace_id.id_flags();                           \
      const unsigned char* uid_category_group_enabled =                      \
          INTERNAL_TRACE_EVENT_UID(category_group_enabled);                  \
      trace_event_internal::AddTraceEventWithThreadIdAndTimestamps(          \
          phase, uid_category_group_enabled, name,                           \
          trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),       \
          thread_id, timestamp, thread_timestamp,                            \
          trace_event_flags | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP);          \
    }                                                                        \
  } while (0)

// Implementation detail: internal macro to create static category and add
// metadata event if the category is enabled.
#define INTERNAL_TRACE_EVENT_METADATA_ADD(category_group, name, ...) \
  do {                                                               \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);          \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {             \
      TRACE_EVENT_API_ADD_METADATA_EVENT(                            \
          INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,    \
          ##__VA_ARGS__);                                            \
    }                                                                \
  } while (0)
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

namespace trace_event_internal {

// Specify these values when the corresponding argument of AddTraceEvent is not
// used.
const int kZeroNumArgs = 0;
const std::nullptr_t kGlobalScope = nullptr;
const uint64_t kNoId = 0;

// TraceID encapsulates an ID that can either be an integer or pointer.
class BASE_EXPORT TraceID {
 public:
  // Can be combined with WithScope.
  class LocalId {
   public:
    explicit LocalId(const void* raw_id)
        : raw_id_(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(raw_id))) {}
    explicit LocalId(uint64_t raw_id) : raw_id_(raw_id) {}
    uint64_t raw_id() const { return raw_id_; }

   private:
    uint64_t raw_id_;
  };

  // Can be combined with WithScope.
  class GlobalId {
   public:
    explicit GlobalId(uint64_t raw_id) : raw_id_(raw_id) {}
    uint64_t raw_id() const { return raw_id_; }

   private:
    uint64_t raw_id_;
  };

  class WithScope {
   public:
    WithScope(const char* scope, uint64_t raw_id)
        : scope_(scope), raw_id_(raw_id) {}
    WithScope(const char* scope, LocalId local_id)
        : scope_(scope), raw_id_(local_id.raw_id()) {
      id_flags_ = TRACE_EVENT_FLAG_HAS_LOCAL_ID;
    }
    WithScope(const char* scope, GlobalId global_id)
        : scope_(scope), raw_id_(global_id.raw_id()) {
      id_flags_ = TRACE_EVENT_FLAG_HAS_GLOBAL_ID;
    }
    uint64_t raw_id() const { return raw_id_; }
    const char* scope() const { return scope_; }
    unsigned int id_flags() const { return id_flags_; }

   private:
    const char* scope_ = nullptr;
    uint64_t raw_id_;
    unsigned int id_flags_ = TRACE_EVENT_FLAG_HAS_ID;
  };

  explicit TraceID(const void* raw_id)
      : raw_id_(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(raw_id))) {
    id_flags_ = TRACE_EVENT_FLAG_HAS_LOCAL_ID;
  }
  explicit TraceID(unsigned long long raw_id)
      : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(unsigned long raw_id)
      : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(unsigned int raw_id)
      : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(unsigned short raw_id)
      : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(unsigned char raw_id)
      : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(long long raw_id) : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(long raw_id) : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(int raw_id) : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(short raw_id) : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(signed char raw_id)
      : raw_id_(static_cast<uint64_t>(raw_id)) {}
  explicit TraceID(LocalId raw_id) : raw_id_(raw_id.raw_id()) {
    id_flags_ = TRACE_EVENT_FLAG_HAS_LOCAL_ID;
  }
  explicit TraceID(GlobalId raw_id) : raw_id_(raw_id.raw_id()) {
    id_flags_ = TRACE_EVENT_FLAG_HAS_GLOBAL_ID;
  }
  explicit TraceID(WithScope scoped_id)
      : scope_(scoped_id.scope()),
        raw_id_(scoped_id.raw_id()),
        id_flags_(scoped_id.id_flags()) {}

  uint64_t raw_id() const { return raw_id_; }
  const char* scope() const { return scope_; }
  unsigned int id_flags() const { return id_flags_; }

 private:
  const char* scope_ = nullptr;
  uint64_t raw_id_;
  unsigned int id_flags_ = TRACE_EVENT_FLAG_HAS_ID;
};

// These functions all internally call
// base::trace_event::TraceLog::GetInstance() then call the method with the same
// name on it. This is used to reduce the generated machine code at each
// TRACE_EVENTXXX macro call.

base::trace_event::TraceEventHandle BASE_EXPORT
AddTraceEvent(char phase,
              const unsigned char* category_group_enabled,
              const char* name,
              const char* scope,
              uint64_t id,
              base::trace_event::TraceArguments* args,
              unsigned int flags);

base::trace_event::TraceEventHandle BASE_EXPORT
AddTraceEventWithBindId(char phase,
                        const unsigned char* category_group_enabled,
                        const char* name,
                        const char* scope,
                        uint64_t id,
                        uint64_t bind_id,
                        base::trace_event::TraceArguments* args,
                        unsigned int flags);

base::trace_event::TraceEventHandle BASE_EXPORT
AddTraceEventWithProcessId(char phase,
                           const unsigned char* category_group_enabled,
                           const char* name,
                           const char* scope,
                           uint64_t id,
                           base::ProcessId process_id,
                           base::trace_event::TraceArguments* args,
                           unsigned int flags);

base::trace_event::TraceEventHandle BASE_EXPORT
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    base::trace_event::TraceArguments* args,
    unsigned int flags);

base::trace_event::TraceEventHandle BASE_EXPORT
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    uint64_t bind_id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    base::trace_event::TraceArguments* args,
    unsigned int flags);

base::trace_event::TraceEventHandle BASE_EXPORT
AddTraceEventWithThreadIdAndTimestamps(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    const base::ThreadTicks& thread_timestamp,
    unsigned int flags);

void BASE_EXPORT AddMetadataEvent(const unsigned char* category_group_enabled,
                                  const char* name,
                                  base::trace_event::TraceArguments* args,
                                  unsigned int flags);

int BASE_EXPORT GetNumTracesRecorded();

void BASE_EXPORT
UpdateTraceEventDuration(const unsigned char* category_group_enabled,
                         const char* name,
                         base::trace_event::TraceEventHandle handle);

void BASE_EXPORT
UpdateTraceEventDurationExplicit(const unsigned char* category_group_enabled,
                                 const char* name,
                                 base::trace_event::TraceEventHandle handle,
                                 base::PlatformThreadId thread_id,
                                 bool explicit_timestamps,
                                 const base::TimeTicks& now,
                                 const base::ThreadTicks& thread_now);

// These AddTraceEvent and AddTraceEventWithThreadIdAndTimestamp template
// functions are defined here instead of in the macro, because the arg_values
// could be temporary objects, such as `std::string`. In order to store
// pointers to the internal c_str and pass through to the tracing API,
// the arg_values must live throughout these procedures.

template <class ARG1_TYPE>
inline base::trace_event::TraceEventHandle
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    unsigned int flags,
    uint64_t bind_id,
    const char* arg1_name,
    ARG1_TYPE&& arg1_val) {
  base::trace_event::TraceArguments args(arg1_name,
                                         std::forward<ARG1_TYPE>(arg1_val));
  return TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, &args, flags);
}

template <class ARG1_TYPE, class ARG2_TYPE>
inline base::trace_event::TraceEventHandle
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    unsigned int flags,
    uint64_t bind_id,
    const char* arg1_name,
    ARG1_TYPE&& arg1_val,
    const char* arg2_name,
    ARG2_TYPE&& arg2_val) {
  base::trace_event::TraceArguments args(
      arg1_name, std::forward<ARG1_TYPE>(arg1_val), arg2_name,
      std::forward<ARG2_TYPE>(arg2_val));
  return TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, &args, flags);
}

inline base::trace_event::TraceEventHandle
AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    base::PlatformThreadId thread_id,
    const base::TimeTicks& timestamp,
    unsigned int flags,
    uint64_t bind_id) {
  return TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
      phase, category_group_enabled, name, scope, id, bind_id, thread_id,
      timestamp, nullptr, flags);
}

inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    unsigned int flags,
    uint64_t bind_id) {
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id,
      TRACE_EVENT_API_CURRENT_THREAD_ID, TRACE_TIME_TICKS_NOW(), flags,
      bind_id);
}

template <class ARG1_TYPE>
inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    unsigned int flags,
    uint64_t bind_id,
    const char* arg1_name,
    ARG1_TYPE&& arg1_val) {
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id,
      TRACE_EVENT_API_CURRENT_THREAD_ID, TRACE_TIME_TICKS_NOW(), flags, bind_id,
      arg1_name, std::forward<ARG1_TYPE>(arg1_val));
}

template <class ARG1_TYPE, class ARG2_TYPE>
inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    uint64_t id,
    unsigned int flags,
    uint64_t bind_id,
    const char* arg1_name,
    ARG1_TYPE&& arg1_val,
    const char* arg2_name,
    ARG2_TYPE&& arg2_val) {
  return AddTraceEventWithThreadIdAndTimestamp(
      phase, category_group_enabled, name, scope, id,
      TRACE_EVENT_API_CURRENT_THREAD_ID, TRACE_TIME_TICKS_NOW(), flags, bind_id,
      arg1_name, std::forward<ARG1_TYPE>(arg1_val), arg2_name,
      std::forward<ARG2_TYPE>(arg2_val));
}

template <class ARG1_TYPE>
void AddMetadataEvent(const unsigned char* category_group_enabled,
                      const char* event_name,
                      const char* arg_name,
                      ARG1_TYPE&& arg_val) {
  base::trace_event::TraceArguments args(arg_name,
                                         std::forward<ARG1_TYPE>(arg_val));
  trace_event_internal::AddMetadataEvent(category_group_enabled, event_name,
                                         &args, TRACE_EVENT_FLAG_NONE);
}

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

// Used by TRACE_EVENTx macros. Do not use directly.
class TRACE_EVENT_API_CLASS_EXPORT ScopedTracer {
 public:
  ScopedTracer() = default;

  ~ScopedTracer() {
    if (category_group_enabled_ && *category_group_enabled_) {
      TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(category_group_enabled_,
                                                  name_, event_handle_);
    }
  }

  void Initialize(const unsigned char* category_group_enabled,
                  const char* name,
                  base::trace_event::TraceEventHandle event_handle) {
    category_group_enabled_ = category_group_enabled;
    name_ = name;
    event_handle_ = event_handle;
  }

 private:
  // NOTE: Only initialize the first member to reduce generated code size,
  // since there is no point in initializing the other members if Initialize()
  // is never called.
  const unsigned char* category_group_enabled_ = nullptr;
  const char* name_;
  base::trace_event::TraceEventHandle event_handle_;
};

// Used by TRACE_EVENT_BINARY_EFFICIENTx macro. Do not use directly.
class TRACE_EVENT_API_CLASS_EXPORT ScopedTraceBinaryEfficient {
 public:
  ScopedTraceBinaryEfficient(const char* category_group, const char* name);
  ~ScopedTraceBinaryEfficient();

 private:
  const unsigned char* category_group_enabled_;
  const char* name_;
  base::trace_event::TraceEventHandle event_handle_;
};

// This macro generates less code then TRACE_EVENT0 but is also
// slower to execute when tracing is off. It should generally only be
// used with code that is seldom executed or conditionally executed
// when debugging.
// For now the category_group must be "gpu".
#define TRACE_EVENT_BINARY_EFFICIENT0(category_group, name) \
    trace_event_internal::ScopedTraceBinaryEfficient \
        INTERNAL_TRACE_EVENT_UID(scoped_trace)(category_group, name);

#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

}  // namespace trace_event_internal

namespace base {
namespace trace_event {

template <typename IDType, const char* category>
class TraceScopedTrackableObject {
 public:
  TraceScopedTrackableObject(const char* name, IDType id)
      : name_(name), id_(id) {
    TRACE_EVENT_OBJECT_CREATED_WITH_ID(category, name_, id_);
  }
  TraceScopedTrackableObject(const TraceScopedTrackableObject&) = delete;
  TraceScopedTrackableObject& operator=(const TraceScopedTrackableObject&) =
      delete;

  template <typename ArgType> void snapshot(ArgType snapshot) {
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(category, name_, id_, snapshot);
  }

  ~TraceScopedTrackableObject() {
    TRACE_EVENT_OBJECT_DELETED_WITH_ID(category, name_, id_);
  }

 private:
  const char* name_;
  IDType id_;
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_TRACE_EVENT_H_

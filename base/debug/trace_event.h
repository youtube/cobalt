// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
// Events can be INSTANT, or can be pairs of BEGIN and END in the same scope:
//   TRACE_EVENT_BEGIN0("MY_SUBSYSTEM", "SomethingCostly")
//   doSomethingCostly()
//   TRACE_EVENT_END0("MY_SUBSYSTEM", "SomethingCostly")
// Note: our tools can't always determine the correct BEGIN/END pairs unless
// these are used in the same scope. Use START/FINISH macros if you need them
// to be in separate scopes.
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
// To trace an asynchronous procedure such as an IPC send/receive, use START and
// FINISH:
//   [single threaded sender code]
//     static int send_count = 0;
//     ++send_count;
//     TRACE_EVENT_START0("ipc", "message", send_count);
//     Send(new MyMessage(send_count));
//   [receive code]
//     void OnMyMessage(send_count) {
//       TRACE_EVENT_FINISH0("ipc", "message", send_count);
//     }
// The third parameter is a unique ID to match START/FINISH pairs.
// START and FINISH can occur on any thread of any traced process. Pointers can
// be used for the ID parameter, and they will be mangled internally so that
// the same pointer on two different processes will not match. For example:
//   class MyTracedClass {
//    public:
//     MyTracedClass() {
//       TRACE_EVENT_START0("category", "MyTracedClass", this);
//     }
//     ~MyTracedClass() {
//       TRACE_EVENT_FINISH0("category", "MyTracedClass", this);
//     }
//   }
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
// Since counters are in a global namespace, you may want to disembiguate with a
// unique ID, by using the TRACE_COUNTER_ID* variations.
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
// Then the category_enabled flag is checked. This is a unsigned char, and
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

////////////////////////////////////////////////////////////////////////////////
// TODO(jbates): split this into a separate header.
// This header is designed to be give you trace_event macros without specifying
// how the events actually get collected and stored. If you need to expose trace
// event to some other universe, you can copy-and-paste this file and just
// implement these API macros.

// unsigned char*
//     TRACE_EVENT_API_GET_CATEGORY_ENABLED(const char* category_name)
#define TRACE_EVENT_API_GET_CATEGORY_ENABLED \
    base::debug::TraceLog::GetCategoryEnabled

// Returns the threshold_begin_id used by TRACE_IF_LONGER_THAN macros.
// int TRACE_EVENT_API_ADD_TRACE_EVENT(
//                    char phase,
//                    const unsigned char* category_enabled,
//                    const char* name,
//                    unsigned long long id,
//                    int num_args,
//                    const char** arg_names,
//                    const unsigned char* arg_types,
//                    const unsigned long long* arg_values,
//                    int threshold_begin_id,
//                    long long threshold,
//                    unsigned char flags)
#define TRACE_EVENT_API_ADD_TRACE_EVENT \
    base::debug::TraceLog::GetInstance()->AddTraceEvent

// void TRACE_EVENT_API_ADD_COUNTER_EVENT(
//                    const unsigned char* category_enabled,
//                    const char* name,
//                    unsigned long long id,
//                    const char* arg1_name, int arg1_val,
//                    const char* arg2_name, int arg2_val,
//                    unsigned char flags)
#define TRACE_EVENT_API_ADD_COUNTER_EVENT \
    base::debug::TraceLog::GetInstance()->AddCounterEvent

// Mangle |pointer| with a process ID hash so that if |pointer| occurs on more
// than one process, it will not collide in the trace data.
// unsigned long long TRACE_EVENT_API_GET_ID_FROM_POINTER(void* pointer)
#define TRACE_EVENT_API_GET_ID_FROM_POINTER \
    base::debug::TraceLog::GetInstance()->GetInterProcessID

////////////////////////////////////////////////////////////////////////////////

// By default, const char* argument values are assumed to have long-lived scope
// and will not be copied. Use this macro to force a const char* to be copied.
#define TRACE_STR_COPY(str) \
    trace_event_internal::TraceStringWithCopy(str)

// Older style trace macros with explicit id and extra data
// Only these macros result in publishing data to ETW as currently implemented.
#define TRACE_EVENT_BEGIN_ETW(name, id, extra) \
    base::debug::TraceLog::AddTraceEventEtw( \
        TRACE_EVENT_PHASE_BEGIN, \
        name, reinterpret_cast<const void*>(id), extra)

#define TRACE_EVENT_END_ETW(name, id, extra) \
    base::debug::TraceLog::AddTraceEventEtw( \
        TRACE_EVENT_PHASE_END, \
        name, reinterpret_cast<const void*>(id), extra)

#define TRACE_EVENT_INSTANT_ETW(name, id, extra) \
    base::debug::TraceLog::AddTraceEventEtw( \
        TRACE_EVENT_PHASE_INSTANT, \
        name, reinterpret_cast<const void*>(id), extra)

// Records a pair of begin and end events called "name" for the current
// scope, with 0, 1 or 2 associated arguments. If the category is not
// enabled, then this does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT0(category, name) \
    INTERNAL_TRACE_EVENT_ADD_SCOPED(category, name)
#define TRACE_EVENT1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_SCOPED(category, name, arg1_name, arg1_val)
#define TRACE_EVENT2(category, name, arg1_name, arg1_val, arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_SCOPED(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val)

// Same as TRACE_EVENT except that they are not included in official builds.
#ifdef OFFICIAL_BUILD
#define UNSHIPPED_TRACE_EVENT0(category, name) (void)0
#define UNSHIPPED_TRACE_EVENT1(category, name, arg1_name, arg1_val) (void)0
#define UNSHIPPED_TRACE_EVENT2(category, name, arg1_name, arg1_val, \
                               arg2_name, arg2_val) (void)0
#define UNSHIPPED_TRACE_EVENT_INSTANT0(category, name) (void)0
#define UNSHIPPED_TRACE_EVENT_INSTANT1(category, name, arg1_name, arg1_val) \
    (void)0
#define UNSHIPPED_TRACE_EVENT_INSTANT2(category, name, arg1_name, arg1_val, \
                                       arg2_name, arg2_val) (void)0
#else
#define UNSHIPPED_TRACE_EVENT0(category, name) \
    TRACE_EVENT0(category, name)
#define UNSHIPPED_TRACE_EVENT1(category, name, arg1_name, arg1_val) \
    TRACE_EVENT1(category, name, arg1_name, arg1_val)
#define UNSHIPPED_TRACE_EVENT2(category, name, arg1_name, arg1_val, \
                               arg2_name, arg2_val) \
    TRACE_EVENT2(category, name, arg1_name, arg1_val, arg2_name, arg2_val)
#define UNSHIPPED_TRACE_EVENT_INSTANT0(category, name) \
    TRACE_EVENT_INSTANT0(category, name)
#define UNSHIPPED_TRACE_EVENT_INSTANT1(category, name, arg1_name, arg1_val) \
    TRACE_EVENT_INSTANT1(category, name, arg1_name, arg1_val)
#define UNSHIPPED_TRACE_EVENT_INSTANT2(category, name, arg1_name, arg1_val, \
                                       arg2_name, arg2_val) \
    TRACE_EVENT_INSTANT2(category, name, arg1_name, arg1_val, \
                         arg2_name, arg2_val)
#endif

// Records a single event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_INSTANT0(category, name) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, \
        category, name, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_INSTANT1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, \
        category, name, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_INSTANT2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, \
        category, name, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val, \
        arg2_name, arg2_val)
#define TRACE_EVENT_COPY_INSTANT0(category, name) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, \
        category, name, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_INSTANT1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, \
        category, name, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_INSTANT2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, \
        category, name, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, \
        arg2_name, arg2_val)

// Records a single BEGIN event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_BEGIN0(category, name) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, \
        category, name, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_BEGIN1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, \
        category, name, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_BEGIN2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, \
        category, name, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val, \
        arg2_name, arg2_val)
#define TRACE_EVENT_COPY_BEGIN0(category, name) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, \
        category, name, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_BEGIN1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, \
        category, name, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_BEGIN2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, \
        category, name, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, \
        arg2_name, arg2_val)

// Records a single END event for "name" immediately. If the category
// is not enabled, then this does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_END0(category, name) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, \
        category, name, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_END1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, \
        category, name, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_END2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, \
        category, name, TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val, \
        arg2_name, arg2_val)
#define TRACE_EVENT_COPY_END0(category, name) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, \
        category, name, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_END1(category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, \
        category, name, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_END2(category, name, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, \
        category, name, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, \
        arg2_name, arg2_val)

// Time threshold event:
// Only record the event if the duration is greater than the specified
// threshold_us (time in microseconds).
// Records a pair of begin and end events called "name" for the current
// scope, with 0, 1 or 2 associated arguments. If the category is not
// enabled, then this does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_IF_LONGER_THAN0(threshold_us, category, name) \
    INTERNAL_TRACE_EVENT_ADD_SCOPED_IF_LONGER_THAN(threshold_us, category, name)
#define TRACE_EVENT_IF_LONGER_THAN1( \
    threshold_us, category, name, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_SCOPED_IF_LONGER_THAN( \
        threshold_us, category, name, arg1_name, arg1_val)
#define TRACE_EVENT_IF_LONGER_THAN2( \
    threshold_us, category, name, arg1_name, arg1_val, arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_SCOPED_IF_LONGER_THAN( \
        threshold_us, category, name, arg1_name, arg1_val, arg2_name, arg2_val)

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
        category, name, trace_event_internal::kNoEventId, \
        value1_name, value1_val, value2_name, value2_val, \
        TRACE_EVENT_FLAG_NONE)
#define TRACE_COPY_COUNTER2(category, name, value1_name, value1_val, \
        value2_name, value2_val) \
    INTERNAL_TRACE_EVENT_ADD_COUNTER( \
        category, name, trace_event_internal::kNoEventId, \
        value1_name, value1_val, value2_name, value2_val, \
        TRACE_EVENT_FLAG_COPY)

// Records the value of a counter called "name" immediately. Value
// must be representable as a 32 bit integer.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - |id| is used to disambiguate counters with the same name. It must either
//   be a pointer or an integer value up to 64 bits. If it's a pointer, the bits
//   will be xored with a hash of the process ID so that the same pointer on
//   two different processes will not collide.
#define TRACE_COUNTER_ID1(category, name, id, value) \
    TRACE_COUNTER_ID2(category, name, id, "value", value, NULL, 0)
#define TRACE_COPY_COUNTER_ID1(category, name, id, value) \
    TRACE_COPY_COUNTER_ID2(category, name, id, "value", value, NULL, 0)

// Records the values of a multi-parted counter called "name" immediately.
// The UI will treat value1 and value2 as parts of a whole, displaying their
// values as a stacked-bar chart.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - |id| is used to disambiguate counters with the same name. It must either
//   be a pointer or an integer value up to 64 bits. If it's a pointer, the bits
//   will be xored with a hash of the process ID so that the same pointer on
//   two different processes will not collide.
#define TRACE_COUNTER_ID2(category, name, id, value1_name, value1_val, \
        value2_name, value2_val) \
    INTERNAL_TRACE_EVENT_ADD_COUNTER( \
        category, name, id, value1_name, value1_val, value2_name, value2_val, \
        TRACE_EVENT_FLAG_HAS_ID)
#define TRACE_COPY_COUNTER_ID2(category, name, id, value1_name, value1_val, \
        value2_name, value2_val) \
    INTERNAL_TRACE_EVENT_ADD_COUNTER( \
        category, name, id, \
        value1_name, value1_val, \
        value2_name, value2_val, \
        TRACE_EVENT_FLAG_COPY|TRACE_EVENT_FLAG_HAS_ID)


// Records a single START event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - |id| is used to match the START event with the FINISH event. It must either
//   be a pointer or an integer value up to 64 bits. If it's a pointer, the bits
//   will be xored with a hash of the process ID so that the same pointer on
//   two different processes will not collide.
#define TRACE_EVENT_START0(category, name, id) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_START, \
        category, name, id, TRACE_EVENT_FLAG_HAS_ID)
#define TRACE_EVENT_START1(category, name, id, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_START, \
        category, name, id, TRACE_EVENT_FLAG_HAS_ID, arg1_name, arg1_val)
#define TRACE_EVENT_START2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_START, \
        category, name, id, TRACE_EVENT_FLAG_HAS_ID, \
        arg1_name, arg1_val, arg2_name, arg2_val)
#define TRACE_EVENT_COPY_START0(category, name, id) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_START, \
        category, name, id, TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_START1(category, name, id, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_START, \
        category, name, id, TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_COPY, \
        arg1_name, arg1_val)
#define TRACE_EVENT_COPY_START2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_START, \
        category, name, id, TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_COPY, \
        arg1_name, arg1_val, arg2_name, arg2_val)

// Records a single FINISH event for "name" immediately. If the category
// is not enabled, then this does nothing.
#define TRACE_EVENT_FINISH0(category, name, id) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FINISH, \
        category, name, id, TRACE_EVENT_FLAG_HAS_ID)
#define TRACE_EVENT_FINISH1(category, name, id, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FINISH, \
        category, name, id, TRACE_EVENT_FLAG_HAS_ID, arg1_name, arg1_val)
#define TRACE_EVENT_FINISH2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FINISH, \
        category, name, id, TRACE_EVENT_FLAG_HAS_ID, \
        arg1_name, arg1_val, arg2_name, arg2_val)
#define TRACE_EVENT_COPY_FINISH0(category, name, id) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FINISH, \
        category, name, id, TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_FINISH1(category, name, id, arg1_name, arg1_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FINISH, \
        category, name, id, TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_COPY, \
        arg1_name, arg1_val)
#define TRACE_EVENT_COPY_FINISH2(category, name, id, arg1_name, arg1_val, \
        arg2_name, arg2_val) \
    INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FINISH, \
        category, name, id, TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_COPY, \
        arg1_name, arg1_val, arg2_name, arg2_val)


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
    static const unsigned char* INTERNAL_TRACE_EVENT_UID(catstatic) = NULL; \
    ANNOTATE_BENIGN_RACE(&INTERNAL_TRACE_EVENT_UID(catstatic), \
                         "trace_event category"); \
    if (!INTERNAL_TRACE_EVENT_UID(catstatic)) \
      INTERNAL_TRACE_EVENT_UID(catstatic) = \
          TRACE_EVENT_API_GET_CATEGORY_ENABLED(category);

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD(phase, category, name, flags, ...) \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
    if (*INTERNAL_TRACE_EVENT_UID(catstatic)) { \
      trace_event_internal::AddTraceEvent( \
          phase, INTERNAL_TRACE_EVENT_UID(catstatic), name, \
          trace_event_internal::kNoEventId, flags, ##__VA_ARGS__); \
    }

// Implementation detail: internal macro to create static category and
// add the counter event if it is enabled.
#define INTERNAL_TRACE_EVENT_ADD_COUNTER( \
      category, name, id, arg1_name, arg1_val, arg2_name, arg2_val, flags) \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
    if (*INTERNAL_TRACE_EVENT_UID(catstatic)) { \
      TRACE_EVENT_API_ADD_COUNTER_EVENT( \
          INTERNAL_TRACE_EVENT_UID(catstatic), \
          name, trace_event_internal::TraceID(id).data(), \
          arg1_name, arg1_val, arg2_name, arg2_val, flags); \
    }

// Implementation detail: internal macro to create static category and add begin
// event if the category is enabled. Also adds the end event when the scope
// ends.
#define INTERNAL_TRACE_EVENT_ADD_SCOPED(category, name, ...) \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
    trace_event_internal::TraceEndOnScopeClose \
        INTERNAL_TRACE_EVENT_UID(profileScope); \
    if (*INTERNAL_TRACE_EVENT_UID(catstatic)) { \
      trace_event_internal::AddTraceEvent( \
          TRACE_EVENT_PHASE_BEGIN, \
          INTERNAL_TRACE_EVENT_UID(catstatic), \
          name, trace_event_internal::kNoEventId, \
          TRACE_EVENT_FLAG_NONE, ##__VA_ARGS__); \
      INTERNAL_TRACE_EVENT_UID(profileScope).Initialize( \
          INTERNAL_TRACE_EVENT_UID(catstatic), name); \
    }

// Implementation detail: internal macro to create static category and add begin
// event if the category is enabled. Also adds the end event when the scope
// ends. If the elapsed time is < threshold time, the begin/end pair is erased.
#define INTERNAL_TRACE_EVENT_ADD_SCOPED_IF_LONGER_THAN(threshold, \
        category, name, ...) \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
    trace_event_internal::TraceEndOnScopeCloseThreshold \
        INTERNAL_TRACE_EVENT_UID(profileScope); \
    if (*INTERNAL_TRACE_EVENT_UID(catstatic)) { \
      int INTERNAL_TRACE_EVENT_UID(begin_event_id) = \
        trace_event_internal::AddTraceEvent( \
            TRACE_EVENT_PHASE_BEGIN, \
            INTERNAL_TRACE_EVENT_UID(catstatic), \
            name, trace_event_internal::kNoEventId, \
            TRACE_EVENT_FLAG_NONE, ##__VA_ARGS__); \
      INTERNAL_TRACE_EVENT_UID(profileScope).Initialize( \
          INTERNAL_TRACE_EVENT_UID(catstatic), name, \
          INTERNAL_TRACE_EVENT_UID(begin_event_id), threshold); \
    }

// Implementation detail: internal macro to create static category and add
// event if the category is enabled.
#define INTERNAL_TRACE_EVENT_ADD_WITH_ID(phase, category, name, id, flags, \
                                         ...) \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category); \
    if (*INTERNAL_TRACE_EVENT_UID(catstatic)) { \
      trace_event_internal::AddTraceEvent( \
          phase, INTERNAL_TRACE_EVENT_UID(catstatic), \
          name, trace_event_internal::TraceID(id).data(), flags, \
          ##__VA_ARGS__); \
    }

template <typename Type>
struct StaticMemorySingletonTraits;

namespace base {

class RefCountedString;

namespace debug {

const int kTraceMaxNumArgs = 2;

// Notes regarding the following definitions:
// New values can be added and propagated to third party libraries, but existing
// definitions must never be changed, because third party libraries may use old
// definitions.

// Phase indicates the nature of an event entry. E.g. part of a begin/end pair.
#define TRACE_EVENT_PHASE_BEGIN    ('B')
#define TRACE_EVENT_PHASE_END      ('E')
#define TRACE_EVENT_PHASE_INSTANT  ('I')
#define TRACE_EVENT_PHASE_START    ('S')
#define TRACE_EVENT_PHASE_FINISH   ('F')
#define TRACE_EVENT_PHASE_METADATA ('M')
#define TRACE_EVENT_PHASE_COUNTER  ('C')

// Flags for changing the behavior of TRACE_EVENT_API_ADD_TRACE_EVENT.
#define TRACE_EVENT_FLAG_NONE        (static_cast<unsigned char>(0))
#define TRACE_EVENT_FLAG_COPY        (static_cast<unsigned char>(1<<0))
#define TRACE_EVENT_FLAG_HAS_ID      (static_cast<unsigned char>(1<<1))

// Type values for identifying types in the TraceValue union.
#define TRACE_VALUE_TYPE_BOOL         (static_cast<unsigned char>(1))
#define TRACE_VALUE_TYPE_UINT         (static_cast<unsigned char>(2))
#define TRACE_VALUE_TYPE_INT          (static_cast<unsigned char>(3))
#define TRACE_VALUE_TYPE_DOUBLE       (static_cast<unsigned char>(4))
#define TRACE_VALUE_TYPE_POINTER      (static_cast<unsigned char>(5))
#define TRACE_VALUE_TYPE_STRING       (static_cast<unsigned char>(6))
#define TRACE_VALUE_TYPE_COPY_STRING  (static_cast<unsigned char>(7))

// Output records are "Events" and can be obtained via the
// OutputCallback whenever the tracing system decides to flush. This
// can happen at any time, on any thread, or you can programatically
// force it to happen.
class BASE_EXPORT TraceEvent {
 public:
  union TraceValue {
    bool as_bool;
    unsigned long long as_uint;
    long long as_int;
    double as_double;
    const void* as_pointer;
    const char* as_string;
  };

  TraceEvent();
  TraceEvent(int thread_id,
             TimeTicks timestamp,
             char phase,
             const unsigned char* category_enabled,
             const char* name,
             unsigned long long id,
             int num_args,
             const char** arg_names,
             const unsigned char* arg_types,
             const unsigned long long* arg_values,
             unsigned char flags);
  ~TraceEvent();

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
  // Note: these are ordered by size (largest first) for optimal packing.
  TimeTicks timestamp_;
  // id_ can be used to store phase-specific data.
  unsigned long long id_;
  TraceValue arg_values_[kTraceMaxNumArgs];
  const char* arg_names_[kTraceMaxNumArgs];
  const unsigned char* category_enabled_;
  const char* name_;
  scoped_refptr<base::RefCountedString> parameter_copy_storage_;
  int thread_id_;
  char phase_;
  unsigned char flags_;
  unsigned char arg_types_[kTraceMaxNumArgs];
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
  void SetOutputCallback(const OutputCallback& json_chunk_callback);

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
  typedef base::Callback<void(const scoped_refptr<RefCountedString>&)>
      OutputCallback;
  void SetOutputCallback(const OutputCallback& cb);

  // The trace buffer does not flush dynamically, so when it fills up,
  // subsequent trace events will be dropped. This callback is generated when
  // the trace buffer is full. The callback must be thread safe.
  typedef base::Callback<void(void)> BufferFullCallback;
  void SetBufferFullCallback(const BufferFullCallback& cb);

  // Flushes all logged data to the callback.
  void Flush();

  // Called by TRACE_EVENT* macros, don't call this directly.
  static const unsigned char* GetCategoryEnabled(const char* name);
  static const char* GetCategoryName(const unsigned char* category_enabled);

  // Called by TRACE_EVENT* macros, don't call this directly.
  // Returns the index in the internal vector of the event if it was added, or
  //         -1 if the event was not added.
  // On end events, the return value of the begin event can be specified along
  // with a threshold in microseconds. If the elapsed time between begin and end
  // is less than the threshold, the begin/end event pair is dropped.
  // If |copy| is set, |name|, |arg_name1| and |arg_name2| will be deep copied
  // into the event; see "Memory scoping note" and TRACE_EVENT_COPY_XXX above.
  int AddTraceEvent(char phase,
                    const unsigned char* category_enabled,
                    const char* name,
                    unsigned long long id,
                    int num_args,
                    const char** arg_names,
                    const unsigned char* arg_types,
                    const unsigned long long* arg_values,
                    int threshold_begin_id,
                    long long threshold,
                    unsigned char flags);
  static void AddTraceEventEtw(char phase,
                               const char* name,
                               const void* id,
                               const char* extra);
  static void AddTraceEventEtw(char phase,
                               const char* name,
                               const void* id,
                               const std::string& extra);

  // A wrapper around AddTraceEvent used by TRACE_COUNTERx macros
  // that allows only integer values for the counters.
  void AddCounterEvent(const unsigned char* category_enabled,
                       const char* name,
                       unsigned long long id,
                       const char* arg1_name, int arg1_val,
                       const char* arg2_name, int arg2_val,
                       unsigned char flags);

  // Mangle |ptr| with a hash based on the process ID so that if |ptr| occurs on
  // more than one process, it will not collide.
  unsigned long long GetInterProcessID(void* ptr) const {
    return static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(ptr)) ^
           process_id_hash_;
  }

  int process_id() const { return process_id_; }

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

  void SetProcessID(int process_id);

 private:
  // This allows constructor and destructor to be private and usable only
  // by the Singleton class.
  friend struct StaticMemorySingletonTraits<TraceLog>;

  TraceLog();
  ~TraceLog();
  const unsigned char* GetCategoryEnabledInternal(const char* name);
  void AddThreadNameMetadataEvents();
  void AddClockSyncMetadataEvents();

  // TODO(nduca): switch to per-thread trace buffers to reduce thread
  // synchronization.
  Lock lock_;
  bool enabled_;
  OutputCallback output_callback_;
  BufferFullCallback buffer_full_callback_;
  std::vector<TraceEvent> logged_events_;
  std::vector<std::string> included_categories_;
  std::vector<std::string> excluded_categories_;

  base::hash_map<int, std::string> thread_names_;

  // XORed with TraceID to make it unlikely to collide with other processes.
  unsigned long long process_id_hash_;

  int process_id_;

  DISALLOW_COPY_AND_ASSIGN(TraceLog);
};

}  // namespace debug
}  // namespace base

namespace trace_event_internal {

// Specify these values when the corresponding argument of AddTraceEvent is not
// used.
const int kZeroNumArgs = 0;
const int kNoThreshholdBeginId = -1;
const long long kNoThresholdValue = 0;
const unsigned long long kNoEventId = 0;

// TraceID encapsulates an ID that can either be an integer or pointer. Pointers
// are mangled with the Process ID so that they are unlikely to collide when the
// same pointer is used on different processes.
class BASE_EXPORT TraceID {
 public:
  explicit TraceID(void* rhs) :
      data_(TRACE_EVENT_API_GET_ID_FROM_POINTER(rhs)) {}
  explicit TraceID(unsigned long long rhs) : data_(rhs) {}
  explicit TraceID(unsigned long rhs) : data_(rhs) {}
  explicit TraceID(unsigned int rhs) : data_(rhs) {}
  explicit TraceID(unsigned short rhs) : data_(rhs) {}
  explicit TraceID(unsigned char rhs) : data_(rhs) {}
  explicit TraceID(long long rhs) :
      data_(static_cast<unsigned long long>(rhs)) {}
  explicit TraceID(long rhs) : data_(static_cast<unsigned long long>(rhs)) {}
  explicit TraceID(int rhs) : data_(static_cast<unsigned long long>(rhs)) {}
  explicit TraceID(short rhs) : data_(static_cast<unsigned long long>(rhs)) {}
  explicit TraceID(signed char rhs) :
      data_(static_cast<unsigned long long>(rhs)) {}

  unsigned long long data() const { return data_; }

 private:
  unsigned long long data_;
};

// Simple union to store various types as unsigned long long.
union TraceValueUnion {
  bool as_bool;
  unsigned long long as_uint;
  long long as_int;
  double as_double;
  const void* as_pointer;
  const char* as_string;
};

// Simple container for const char* that should be copied instead of retained.
class TraceStringWithCopy {
 public:
  explicit TraceStringWithCopy(const char* str) : str_(str) {}
  operator const char* () const { return str_; }
 private:
  const char* str_;
};

// Define SetTraceValue for each allowed type. It stores the type and
// value in the return arguments. This allows this API to avoid declaring any
// structures so that it is portable to third_party libraries.
#define INTERNAL_DECLARE_SET_TRACE_VALUE(actual_type, \
                                         union_member, \
                                         value_type_id) \
    static inline void SetTraceValue(actual_type arg, \
                                     unsigned char* type, \
                                     unsigned long long* value) { \
      TraceValueUnion type_value; \
      type_value.union_member = arg; \
      *type = value_type_id; \
      *value = type_value.as_uint; \
    }
// Simpler form for int types that can be safely casted.
#define INTERNAL_DECLARE_SET_TRACE_VALUE_INT(actual_type, \
                                             value_type_id) \
    static inline void SetTraceValue(actual_type arg, \
                                     unsigned char* type, \
                                     unsigned long long* value) { \
      *type = value_type_id; \
      *value = static_cast<unsigned long long>(arg); \
    }

INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned long long, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned int, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned short, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned char, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(long long, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(int, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(short, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(signed char, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE(bool, as_bool, TRACE_VALUE_TYPE_BOOL)
INTERNAL_DECLARE_SET_TRACE_VALUE(double, as_double, TRACE_VALUE_TYPE_DOUBLE)
INTERNAL_DECLARE_SET_TRACE_VALUE(const void*, as_pointer,
                                 TRACE_VALUE_TYPE_POINTER)
INTERNAL_DECLARE_SET_TRACE_VALUE(const char*, as_string,
                                 TRACE_VALUE_TYPE_STRING)
INTERNAL_DECLARE_SET_TRACE_VALUE(const TraceStringWithCopy&, as_string,
                                 TRACE_VALUE_TYPE_COPY_STRING)

#undef INTERNAL_DECLARE_SET_TRACE_VALUE
#undef INTERNAL_DECLARE_SET_TRACE_VALUE_INT

// std::string version of SetTraceValue so that trace arguments can be strings.
static inline void SetTraceValue(const std::string& arg,
                                 unsigned char* type,
                                 unsigned long long* value) {
  TraceValueUnion type_value;
  type_value.as_string = arg.c_str();
  *type = TRACE_VALUE_TYPE_COPY_STRING;
  *value = type_value.as_uint;
}

// These AddTraceEvent template functions are defined here instead of in the
// macro, because the arg_values could be temporary objects, such as
// std::string. In order to store pointers to the internal c_str and pass
// through to the tracing API, the arg_values must live throughout
// these procedures.

static inline int AddTraceEvent(char phase,
                                const unsigned char* category_enabled,
                                const char* name,
                                unsigned long long id,
                                unsigned char flags) {
  return TRACE_EVENT_API_ADD_TRACE_EVENT(
      phase, category_enabled, name, id,
      kZeroNumArgs, NULL, NULL, NULL,
      kNoThreshholdBeginId, kNoThresholdValue, flags);
}

template<class ARG1_TYPE>
static inline int AddTraceEvent(char phase,
                                const unsigned char* category_enabled,
                                const char* name,
                                unsigned long long id,
                                unsigned char flags,
                                const char* arg1_name,
                                const ARG1_TYPE& arg1_val) {
  const int num_args = 1;
  unsigned char arg_types[1];
  unsigned long long arg_values[1];
  SetTraceValue(arg1_val, &arg_types[0], &arg_values[0]);
  return TRACE_EVENT_API_ADD_TRACE_EVENT(
      phase, category_enabled, name, id,
      num_args, &arg1_name, arg_types, arg_values,
      kNoThreshholdBeginId, kNoThresholdValue, flags);
}

template<class ARG1_TYPE, class ARG2_TYPE>
static inline int AddTraceEvent(char phase,
                                const unsigned char* category_enabled,
                                const char* name,
                                unsigned long long id,
                                unsigned char flags,
                                const char* arg1_name,
                                const ARG1_TYPE& arg1_val,
                                const char* arg2_name,
                                const ARG2_TYPE& arg2_val) {
  const int num_args = 2;
  const char* arg_names[2] = { arg1_name, arg2_name };
  unsigned char arg_types[2];
  unsigned long long arg_values[2];
  SetTraceValue(arg1_val, &arg_types[0], &arg_values[0]);
  SetTraceValue(arg2_val, &arg_types[1], &arg_values[1]);
  return TRACE_EVENT_API_ADD_TRACE_EVENT(
      phase, category_enabled, name, id,
      num_args, arg_names, arg_types, arg_values,
      kNoThreshholdBeginId, kNoThresholdValue, flags);
}

// Used by TRACE_EVENTx macro. Do not use directly.
class BASE_EXPORT TraceEndOnScopeClose {
 public:
  // Note: members of data_ intentionally left uninitialized. See Initialize.
  TraceEndOnScopeClose() : p_data_(NULL) {}
  ~TraceEndOnScopeClose() {
    if (p_data_)
      AddEventIfEnabled();
  }

  void Initialize(const unsigned char* category_enabled,
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
    const unsigned char* category_enabled;
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
  void Initialize(const unsigned char* category_enabled,
                  const char* name,
                  int threshold_begin_id,
                  long long threshold);

 private:
  // Add the end event if the category is still enabled.
  void AddEventIfEnabled();

  // This Data struct workaround is to avoid initializing all the members
  // in Data during construction of this object, since this object is always
  // constructed, even when tracing is disabled. If the members of Data were
  // members of this class instead, compiler warnings occur about potential
  // uninitialized accesses.
  struct Data {
    long long threshold;
    const unsigned char* category_enabled;
    const char* name;
    int threshold_begin_id;
  };
  Data* p_data_;
  Data data_;
};

}  // namespace trace_event_internal

#endif  // BASE_DEBUG_TRACE_EVENT_H_

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Trace events to track application performance.  Events consist of a name
// a type (BEGIN, END or INSTANT), a tracking id and extra string data.
// In addition, the current process id, thread id, a timestamp down to the
// microsecond and a file and line number of the calling location.
//
// The current implementation logs these events into a log file of the form
// trace_<pid>.log where it's designed to be post-processed to generate a
// trace report.  In the future, it may use another mechansim to facilitate
// real-time analysis.

#ifndef BASE_DEBUG_TRACE_EVENT_H_
#define BASE_DEBUG_TRACE_EVENT_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)
// On Windows we always pull in an alternative implementation
// which logs to Event Tracing for Windows.
//
// Note that the Windows implementation is always enabled, irrespective the
// value of the CHROMIUM_ENABLE_TRACE_EVENT define. The Windows implementation
// is controlled by Event Tracing for Windows, which will turn tracing on only
// if there is someone listening for the events it generates.
#include "base/debug/trace_event_win.h"
#else  // defined(OS_WIN)

#include <string>

#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "base/timer.h"

#ifndef CHROMIUM_ENABLE_TRACE_EVENT
#define TRACE_EVENT_BEGIN(name, id, extra) ((void) 0)
#define TRACE_EVENT_END(name, id, extra) ((void) 0)
#define TRACE_EVENT_INSTANT(name, id, extra) ((void) 0)

#else  // CHROMIUM_ENABLE_TRACE_EVENT
// Use the following macros rather than using the TraceLog class directly as the
// underlying implementation may change in the future.  Here's a sample usage:
// TRACE_EVENT_BEGIN("v8.run", documentId, scriptLocation);
// RunScript(script);
// TRACE_EVENT_END("v8.run", documentId, scriptLocation);

// Record that an event (of name, id) has begun.  All BEGIN events should have
// corresponding END events with a matching (name, id).
#define TRACE_EVENT_BEGIN(name, id, extra) \
  base::debug::TraceLog::GetInstance()->Trace( \
      name, \
      base::debug::TraceLog::EVENT_BEGIN, \
      reinterpret_cast<const void*>(id), \
      extra, \
      __FILE__, \
      __LINE__)

// Record that an event (of name, id) has ended.  All END events should have
// corresponding BEGIN events with a matching (name, id).
#define TRACE_EVENT_END(name, id, extra) \
  base::debug::TraceLog::GetInstance()->Trace( \
      name, \
      base::debug::TraceLog::EVENT_END, \
      reinterpret_cast<const void*>(id), \
      extra, \
      __FILE__, \
      __LINE__)

// Record that an event (of name, id) with no duration has happened.
#define TRACE_EVENT_INSTANT(name, id, extra) \
  base::debug::TraceLog::GetInstance()->Trace( \
      name, \
      base::debug::TraceLog::EVENT_INSTANT, \
      reinterpret_cast<const void*>(id), \
      extra, \
      __FILE__, \
      __LINE__)
#endif  // CHROMIUM_ENABLE_TRACE_EVENT

namespace base {

class ProcessMetrics;

namespace debug {

class TraceLog {
 public:
  enum EventType {
    EVENT_BEGIN,
    EVENT_END,
    EVENT_INSTANT
  };

  static TraceLog* GetInstance();

  // Is tracing currently enabled.
  static bool IsTracing();
  // Start logging trace events.
  static bool StartTracing();
  // Stop logging trace events.
  static void StopTracing();

  // Log a trace event of (name, type, id) with the optional extra string.
  void Trace(const std::string& name,
             EventType type,
             const void* id,
             const std::wstring& extra,
             const char* file,
             int line);
  void Trace(const std::string& name,
             EventType type,
             const void* id,
             const std::string& extra,
             const char* file,
             int line);

 private:
  // This allows constructor and destructor to be private and usable only
  // by the Singleton class.
  friend struct DefaultSingletonTraits<TraceLog>;

  TraceLog();
  ~TraceLog();
  bool OpenLogFile();
  void CloseLogFile();
  bool Start();
  void Stop();
  void Heartbeat();
  void Log(const std::string& msg);

  bool enabled_;
  FILE* log_file_;
  base::Lock file_lock_;
  TimeTicks trace_start_time_;
  scoped_ptr<base::ProcessMetrics> process_metrics_;
  RepeatingTimer<TraceLog> timer_;
};

}  // namespace debug
}  // namespace base

#endif  // defined(OS_WIN)

#endif  // BASE_DEBUG_TRACE_EVENT_H_

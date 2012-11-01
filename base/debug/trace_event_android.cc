// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_impl.h"

#include <fcntl.h>

#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/stringprintf.h"

namespace {

int g_atrace_fd = -1;
const char* kATraceMarkerFile = "/sys/kernel/debug/tracing/trace_marker";

}  // namespace

namespace base {
namespace debug {

// static
void TraceLog::InitATrace() {
  DCHECK(g_atrace_fd == -1);
  g_atrace_fd = open(kATraceMarkerFile, O_WRONLY | O_APPEND);
  if (g_atrace_fd == -1)
    LOG(WARNING) << "Couldn't open " << kATraceMarkerFile;
}

// static
bool TraceLog::IsATraceEnabled() {
  return g_atrace_fd != -1;
}

void TraceLog::SendToATrace(char phase,
                            const char* category,
                            const char* name,
                            int num_args,
                            const char** arg_names,
                            const unsigned char* arg_types,
                            const unsigned long long* arg_values) {
  if (!IsATraceEnabled())
    return;

  switch (phase) {
    case TRACE_EVENT_PHASE_BEGIN:
    case TRACE_EVENT_PHASE_INSTANT: {
      std::string out = StringPrintf("B|%d|%s-%s", getpid(), category, name);
      for (int i = 0; i < num_args; ++i) {
        out += '|';
        out += arg_names[i];
        out += '=';
        TraceEvent::TraceValue value;
        value.as_uint = arg_values[i];
        std::string::size_type value_start = out.length();
        TraceEvent::AppendValueAsJSON(arg_types[i], value, &out);
        // Remove the quotes which may confuse the atrace script.
        ReplaceSubstringsAfterOffset(&out, value_start, "\\\"", "'");
        ReplaceSubstringsAfterOffset(&out, value_start, "\"", "");
      }
      write(g_atrace_fd, out.c_str(), out.size());

      if (phase != TRACE_EVENT_PHASE_INSTANT)
        break;
      // Fall through. Simulate an instance event with a pair of begin/end.
    }
    case TRACE_EVENT_PHASE_END: {
      // Though a single 'E' is enough, here append pid and name so that
      // unpaired events can be found easily.
      std::string out = StringPrintf("E|%d|%s-%s", getpid(), category, name);
      write(g_atrace_fd, out.c_str(), out.size());
      break;
    }
    case TRACE_EVENT_PHASE_COUNTER:
      for (int i = 0; i < num_args; ++i) {
        DCHECK(arg_types[i] == TRACE_VALUE_TYPE_INT);
        std::string out = StringPrintf(
            "C|%d|%s-%s-%s|%d",
            getpid(), category, name,
            arg_names[i], static_cast<int>(arg_values[i]));
        write(g_atrace_fd, out.c_str(), out.size());
      }
      break;

    default:
      // Do nothing.
      break;
  }
}

void TraceLog::AddClockSyncMetadataEvents() {
  // Since Android does not support sched_setaffinity, we cannot establish clock
  // sync unless the scheduler clock is set to global. If the trace_clock file
  // can't be read, we will assume the kernel doesn't support tracing and do
  // nothing.
  std::string clock_mode;
  if (!file_util::ReadFileToString(
      FilePath("/sys/kernel/debug/tracing/trace_clock"), &clock_mode))
    return;

  if (clock_mode != "local [global]\n") {
    DLOG(WARNING) <<
        "The kernel's tracing clock must be set to global in order for " <<
        "trace_event to be synchronized with . Do this by\n" <<
        "  echo global > /sys/kerel/debug/tracing/trace_clock";
    return;
  }

  int atrace_fd = g_atrace_fd;
  if (atrace_fd == -1) {
    // This function may be called when atrace is not enabled.
    atrace_fd = open(kATraceMarkerFile, O_WRONLY | O_APPEND);
    if (atrace_fd == -1) {
      LOG(WARNING) << "Couldn't open " << kATraceMarkerFile;
      return;
    }
  }

  // Android's kernel trace system has a trace_marker feature: this is a file on
  // debugfs that takes the written data and pushes it onto the trace
  // buffer. So, to establish clock sync, we write our monotonic clock into that
  // trace buffer.
  TimeTicks now = TimeTicks::NowFromSystemTraceTime();
  double now_in_seconds = now.ToInternalValue() / 1000000.0;
  std::string marker = StringPrintf(
      "trace_event_clock_sync: parent_ts=%f\n", now_in_seconds);
  if (write(atrace_fd, marker.c_str(), marker.size()) != 0) {
    DLOG(WARNING) << "Couldn't write to " << kATraceMarkerFile << ": "
                  << strerror(errno);
  }

  if (g_atrace_fd == -1)
    close(atrace_fd);
}

}  // namespace debug
}  // namespace base

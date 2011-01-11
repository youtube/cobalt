// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"

#include "base/format_macros.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "base/time.h"

#define USE_UNRELIABLE_NOW

namespace base {
namespace debug {

static const char* kEventTypeNames[] = {
  "BEGIN",
  "END",
  "INSTANT"
};

static const FilePath::CharType* kLogFileName =
    FILE_PATH_LITERAL("trace_%d.log");

// static
TraceLog* TraceLog::GetInstance() {
  return Singleton<TraceLog, DefaultSingletonTraits<TraceLog> >::get();
}

// static
bool TraceLog::IsTracing() {
  return TraceLog::GetInstance()->enabled_;
}

// static
bool TraceLog::StartTracing() {
  return TraceLog::GetInstance()->Start();
}

// static
void TraceLog::StopTracing() {
  return TraceLog::GetInstance()->Stop();
}

void TraceLog::Trace(const std::string& name,
                     EventType type,
                     const void* id,
                     const std::wstring& extra,
                     const char* file,
                     int line) {
  if (!enabled_)
    return;
  Trace(name, type, id, WideToUTF8(extra), file, line);
}

void TraceLog::Trace(const std::string& name,
                     EventType type,
                     const void* id,
                     const std::string& extra,
                     const char* file,
                     int line) {
  if (!enabled_)
    return;

#ifdef USE_UNRELIABLE_NOW
  TimeTicks tick = TimeTicks::HighResNow();
#else
  TimeTicks tick = TimeTicks::Now();
#endif
  TimeDelta delta = tick - trace_start_time_;
  int64 usec = delta.InMicroseconds();
  std::string msg =
    StringPrintf("{'pid':'0x%lx', 'tid':'0x%lx', 'type':'%s', "
                 "'name':'%s', 'id':'%p', 'extra':'%s', 'file':'%s', "
                 "'line_number':'%d', 'usec_begin': %" PRId64 "},\n",
                 static_cast<unsigned long>(base::GetCurrentProcId()),
                 static_cast<unsigned long>(PlatformThread::CurrentId()),
                 kEventTypeNames[type],
                 name.c_str(),
                 id,
                 extra.c_str(),
                 file,
                 line,
                 usec);

  Log(msg);
}

TraceLog::TraceLog() : enabled_(false), log_file_(NULL) {
  base::ProcessHandle proc = base::GetCurrentProcessHandle();
#if !defined(OS_MACOSX)
  process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(proc));
#else
  // The default port provider is sufficient to get data for the current
  // process.
  process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(proc,
                                                                    NULL));
#endif
}

TraceLog::~TraceLog() {
  Stop();
}

bool TraceLog::OpenLogFile() {
  FilePath::StringType pid_filename =
      StringPrintf(kLogFileName, base::GetCurrentProcId());
  FilePath log_file_path;
  if (!PathService::Get(base::DIR_EXE, &log_file_path))
    return false;
  log_file_path = log_file_path.Append(pid_filename);
  log_file_ = file_util::OpenFile(log_file_path, "a");
  if (!log_file_) {
    // try the current directory
    log_file_ = file_util::OpenFile(FilePath(pid_filename), "a");
    if (!log_file_) {
      return false;
    }
  }
  return true;
}

void TraceLog::CloseLogFile() {
  if (log_file_) {
    file_util::CloseFile(log_file_);
  }
}

bool TraceLog::Start() {
  if (enabled_)
    return true;
  enabled_ = OpenLogFile();
  if (enabled_) {
    Log("var raw_trace_events = [\n");
    trace_start_time_ = TimeTicks::Now();
    timer_.Start(TimeDelta::FromMilliseconds(250), this, &TraceLog::Heartbeat);
  }
  return enabled_;
}

void TraceLog::Stop() {
  if (enabled_) {
    enabled_ = false;
    Log("];\n");
    CloseLogFile();
    timer_.Stop();
  }
}

void TraceLog::Heartbeat() {
  std::string cpu = StringPrintf("%.0f", process_metrics_->GetCPUUsage());
  TRACE_EVENT_INSTANT("heartbeat.cpu", 0, cpu);
}

void TraceLog::Log(const std::string& msg) {
  AutoLock lock(file_lock_);

  fprintf(log_file_, "%s", msg.c_str());
}

}  // namespace debug
}  // namespace base

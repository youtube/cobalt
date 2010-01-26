// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging_win.h"
#include "base/atomicops.h"
#include "base/singleton.h"
#include <initguid.h>  // NOLINT

namespace {

struct LogEventProviderTraits {
  // WARNING: User has to deal with get() returning NULL.
  static logging::LogEventProvider* New() {
    if (base::subtle::NoBarrier_AtomicExchange(&dead_, 1))
      return NULL;
    logging::LogEventProvider* ptr =
        reinterpret_cast<logging::LogEventProvider*>(buffer_);
    // We are protected by a memory barrier.
    new(ptr) logging::LogEventProvider();
    return ptr;
  }

  static void Delete(logging::LogEventProvider* p) {
    base::subtle::NoBarrier_Store(&dead_, 1);
    MemoryBarrier();
    p->logging::LogEventProvider::~LogEventProvider();
  }

  static const bool kRegisterAtExit = true;

 private:
  static const size_t kBufferSize = (sizeof(logging::LogEventProvider) +
                                     sizeof(intptr_t) - 1) / sizeof(intptr_t);
  static intptr_t buffer_[kBufferSize];

  // Signal the object was already deleted, so it is not revived.
  static base::subtle::Atomic32 dead_;
};

intptr_t LogEventProviderTraits::buffer_[kBufferSize];
base::subtle::Atomic32 LogEventProviderTraits::dead_ = 0;

Singleton<logging::LogEventProvider, LogEventProviderTraits> log_provider;

}  // namespace

namespace logging {

DEFINE_GUID(kLogEventId,
    0x7fe69228, 0x633e, 0x4f06, 0x80, 0xc1, 0x52, 0x7f, 0xea, 0x23, 0xe3, 0xa7);

LogEventProvider::LogEventProvider() : old_log_level_(LOG_NONE) {
}

bool LogEventProvider::LogMessage(int severity, const std::string& message) {
  EtwEventLevel level = TRACE_LEVEL_NONE;

  // Convert the log severity to the most appropriate ETW trace level.
  switch (severity) {
    case LOG_INFO:
      level = TRACE_LEVEL_INFORMATION;
      break;
    case LOG_WARNING:
      level = TRACE_LEVEL_WARNING;
      break;
    case LOG_ERROR:
    case LOG_ERROR_REPORT:
      level = TRACE_LEVEL_ERROR;
      break;
    case LOG_FATAL:
      level = TRACE_LEVEL_FATAL;
      break;
  };

  // Bail if we're not logging, not at that level,
  // or if we're post-atexit handling.
  LogEventProvider* provider = log_provider.get();
  if (provider == NULL || level > provider->enable_level())
    return false;

  // And now log the event, with stack trace if one is
  // requested per our enable flags.
  if (provider->enable_flags() & ENABLE_STACK_TRACE_CAPTURE) {
    const size_t kMaxBacktraceDepth = 32;
    void* backtrace[kMaxBacktraceDepth];
    DWORD depth = CaptureStackBackTrace(2, kMaxBacktraceDepth, backtrace, NULL);
    EtwMofEvent<3> event(kLogEventId, LOG_MESSAGE_WITH_STACKTRACE, level);

    event.SetField(0, sizeof(depth), &depth);
    event.SetField(1, sizeof(backtrace[0]) * depth, &backtrace);
    event.SetField(2, message.length() + 1, message.c_str());

    provider->Log(event.get());
  } else {
    EtwMofEvent<1> event(kLogEventId, LOG_MESSAGE, level);
    event.SetField(0, message.length() + 1, message.c_str());
    provider->Log(event.get());
  }

  // Don't increase verbosity in other log destinations.
  if (severity >= provider->old_log_level_)
    return true;

  return false;
}

void LogEventProvider::Initialize(const GUID& provider_name) {
  LogEventProvider* provider = log_provider.get();

  provider->set_provider_name(provider_name);
  provider->Register();

  // Register our message handler with logging.
  SetLogMessageHandler(LogMessage);
}

void LogEventProvider::Uninitialize() {
  log_provider.get()->Unregister();
}

void LogEventProvider::OnEventsEnabled() {
  // Grab the old log level so we can restore it later.
  old_log_level_ = GetMinLogLevel();

  // Convert the new trace level to a logging severity
  // and enable logging at that level.
  EtwEventLevel level = enable_level();
  switch (level) {
    case TRACE_LEVEL_NONE:
    case TRACE_LEVEL_FATAL:
      SetMinLogLevel(LOG_FATAL);
      break;
    case TRACE_LEVEL_ERROR:
      SetMinLogLevel(LOG_ERROR);
      break;
    case TRACE_LEVEL_WARNING:
      SetMinLogLevel(LOG_WARNING);
      break;
    case TRACE_LEVEL_INFORMATION:
    case TRACE_LEVEL_VERBOSE:
      SetMinLogLevel(LOG_INFO);
      break;
  }
}

void LogEventProvider::OnEventsDisabled() {
  // Restore the old log level.
  SetMinLogLevel(old_log_level_);
}

}  // namespace logging

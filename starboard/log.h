// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Module Overview: Starboard Logging module
//
// Defines core debug logging functions.

#ifndef STARBOARD_LOG_H_
#define STARBOARD_LOG_H_

#include <stdarg.h>

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/system.h"

#ifdef __cplusplus
extern "C" {
#endif

// The priority at which a message should be logged. The platform may be
// configured to filter logs by priority, or render them differently.
typedef enum SbLogPriority {
  kSbLogPriorityUnknown = 0,
  kSbLogPriorityInfo,
  kSbLogPriorityWarning,
  kSbLogPriorityError,
  kSbLogPriorityFatal,
} SbLogPriority;

// Writes |message| to the platform's debug output log. This method is
// thread-safe, and responsible for ensuring that the output from multiple
// threads is not mixed.
//
// |priority|: The SbLogPriority at which the message should be logged. Note
//   that passing |kSbLogPriorityFatal| does not terminate the program. Such a
//   policy must be enforced at the application level. In fact, |priority| may
//   be completely ignored on many platforms.
// |message|: The message to be logged. No formatting is required to be done
//   on the value, including newline termination. That said, platforms can
//   adjust the message to be more suitable for their output method by
//   wrapping the text, stripping unprintable characters, etc.
SB_EXPORT void SbLog(SbLogPriority priority, const char* message);

// A bare-bones log output method that is async-signal-safe, i.e. safe to call
// from an asynchronous signal handler (e.g. a |SIGSEGV| handler). It should not
// do any additional formatting.
//
// |message|: The message to be logged.
SB_EXPORT void SbLogRaw(const char* message);

// Dumps the stack of the current thread to the log in an async-signal-safe
// manner, i.e. safe to call from an asynchronous signal handler (e.g. a
// |SIGSEGV| handler). Does not include SbLogRawDumpStack itself.
//
// |frames_to_skip|: The number of frames to skip from the top of the stack
//   when dumping the current thread to the log. This parameter lets you remove
//   noise from helper functions that might end up on top of every stack dump
//   so that the stack dump is just the relevant function stack where the
//   problem occurred.
SB_EXPORT void SbLogRawDumpStack(int frames_to_skip);

// A formatted log output method that is async-signal-safe, i.e. safe to call
// from an asynchronous signal handler (e.g. a |SIGSEGV| handler).
SB_EXPORT void SbLogRawFormat(const char* format, va_list args)
    SB_PRINTF_FORMAT(1, 0);

// Inline wrapper of SbLogFormat to convert from ellipsis to va_args.
static SB_C_INLINE void SbLogRawFormatF(const char* format, ...)
    SB_PRINTF_FORMAT(1, 2);
void SbLogRawFormatF(const char* format, ...) {
  va_list args;
  va_start(args, format);
  SbLogRawFormat(format, args);
  va_end(args);
}

// A log output method that additionally performs a string format on the
// data being logged.
SB_EXPORT void SbLogFormat(const char* format, va_list args)
    SB_PRINTF_FORMAT(1, 0);

// Inline wrapper of SbLogFormat that converts from ellipsis to va_args.
static SB_C_INLINE void SbLogFormatF(const char* format, ...)
    SB_PRINTF_FORMAT(1, 2);
void SbLogFormatF(const char* format, ...) {
  va_list args;
  va_start(args, format);
  SbLogFormat(format, args);
  va_end(args);
}

// Flushes the log buffer on some platforms. This method is safe to call from
// multiple threads.
SB_EXPORT void SbLogFlush();

// Indicates whether the log output goes to a TTY or is being redirected.
SB_EXPORT bool SbLogIsTty();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_LOG_H_

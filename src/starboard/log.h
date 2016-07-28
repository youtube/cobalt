// Copyright 2015 Google Inc. All Rights Reserved.
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

// Debug logging.

#ifndef STARBOARD_LOG_H_
#define STARBOARD_LOG_H_

#include <stdarg.h>

#ifdef __cplusplus
#include <sstream>
#include <string>
#endif

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

// Writes |message| at |priority| to the debug output log for this platform.
// Passing kSbLogPriorityFatal will not terminate the program, such policy must
// be enforced at the application level. |priority| may, in fact, be completely
// ignored on many platforms. No formatting is required to be done on the
// message, not even newline termination. That said, platforms may wish to
// adjust the message to be more suitable for their output method. This could be
// wrapping the text, or stripping unprintable characters.
SB_EXPORT void SbLog(SbLogPriority priority, const char* message);

// A bare-bones log output method that is async-signal-safe, i.e. safe to call
// from an asynchronous signal handler (e.g. a SIGSEGV handler). It should do no
// additional formatting.
SB_EXPORT void SbLogRaw(const char* message);

// Dumps the stack of the current thread to the log in an async-signal-safe
// manner, i.e. safe to call from an asynchronous signal handler (e.g. a SIGSEGV
// handler). Does not include SbLogRawDumpStack itself, and will additionally
// skip |frames_to_skip| frames from the top of the stack.
SB_EXPORT void SbLogRawDumpStack(int frames_to_skip);

// A formatted log output method that is async-signal-safe, i.e. safe to call
// from an asynchronous signal handler (e.g. a SIGSEGV handler).
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

// A log output method, that additionally performs a string format on the way
// out.
SB_EXPORT void SbLogFormat(const char* format, va_list args)
    SB_PRINTF_FORMAT(1, 0);

// Inline wrapper of SbLogFormat to convert from ellipsis to va_args.
static SB_C_INLINE void SbLogFormatF(const char* format, ...)
    SB_PRINTF_FORMAT(1, 2);
void SbLogFormatF(const char* format, ...) {
  va_list args;
  va_start(args, format);
  SbLogFormat(format, args);
  va_end(args);
}

// Flushes the log buffer, on some platforms.
SB_EXPORT void SbLogFlush();

// Returns whether the log output goes to a TTY or is being redirected.
SB_EXPORT bool SbLogIsTty();

#ifdef __cplusplus
}  // extern "C"
#endif

#if defined(__LB_SHELL__FOR_RELEASE__)
#define SB_LOGGING_IS_OFFICIAL_BUILD 1
#else
#define SB_LOGGING_IS_OFFICIAL_BUILD 0
#endif

#ifdef __cplusplus
// If we are a C++ program, then we provide a selected subset of base/logging
// macros and assertions. See that file for more comments.

namespace starboard {
namespace logging {

SB_EXPORT void SetMinLogLevel(SbLogPriority level);
SB_EXPORT SbLogPriority GetMinLogLevel();
SB_EXPORT void Break();

// An object which will dumps the stack to the given ostream, without adding any
// frames of its own. |skip_frames| is the number of frames to skip in the dump.
struct SB_EXPORT Stack {
  explicit Stack(int skip_frames) : skip_frames(skip_frames) {}
  int skip_frames;
};
SB_EXPORT std::ostream& operator<<(std::ostream& out, const Stack& stack);

SB_EXPORT std::ostream& operator<<(std::ostream& out, const wchar_t* wstr);
inline std::ostream& operator<<(std::ostream& out, const std::wstring& wstr) {
  return out << wstr.c_str();
}

#if defined(__cplusplus_winrt)
inline std::ostream& operator<<(std::ostream& out, ::Platform::String ^ str) {
  return out << std::wstring(str->Begin(), str->End());
}
#endif

const SbLogPriority SB_LOG_INFO = kSbLogPriorityInfo;
const SbLogPriority SB_LOG_WARNING = kSbLogPriorityWarning;
const SbLogPriority SB_LOG_ERROR = kSbLogPriorityError;
const SbLogPriority SB_LOG_FATAL = kSbLogPriorityFatal;

class SB_EXPORT LogMessage {
 public:
  LogMessage(const char* file, int line, SbLogPriority priority);
  ~LogMessage();

  std::ostream& stream() { return stream_; }

 private:
  void Init(const char* file, int line);

  SbLogPriority priority_;
  std::ostringstream stream_;
  size_t message_start_;  // Offset of the start of the message (past prefix
                          // info).
  // The file and line information passed in to the constructor.
  const char* file_;
  const int line_;
};

class LogMessageVoidify {
 public:
  LogMessageVoidify() {}
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) {}
};

}  // namespace logging
}  // namespace starboard

#define SB_LOG_MESSAGE_INFO                            \
  ::starboard::logging::LogMessage(__FILE__, __LINE__, \
                                   ::starboard::logging::SB_LOG_INFO)
#define SB_LOG_MESSAGE_WARNING                         \
  ::starboard::logging::LogMessage(__FILE__, __LINE__, \
                                   ::starboard::logging::SB_LOG_WARNING)
#define SB_LOG_MESSAGE_ERROR                           \
  ::starboard::logging::LogMessage(__FILE__, __LINE__, \
                                   ::starboard::logging::SB_LOG_ERROR)
#define SB_LOG_MESSAGE_FATAL                           \
  ::starboard::logging::LogMessage(__FILE__, __LINE__, \
                                   ::starboard::logging::SB_LOG_FATAL)

#define SB_LOG_STREAM(severity) SB_LOG_MESSAGE_##severity.stream()
#define SB_LAZY_STREAM(stream, condition) \
  !(condition) ? (void)0 : ::starboard::logging::LogMessageVoidify() & (stream)

#define SB_LOG_IS_ON(severity)                  \
  ((::starboard::logging::SB_LOG_##severity) >= \
    ::starboard::logging::GetMinLogLevel())
#define SB_LOG_IF(severity, condition) \
  SB_LAZY_STREAM(SB_LOG_STREAM(severity), SB_LOG_IS_ON(severity) && (condition))
#define SB_LOG(severity) SB_LOG_IF(severity, true)
#define SB_EAT_STREAM_PARAMETERS SB_LOG_IF(INFO, false)
#define SB_STACK_IF(severity, condition) \
  SB_LOG_IF(severity, condition) << "\n" << ::starboard::logging::Stack(0)
#define SB_STACK(severity) SB_STACK_IF(severity, true)

#if SB_LOGGING_IS_OFFICIAL_BUILD
#define SB_CHECK(condition) \
  !(condition) ? ::starboard::logging::Break() : SB_EAT_STREAM_PARAMETERS
#define SB_DCHECK(condition) SB_EAT_STREAM_PARAMETERS
#elif defined(_PREFAST_)
#define SB_CHECK(condition) \
  __analysis_assume(condition), SB_EAT_STREAM_PARAMETERS
#define SB_DCHECK(condition) SB_CHECK(condition)
#else
#define SB_CHECK(condition) \
  SB_LOG_IF(FATAL, !(condition)) << "Check failed: " #condition ". "
#define SB_DCHECK(condition) SB_CHECK(condition)
#endif  // SB_LOGGING_IS_OFFICIAL_BUILD

#if SB_LOGGING_IS_OFFICIAL_BUILD
#define SB_DLOG_IS_ON(severity) false
#define SB_DLOG_IF(severity, condition) SB_EAT_STREAM_PARAMETERS
#else  // SB_LOGGING_IS_OFFICIAL_BUILD
#define SB_DLOG_IS_ON(severity) SB_LOG_IS_ON(severity)
#define SB_DLOG_IF(severity, condition) SB_LOG_IF(severity, condition)
#endif  // SB_LOGGING_IS_OFFICIAL_BUILD

#define SB_DLOG(severity) SB_DLOG_IF(severity, SB_DLOG_IS_ON(severity))
#define SB_DSTACK(severity) SB_STACK_IF(severity, SB_DLOG_IS_ON(severity))
#define SB_NOTREACHED() SB_DCHECK(false)

#if SB_IS(COMPILER_GCC)
#define SB_NOTIMPLEMENTED_MSG \
  "Not implemented reached in " << __PRETTY_FUNCTION__
#else
#define SB_NOTIMPLEMENTED_MSG "Not implemented reached in " << __FUNCTION__
#endif

#if !defined(SB_NOTIMPLEMENTED_POLICY)
#if SB_LOGGING_IS_OFFICIAL_BUILD
#define SB_NOTIMPLEMENTED_POLICY 0
#else
#define SB_NOTIMPLEMENTED_POLICY 5
#endif
#endif  // !defined(SB_NOTIMPLEMENTED_POLICY)

#if SB_NOTIMPLEMENTED_POLICY == 0
#define SB_NOTIMPLEMENTED() SB_EAT_STREAM_PARAMETERS
#elif SB_NOTIMPLEMENTED_POLICY == 1
#define SB_NOTIMPLEMENTED() SB_COMPILE_ASSERT(false, NOT_IMPLEMENTED)
#elif SB_NOTIMPLEMENTED_POLICY == 2
#define SB_NOTIMPLEMENTED() SB_COMPILE_ASSERT(false, NOT_IMPLEMENTED)
#elif SB_NOTIMPLEMENTED_POLICY == 3
#define SB_NOTIMPLEMENTED() SB_NOTREACHED()
#elif SB_NOTIMPLEMENTED_POLICY == 4
#define SB_NOTIMPLEMENTED() SB_LOG(ERROR) << SB_NOTIMPLEMENTED_MSG
#elif SB_NOTIMPLEMENTED_POLICY == 5
#define SB_NOTIMPLEMENTED()                                  \
  do {                                                       \
    static int count = 0;                                    \
    SB_LOG_IF(ERROR, 0 == count++) << SB_NOTIMPLEMENTED_MSG; \
  } while (0);                                               \
  SB_EAT_STREAM_PARAMETERS
#endif

#else  // __cplusplus
// We also provide a very small subset for straight-C users.

#define SB_NOTIMPLEMENTED_IN(X) "Not implemented reached in " #X
#if SB_IS(COMPILER_GCC)
#define SB_NOTIMPLEMENTED_MSG SB_NOTIMPLEMENTED_IN(__PRETTY_FUNCTION__)
#else
#define SB_NOTIMPLEMENTED_MSG SB_NOTIMPLEMENTED_IN(__FUNCTION__)
#endif

#define SB_CHECK(condition)                                        \
  do {                                                             \
    if (!(condition)) {                                            \
      SbLog(kSbLogPriorityError, "Check failed: " #condition "."); \
      SbSystemBreakIntoDebugger();                                 \
    }                                                              \
  } while (0)

#if SB_LOGGING_IS_OFFICIAL_BUILD
#define SB_NOTIMPLEMENTED()
#define SB_DCHECK(condition)
#else
#define SB_DCHECK(condition) SB_CHECK(condition)
#define SB_NOTIMPLEMENTED()                              \
  do {                                                   \
    static int count = 0;                                \
    if (0 == count++) {                                  \
      SbLog(kSbLogPriorityError, SB_NOTIMPLEMENTED_MSG); \
    }                                                    \
  } while (0)
#endif

#define SB_NOTREACHED() SB_DCHECK(false)

#endif  // __cplusplus

#endif  // STARBOARD_LOG_H_

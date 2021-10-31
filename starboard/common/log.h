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
// Implements a set of convenience functions and macros built on top of the core
// Starboard logging API.

#ifndef STARBOARD_COMMON_LOG_H_
#define STARBOARD_COMMON_LOG_H_

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/system.h"

#ifdef __cplusplus

extern "C++" {

#include <sstream>
#include <string>

#if defined(COBALT_BUILD_TYPE_GOLD)
#define SB_LOGGING_IS_OFFICIAL_BUILD 1
#else
#define SB_LOGGING_IS_OFFICIAL_BUILD 0
#endif

// This file provides a selected subset of the //base/logging/ macros and
// assertions. See those files for more comments and details.

namespace starboard {
namespace logging {

void SetMinLogLevel(SbLogPriority level);
SbLogPriority GetMinLogLevel();
SbLogPriority StringToLogLevel(const std::string& log_level);
void Break();

// An object which will dumps the stack to the given ostream, without adding any
// frames of its own. |skip_frames| is the number of frames to skip in the dump.
struct Stack {
  explicit Stack(int skip_frames);
  int skip_frames;
};
std::ostream& operator<<(std::ostream& out, const Stack& stack);

std::ostream& operator<<(std::ostream& out, const wchar_t* wstr);
std::ostream& operator<<(std::ostream& out, const std::wstring& wstr);

#if defined(__cplusplus_winrt)
inline std::ostream& operator<<(std::ostream& out, ::Platform::String ^ str);
#endif

const SbLogPriority SB_LOG_INFO = kSbLogPriorityInfo;
const SbLogPriority SB_LOG_WARNING = kSbLogPriorityWarning;
const SbLogPriority SB_LOG_ERROR = kSbLogPriorityError;
const SbLogPriority SB_LOG_FATAL = kSbLogPriorityFatal;
const SbLogPriority SB_LOG_0 = SB_LOG_ERROR;

// TODO: Export this, e.g. by wrapping in straight-C functions which can then be
// SB_EXPORT'd. Using SB_EXPORT here directly causes issues on Windows because
// it uses std classes which also need to be exported.
class LogMessage {
 public:
  LogMessage(const char* file, int line, SbLogPriority priority);
  ~LogMessage();

  std::ostream& stream();

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
  LogMessageVoidify();
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&);
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

// Compatibility with base/logging.h which defines ERROR to be 0 to workaround
// some system header defines that do the same thing.
#define SB_LOG_MESSAGE_0 SB_LOG_MESSAGE_ERROR

#define SB_LOG_STREAM(severity) SB_LOG_MESSAGE_##severity.stream()
#define SB_LAZY_STREAM(stream, condition) \
  !(condition) ? (void)0 : ::starboard::logging::LogMessageVoidify() & (stream)

#if SB_LOGGING_IS_OFFICIAL_BUILD && !SB_IS(EVERGREEN) && \
    !SB_IS(EVERGREEN_COMPATIBLE)
#define SB_LOG_IS_ON(severity)                         \
  ((::starboard::logging::SB_LOG_##severity >=         \
    ::starboard::logging::SB_LOG_FATAL)                \
       ? ((::starboard::logging::SB_LOG_##severity) >= \
          ::starboard::logging::GetMinLogLevel())      \
       : false)
#else  // SB_LOGGING_IS_OFFICIAL_BUILD
#define SB_LOG_IS_ON(severity)                  \
  ((::starboard::logging::SB_LOG_##severity) >= \
   ::starboard::logging::GetMinLogLevel())
#endif  // SB_LOGGING_IS_OFFICIAL_BUILD

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
#elif defined(_PREFAST_)
#define SB_CHECK(condition) \
  __analysis_assume(condition), SB_EAT_STREAM_PARAMETERS
#else
#define SB_CHECK(condition) \
  SB_LOG_IF(FATAL, !(condition)) << "Check failed: " #condition ". "
#endif  // SB_LOGGING_IS_OFFICIAL_BUILD

#if SB_LOGGING_IS_OFFICIAL_BUILD || \
    (defined(NDEBUG) && !defined(__LB_SHELL__FORCE_LOGGING__))
#define SB_DLOG_IS_ON(severity) false
#define SB_DLOG_IF(severity, condition) SB_EAT_STREAM_PARAMETERS
#else
#define SB_DLOG_IS_ON(severity) SB_LOG_IS_ON(severity)
#define SB_DLOG_IF(severity, condition) SB_LOG_IF(severity, condition)
#endif

#if SB_LOGGING_IS_OFFICIAL_BUILD || \
    (defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON))
#define SB_DCHECK(condition) SB_EAT_STREAM_PARAMETERS
#define SB_DCHECK_ENABLED 0
#else
#define SB_DCHECK(condition) SB_CHECK(condition)
#define SB_DCHECK_ENABLED 1
#endif

#define SB_DLOG(severity) SB_DLOG_IF(severity, SB_DLOG_IS_ON(severity))
#define SB_DSTACK(severity) SB_STACK_IF(severity, SB_DLOG_IS_ON(severity))
#define SB_NOTREACHED() SB_DCHECK(false)

#define SB_NOTIMPLEMENTED_MSG "Not implemented reached in " << SB_FUNCTION

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

}  // extern "C++"

#else  // !__cplusplus

// Provide a very small subset for straight-C users.
#define SB_NOTIMPLEMENTED_IN(X) "Not implemented reached in " #X
#define SB_NOTIMPLEMENTED_MSG SB_NOTIMPLEMENTED_IN(SB_FUNCTION)

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
#define SB_DCHECK_ENABLED 0
#else
#define SB_DCHECK(condition) SB_CHECK(condition)
#define SB_DCHECK_ENABLED 1
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

#endif  // STARBOARD_COMMON_LOG_H_

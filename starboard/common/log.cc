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

#include "starboard/common/log.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "starboard/client_porting/eztime/eztime.h"
#include "starboard/client_porting/poem/string_poem.h"
#include "starboard/common/string.h"
#include "starboard/system.h"
#include "starboard/thread.h"
#include "starboard/time.h"

namespace starboard {
namespace logging {
namespace {
#if SB_LOGGING_IS_OFFICIAL_BUILD
SbLogPriority g_min_log_level = kSbLogPriorityFatal;
#else
SbLogPriority g_min_log_level = kSbLogPriorityUnknown;
#endif

#if defined(COMPILER_MSVC)
#pragma optimize("", off)
#endif

void Alias(const void* var) {}

#if defined(COMPILER_MSVC)
#pragma optimize("", on)
#endif

const char* log_priority_names[] = {
    "UNKNOWN", "INFO", "WARNING", "ERROR", "FATAL",
};
}  // namespace

void SetMinLogLevel(SbLogPriority priority) {
  g_min_log_level = priority;
}

SbLogPriority GetMinLogLevel() {
  return g_min_log_level;
}

SbLogPriority StringToLogLevel(const std::string& log_level) {
  // The command-line arguments are lower-case, so we need to compare against
  // the lower-case logging level string equivalents.
  if (log_level == "warning") {
    return SB_LOG_WARNING;
  } else if (log_level == "error") {
    return SB_LOG_ERROR;
  } else if (log_level == "fatal") {
    return SB_LOG_FATAL;
  }

  return SB_LOG_INFO;
}

void Break() {
  SbSystemBreakIntoDebugger();
}

Stack::Stack(int skip_frames) : skip_frames(skip_frames) {}

std::ostream& operator<<(std::ostream& out, const Stack& stack_token) {
  int skip_frames = stack_token.skip_frames;
  if (skip_frames < 0) {
    skip_frames = 0;
  }

  void* stack[256];
  int count = SbSystemGetStack(stack, SB_ARRAY_SIZE_INT(stack));

  // Skip over DumpStack's stack frame, plus any more requested by the caller.
  for (int i = 1 + skip_frames; i < count; ++i) {
    char symbol[512];
    bool result =
        SbSystemSymbolize(stack[i], symbol, SB_ARRAY_SIZE_INT(symbol));
    out << "\t";
    if (result) {
      out << symbol;
    } else {
      out << "<unknown>";
    }
    out << " [" << stack[i] << "]\n";
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const wchar_t* wstr) {
  // We don't have any good cross-platform wide character to UTF8 converter at
  // this level in the stack, so just throwing out non-ASCII characters.
  // TODO: Convert to UTF8.
  size_t len = wcslen(wstr);
  char* buffer = new char[len + 1];
  size_t pos = 0;
  for (size_t i = 0; i < len; ++i) {
    if (wstr[i] < 128) {
      buffer[pos++] = static_cast<char>(wstr[i]);
    }
  }
  buffer[pos] = '\0';
  out << buffer;
  delete[] buffer;
  return out;
}

std::ostream& operator<<(std::ostream& out, const std::wstring& wstr) {
  return out << wstr.c_str();
}

#if defined(__cplusplus_winrt)
std::ostream& operator<<(std::ostream& out, ::Platform::String ^ str) {
  return out << std::wstring(str->Begin(), str->End());
}
#endif

LogMessage::LogMessage(const char* file, int line, SbLogPriority priority)
    : priority_(priority), file_(file), line_(line) {
  Init(file, line);
}

LogMessage::~LogMessage() {
  stream_ << std::endl;
  // Output the stack trace on Fatal messages.
  if (priority_ == kSbLogPriorityFatal) {
    stream_ << Stack(1);
  }
  std::string str_newline(stream_.str());

  SbLog(priority_, str_newline.c_str());

  if (priority_ == kSbLogPriorityFatal) {
    // Ensure the first characters of the string are on the stack so they
    // are contained in minidumps for diagnostic purposes.
    char str_stack[1024];
    starboard::strlcpy(str_stack, str_newline.c_str(), 1024);

    Alias(str_stack);
    Break();
  }
}

std::ostream& LogMessage::stream() {
  return stream_;
}

void LogMessage::Init(const char* file, int line) {
  std::string filename(file);
  size_t last_slash_pos = filename.find_last_of("\\/");
  if (last_slash_pos != std::string::npos) {
    filename.erase(0, last_slash_pos + 1);
  }

  stream_ << '[';
  stream_ << SbThreadGetId() << ':';
  EzTimeValue time_value;
  EzTimeValueGetNow(&time_value, NULL);
  EzTimeT t = time_value.tv_sec;
  struct EzTimeExploded local_time = {0};
  EzTimeTExplodeLocal(&t, &local_time);
  struct EzTimeExploded* tm_time = &local_time;
  stream_ << std::setfill('0') << std::setw(2) << 1 + tm_time->tm_mon
          << std::setw(2) << tm_time->tm_mday << '/' << std::setw(2)
          << tm_time->tm_hour << std::setw(2) << tm_time->tm_min << std::setw(2)
          << tm_time->tm_sec << '.' << std::setw(6) << time_value.tv_usec
          << ':';
  stream_ << log_priority_names[priority_];
  stream_ << ":" << filename << "(" << line << ")] ";
  message_start_ = stream_.tellp();
}

LogMessageVoidify::LogMessageVoidify() {}

void LogMessageVoidify::operator&(std::ostream&) {}

}  // namespace logging
}  // namespace starboard

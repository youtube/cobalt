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

#include "starboard/log.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

#include "starboard/client_porting/poem/string_poem.h"
#include "starboard/mutex.h"
#include "starboard/system.h"
#include "starboard/thread.h"
#include "starboard/time.h"

namespace starboard {
namespace logging {

namespace {
SbLogPriority g_min_log_level = kSbLogPriorityUnknown;
SbMutex g_log_mutex = SB_MUTEX_INITIALIZER;

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
#if SB_LOGGING_IS_OFFICIAL_BUILD
  return SB_LOG_FATAL;
#else
  return g_min_log_level;
#endif
}

void Break() {
  SbSystemBreakIntoDebugger();
}

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
  SbMutexAcquire(&g_log_mutex);
  SbLog(priority_, str_newline.c_str());
  SbMutexRelease(&g_log_mutex);
  if (priority_ == kSbLogPriorityFatal) {
    // Ensure the first characters of the string are on the stack so they
    // are contained in minidumps for diagnostic purposes.
    char str_stack[1024];
    const size_t copy_bytes =
        std::min(SB_ARRAY_SIZE(str_stack), str_newline.length() + 1);
    PoemStringCopyN(str_stack, str_newline.c_str(),
                    static_cast<int>(copy_bytes));

    Alias(str_stack);
    Break();
  }
}

void LogMessage::Init(const char* file, int line) {
  std::string filename(file);
  size_t last_slash_pos = filename.find_last_of("\\/");
  if (last_slash_pos != std::string::npos) {
    filename.erase(0, last_slash_pos + 1);
  }

  stream_ << '[';
  stream_ << SbThreadGetId() << ':';
  stream_ << SbTimeGetMonotonicNow() << ':';
  stream_ << log_priority_names[priority_];
  stream_ << ":" << filename << "(" << line << ")] ";
  message_start_ = stream_.tellp();
}

}  // namespace logging
}  // namespace starboard

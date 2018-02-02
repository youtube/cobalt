// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <stdio.h>
#include <string>

#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/string.h"

namespace {
SbMutex log_line_mutex = SB_MUTEX_INITIALIZER;
std::stringstream log_line;
const int kFormatBufferSizeBytes = 16 * 1024;

}  // namespace

void SbLogFormat(const char* format, va_list arguments) {
  // __android_log_vprint() cannot be used here because each call to it
  // will produce a new line in the log, whereas a call to a function in
  // the C vprintf family only produces a new line if fed "\n", etc.  The logic
  // below ensures that a log line is written only when a newline is detected,
  // making Android SbLogFormat() behavior consistent with the expectations of
  // the code that uses it, such as unit test suites.

  char formatted_buffer[kFormatBufferSizeBytes];
  vsprintf(formatted_buffer, format, arguments);

  const char* newline = SbStringFindCharacter(formatted_buffer, '\n');

  SbMutexAcquire(&log_line_mutex);
  std::string buffer_string(formatted_buffer);
  log_line << buffer_string;
  if (newline != NULL) {
    log_line.flush();

    SbLogRaw(log_line.str().c_str());

    log_line.str("");
    log_line.clear();
  }
  SbMutexRelease(&log_line_mutex);
}

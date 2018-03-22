// Copyright 2017 Google Inc. All Rights Reserved.
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

#include <stdio.h>
#include <windows.h>

#include "starboard/shared/win32/log_file_impl.h"

static const int kMaxLogLineChars = 16 * 1024;

namespace sbwin32 = starboard::shared::win32;

void SbLogRawFormat(const char* format, va_list arguments) {
  char log_buffer[kMaxLogLineChars] = {0};
  int result = vsprintf_s(log_buffer, kMaxLogLineChars-1, format, arguments);
  if (result > 0) {
    SbLogRaw(log_buffer);
  } else {
    SbLogRaw("[log line too long]");
  }
}

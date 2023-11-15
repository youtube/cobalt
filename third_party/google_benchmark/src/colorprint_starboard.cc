// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "colorprint.h"

#if SB_API_VERSION >= 16
#include <stdio.h>
#endif
#include <vector>

#include "starboard/string.h"

namespace benchmark {

std::string FormatString(const char* msg, va_list args) {
  va_list args_copy;
  va_copy(args_copy, args);

#if SB_API_VERSION < 16
  int expected_size = ::SbStringFormat(NULL, 0, msg, args_copy);
#else
  int expected_size = ::vsnprintf(NULL, 0, msg, args_copy);
#endif

  va_end(args_copy);

  if (expected_size <= 0) {
    return std::string();
  }

  std::vector<char> buffer(expected_size + 1);
#if SB_API_VERSION < 16
  ::SbStringFormat(buffer.data(), buffer.size(), msg, args);
#else
  ::vsnprintf(buffer.data(), buffer.size(), msg, args);
#endif
  return std::string(buffer.data(), expected_size);
}

std::string FormatString(const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  auto tmp = FormatString(msg, args);
  va_end(args);
  return tmp;
}

void ColorPrintf(std::ostream& out, LogColor color, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  ColorPrintf(out, color, fmt, args);
  va_end(args);
}

void ColorPrintf(std::ostream& out, LogColor color, const char* fmt,
                 va_list args) {
  out << FormatString(fmt, args);
}

bool IsColorTerminal() { return false; }

}  // end namespace benchmark

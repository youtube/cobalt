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

// Module Overview: Starboard String module C++ convenience layer
//
// Implements convenience functions to simplify formatting strings.

#ifndef STARBOARD_COMMON_STRING_H_
#define STARBOARD_COMMON_STRING_H_

#include <stdarg.h>

#include <string>
#include <vector>

#include "starboard/configuration.h"
#include "starboard/string.h"

namespace starboard {

SB_C_INLINE std::string FormatString(const char* format, ...)
    SB_PRINTF_FORMAT(1, 2);

SB_C_INLINE std::string FormatString(const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  int expected_size = ::SbStringFormat(NULL, 0, format, arguments);
  va_end(arguments);

  std::string result;
  if (expected_size <= 0) {
    return result;
  }

  std::vector<char> buffer(expected_size + 1);
  va_start(arguments, format);
  ::SbStringFormat(buffer.data(), buffer.size(), format, arguments);
  va_end(arguments);
  return std::string(buffer.data(), expected_size);
}

}  // namespace starboard

#endif  // STARBOARD_COMMON_STRING_H_

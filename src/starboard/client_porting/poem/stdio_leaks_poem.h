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

// This header plugs in the leaks of stdio functions called as std::<function>.

#ifndef STARBOARD_CLIENT_PORTING_POEM_STDIO_LEAKS_POEM_H_
#define STARBOARD_CLIENT_PORTING_POEM_STDIO_LEAKS_POEM_H_

#include <cstdio>

#include "starboard/string.h"

namespace std {

inline int SbStringFormat(char* buffer,
                          size_t size,
                          const char* format,
                          va_list arguments) {
  return ::SbStringFormat(buffer, size, format, arguments);
}

}  // namespace std

#undef __builtin_vsnprintf
#define __builtin_vsnprintf SbStringFormat
#undef vsnprintf
#define vsnprintf SbStringFormat

#endif  // STARBOARD_CLIENT_PORTING_POEM_STDIO_LEAKS_POEM_H_

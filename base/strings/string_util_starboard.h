// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef BASE_STRING_UTIL_STARBOARD_H_
#define BASE_STRING_UTIL_STARBOARD_H_

#include <stdarg.h>
#if SB_API_VERSION >= 16
#include <stdio.h>
#endif

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "starboard/common/string.h"
#include "starboard/memory.h"
#include "starboard/types.h"

namespace base {

inline int vsnprintf(char* buffer, size_t size,
                     const char* format, va_list arguments) {
  return ::vsnprintf(buffer, size, format, arguments);
}

inline int c16SbMemoryCompare(const char16* s1, const char16* s2, size_t n) {
  // We cannot call memcmp because that changes the semantics.
  while (n-- > 0) {
    if (*s1 != *s2) {
      // We cannot use (*s1 - *s2) because char16 is unsigned.
      return ((*s1 < *s2) ? -1 : 1);
    }
    ++s1;
    ++s2;
  }
  return 0;
}

inline const char16_t* as_u16cstr(const wchar_t* str) {
  return reinterpret_cast<const char16_t*>(str);
}

inline const char16_t* as_u16cstr(WStringPiece str) {
  return reinterpret_cast<const char16_t*>(str.data());
}

BASE_EXPORT bool IsStringASCII(WStringPiece str);

}  // namespace base

#endif  // BASE_STRING_UTIL_STARBOARD_H_

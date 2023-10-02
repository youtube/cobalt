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

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "starboard/common/string.h"
#include "starboard/memory.h"
#include "starboard/types.h"

namespace base {

inline char* strdup(const char* str) {
  return SbStringDuplicate(str);
}

inline int strcasecmp(const char* string1, const char* string2) {
  return SbStringCompareNoCase(string1, string2);
}

inline int strncasecmp(const char* string1, const char* string2, size_t count) {
  return SbStringCompareNoCaseN(string1, string2, count);
}

#if defined(vsnprintf)
#undef vsnprintf
#endif

inline int vsnprintf(char* buffer, size_t size,
                     const char* format, va_list arguments) {
  return SbStringFormat(buffer, size, format, arguments);
}

inline int strncmp16(const char16* s1, const char16* s2, size_t count) {
#if defined(WCHAR_T_IS_UTF16)
  return wcsncmp(s1, s2, count);
#elif defined(WCHAR_T_IS_UTF32)
  return c16SbMemoryCompare(s1, s2, count);
#endif
}

inline int vswprintf(wchar_t* buffer, size_t size,
                     const wchar_t* format, va_list arguments) {
  DCHECK(base::IsWprintfFormatPortable(format));
  return SbStringFormatWide(buffer, size, format, arguments);
}

}  // namespace base

#endif  // BASE_STRING_UTIL_STARBOARD_H_

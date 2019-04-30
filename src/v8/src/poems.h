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

// Specialized poems for porting V8 on top of Starboard.  It would have been
// nice to have been able to use the shared poems, however they are too
// aggressive for V8 (such as grabbing identifiers that will come after std::).

#ifndef V8_SRC_POEMS_H_
#define V8_SRC_POEMS_H_

#if !defined(STARBOARD)
#error "Including V8 poems without STARBOARD defined."
#endif

#if !defined(V8_OS_STARBOARD)
#error "Including V8 poems without V8_OS_STARBOARD defined."
#endif

#include "starboard/memory.h"
#include "starboard/string.h"

#ifdef __cplusplus

// Declaring the following functions static inline is not necessary in C++. See:
// http://stackoverflow.com/questions/10847176/should-i-define-static-inline-methods-in-header-file

// Finds the first occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr
inline char* PoemFindCharacterInString(char* str, int character) {
  const char* const_str = static_cast<const char*>(str);
  const char c = static_cast<char>(character);
  return const_cast<char*>(SbStringFindCharacter(const_str, c));
}

// Finds the first occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr
inline const char* PoemFindCharacterInString(const char* str, int character) {
  const char c = static_cast<char>(character);
  return SbStringFindCharacter(str, c);
}

#else

// Finds the first occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr
static SB_C_INLINE char* PoemFindCharacterInString(const char* str,
                                                   int character) {
  // C-style cast used for C code
  return (char*)(SbStringFindCharacter(str, character));
}
#endif

#define malloc(x) SbMemoryAllocate(x)
#define realloc(x, y) SbMemoryReallocate(x, y)
#define free(x) SbMemoryDeallocate(x)
#define memcpy(x, y, z) SbMemoryCopy(x, y, z)
#define calloc(x, y) SbMemoryCalloc(x, y)
#define strdup(s) SbStringDuplicate(s)
#define snprintf(a, b, c, ...) SbStringFormatF(a, b, c, __VA_ARGS__)
#define strchr(s, c) PoemFindCharacterInString(s, c)
#define memchr(s, c, n) SbMemoryFindByte(s, c, n)

#endif  // V8_SRC_POEMS_H_

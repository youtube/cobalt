// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// A poem (POsix EMulation) for functions in string.h

#ifndef STARBOARD_CLIENT_PORTING_POEM_STRING_POEM_H_
#define STARBOARD_CLIENT_PORTING_POEM_STRING_POEM_H_

#if defined(STARBOARD)

#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "starboard/string.h"

#ifdef __cplusplus

// declaring the following 4 functions static inline is not necessary in C++
// see:
// http://stackoverflow.com/questions/10847176/should-i-define-static-inline-methods-in-header-file

// Finds the last occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr, C++ signature
inline char* PoemFindLastCharacterInString(char* str, int character) {
  const char* const_str = static_cast<const char*>(str);
  const char c = static_cast<char>(character);
  return const_cast<char*>(SbStringFindLastCharacter(const_str, c));
}

// Finds the last occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr, C++ signature
inline const char* PoemFindLastCharacterInString(const char* str,
                                                 int character) {
  const char c = static_cast<char>(character);
  return SbStringFindLastCharacter(str, c);
}

// Finds the first occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr
inline char* PoemFindCharacterInString(char* str, int character) {
  const char* const_str = static_cast<const char*>(str);
  const char c = static_cast<char>(character);
  return const_cast<char*>(strchr(const_str, c));
}

// Finds the first occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr
inline const char* PoemFindCharacterInString(const char* str, int character) {
  const char c = static_cast<char>(character);
  return strchr(str, c);
}

#else

// Finds the first occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr
static SB_C_INLINE char* PoemFindCharacterInString(const char* str,
                                                   int character) {
  // C-style cast used for C code
  return (char*)(strchr(str, character));
}

// Finds the last occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr
static SB_C_INLINE char* PoemFindLastCharacterInString(const char* str,
                                                       int character) {
  // C-style cast used for C code
  return (char*)(SbStringFindLastCharacter(str, character));
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Inline wrapper for a drop-in replacement for |strncpy|.  This function
// copies the null terminated string from src to dest.  If the src string is
// shorter than num_chars_to_copy, then null padding is used.
// Warning: As with strncpy spec, if there is no null character within the first
// num_chars_to_copy in src, this function will write a null character at the
// end.
static SB_C_INLINE char* PoemStringCopyN(char* dest,
                                         const char* src,
                                         int num_chars_to_copy) {
  SB_DCHECK(num_chars_to_copy >= 0);
  if (num_chars_to_copy < 0) {
    return dest;
  }

  char* dest_write_iterator = dest;
  char* dest_write_iterator_end = dest + num_chars_to_copy;
  const char* src_iterator = src;

  while ((*src_iterator != '\0') &&
         (dest_write_iterator != dest_write_iterator_end)) {
    *dest_write_iterator = *src_iterator;

    ++src_iterator;
    ++dest_write_iterator;
  }

  SB_DCHECK(dest_write_iterator_end >= dest_write_iterator);
  memset(dest_write_iterator, '\0',
         dest_write_iterator_end - dest_write_iterator);

  return dest;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#if !defined(POEM_NO_EMULATION)

#undef strcpy
#define strcpy(o, s) SbStringCopyUnsafe(o, s)
#undef strncpy
#define strncpy(o, s, ds) PoemStringCopyN(o, s, ds)
#undef strdup
#define strdup(s) SbStringDuplicate(s)
#undef strchr
#define strchr(s, c) PoemFindCharacterInString(s, c)
#undef strrchr
#define strrchr(s, c) PoemFindLastCharacterInString(s, c)
#undef strstr
#define strstr(s, c) SbStringFindString(s, c)
#undef strncmp
#define strncmp(s1, s2, c) SbStringCompare(s1, s2, c)
#undef strcmp
#define strcmp(s1, s2) SbStringCompareAll(s1, s2)
#undef strcspn

// TODO: Replace forward declarations with <cstring> once string_poem is
// trimmed down.
#ifdef __cplusplus
extern "C" {
#endif

#ifndef memmove
void* memmove(void* dest, const void* src, size_t n);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // POEM_NO_EMULATION

#endif  // STARBOARD

#endif  // STARBOARD_CLIENT_PORTING_POEM_STRING_POEM_H_

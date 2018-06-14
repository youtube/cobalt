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

// A poem (POsix EMulation) for functions in string.h

#ifndef STARBOARD_CLIENT_PORTING_POEM_STRING_POEM_H_
#define STARBOARD_CLIENT_PORTING_POEM_STRING_POEM_H_

#if defined(STARBOARD)

#include "starboard/log.h"
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

// Concatenates |source| onto the end of |out_destination|, presuming it has
// atleast strlen(out_destination) + |num_chars_to_copy| + 1 total characters of
// storage available. Returns |out_destination|.  This method is a drop-in
// replacement for strncat.
// Note: even if num_chars_to_copy == 0, we will still write a NULL character.
// This is consistent with the language of linux's strncat man page:
// "Therefore, the size of dest must be at least strlen(dest)+n+1."
static SB_C_INLINE char* PoemStringConcat(char* out_destination,
                                          const char* source,
                                          int num_chars_to_copy) {
  if (num_chars_to_copy <= 0)
    return out_destination;

  int destination_length = (int)(SbStringGetLength(out_destination));

  int destination_size = destination_length + num_chars_to_copy + 1;
  if (destination_size < 0) {  // Did we accidentally overflow?
    destination_size = INT_MAX;
  }
  SbStringConcat(out_destination, source, destination_size);
  return out_destination;
}

// Inline wrapper for an unsafe PoemStringConcat that assumes |out_destination|
// is big enough. Returns |out_destination|.  Meant to be a drop-in replacement
// for strcat.
static SB_C_INLINE char* PoemStringConcatUnsafe(char* out_destination,
                                                const char* source) {
  return PoemStringConcat(out_destination, source,
                          (int)SbStringGetLength(source));
}

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
  SbMemorySet(dest_write_iterator, '\0',
              dest_write_iterator_end - dest_write_iterator);

  return dest;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#if !defined(POEM_NO_EMULATION)

#undef strlen
#define strlen(s) SbStringGetLength(s)
#undef strcpy
#define strcpy(o, s) SbStringCopyUnsafe(o, s)
#undef strncpy
#define strncpy(o, s, ds) PoemStringCopyN(o, s, ds)
#undef strcat
#define strcat(o, s) PoemStringConcatUnsafe(o, s)
#undef strncat
#define strncat(o, s, ds) PoemStringConcat(o, s, ds)
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

#undef memchr
#define memchr(s, c, n) SbMemoryFindByte(s, c, n)
#undef memset
#define memset(s, c, n) SbMemorySet(s, c, n)
#undef memcpy
#define memcpy(d, s, c) SbMemoryCopy(d, s, c)
#undef memcmp
#define memcmp(s1, s2, n) SbMemoryCompare(s1, s2, n)
#undef memmove
#define memmove(d, s, n) SbMemoryMove(d, s, n)

#endif  // POEM_NO_EMULATION

#endif  // STARBOARD

#endif  // STARBOARD_CLIENT_PORTING_POEM_STRING_POEM_H_

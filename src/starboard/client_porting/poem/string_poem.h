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

#include "starboard/string.h"

#ifdef __cplusplus
// Finds the last occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr, C++ signature
inline char* PoemFindLastCharacter(char* str, int character) {
  const char* const_str = static_cast<const char*>(str);
  const char c = static_cast<char>(character);
  return const_cast<char*>(SbStringFindLastCharacter(const_str, c));
}

// Finds the last occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr, C++ signature
inline const char* PoemFindLastCharacter(const char* str, int character) {
  const char c = static_cast<char>(character);
  return SbStringFindLastCharacter(str, c);
}

// Finds the first occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr
inline char* PoemFindCharacter(char* str, int character) {
  const char* const_str = static_cast<const char*>(str);
  const char c = static_cast<char>(character);
  return const_cast<char*>(SbStringFindCharacter(const_str, c));
}

// Finds the first occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr
inline const char* PoemFindCharacter(const char* str, int character) {
  const char c = static_cast<char>(character);
  return SbStringFindCharacter(str, c);
}

#else

// Finds the first occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr
SB_C_INLINE char* PoemFindCharacter(const char* str, int character) {
  // C-style cast used for C code
  return (char*)(SbStringFindCharacter(str, character));
}

// Finds the last occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// Meant to be a drop-in replacement for strchr
SB_C_INLINE char* PoemFindLastCharacter(const char* str, int character) {
  // C-style cast used for C code
  return (char*)(SbStringFindLastCharacter(str, character));
}
#endif

// Concatenates |source| onto the end of |out_destination|, presuming it has
// |destination_size| total characters of storage available. Returns
// |out_destination|.  This method is a drop-in replacement for strncat
SB_C_INLINE char* PoemConcat(char* out_destination,
                             const char* source,
                             int destination_size) {
  SbStringConcat(out_destination, source, destination_size);
  return out_destination;
}

// Inline wrapper for an unsafe PoemConcat that assumes |out_destination| is
// big enough. Returns |out_destination|.  Meant to be a drop-in replacement for
// strcat.
static SB_C_INLINE char* PoemConcatUnsafe(char* out_destination,
                                          const char* source) {
  return PoemConcat(out_destination, source, INT_MAX);
}

#if defined(POEM_FULL_EMULATION) && (POEM_FULL_EMULATION)

#define strlen(s) SbStringGetLength(s)
#define strcpy(o, s) SbStringCopyUnsafe(o, s)
#define strncpy(o, s, ds) SbStringCopy(o, s, ds)
#define strcat(o, s) PoemConcatUnsafe(o, s)
#define strncat(o, s, ds) PoemConcat(o, s, ds)
#define strdup(s) SbStringDuplicate(s)
#define strchr(s, c) PoemFindCharacter(s, c)
#define strrchr(s, c) PoemFindLastCharacter(s, c)
#define strstr(s, c) SbStringFindString(s, c)
#define strncmp(s1, s2, c) SbStringCompare(s1, s2, c)
#define strcmp(s1, s2) SbStringCompareAll(s1, s2)

// number conversion functions
#define strtol(s, o, b) SbStringParseSignedInteger(s, o, b)
#define atoi(v) SbStringAToI(v)
#define atol(v) SbStringAToL(v)
#define strtol(s, o, b) SbStringParseSignedInteger(s, o, b)
#define strtoul(s, o, b) SbStringParseUnsignedInteger(s, o, b)
#define strtoull(s, o, b) SbStringParseUInt64(s, o, b)
#define strtod(s, o) SbStringParseDouble(s, o)

#endif  // POEM_FULL_EMULATION

#endif  // STARBOARD_CLIENT_PORTING_POEM_STRING_POEM_H_

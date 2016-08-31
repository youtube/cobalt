// Copyright 2015 Google Inc. All Rights Reserved.
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

// Functions for interacting with c-style strings.

#ifndef STARBOARD_STRING_H_
#define STARBOARD_STRING_H_

#include <stdarg.h>

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns the length, in characters, of |str|, a zero-terminated ASCII string.
SB_EXPORT size_t SbStringGetLength(const char* str);

// Same as SbStringGetLength but for wide characters. This assumes that there
// are no multi-element characters.
SB_EXPORT size_t SbStringGetLengthWide(const wchar_t* str);

// Copies as much |source| as possible into |out_destination| and
// null-terminates it, given that it has |destination_size| characters of
// storage available.  Returns the length of |source|.  Meant to be a drop-in
// replacement for strlcpy
SB_EXPORT int SbStringCopy(char* out_destination,
                           const char* source,
                           int destination_size);

// Inline wrapper for an unsafe SbStringCopy that assumes |out_destination| is
// big enough. Meant to be a drop-in replacement for strcpy.
static SB_C_INLINE char* SbStringCopyUnsafe(char* out_destination,
                                            const char* source) {
  SbStringCopy(out_destination, source, INT_MAX);
  return out_destination;
}

// Same as SbStringCopy but for wide characters.
SB_EXPORT int SbStringCopyWide(wchar_t* out_destination,
                               const wchar_t* source,
                               int destination_size);

// Concatenates |source| onto the end of |out_destination|, presuming it has
// |destination_size| total characters of storage available. Returns
// length of |source|. Meant to be a drop-in replacement for strlcpy
// Note: This function's signature is NOT compatible with strncat
SB_EXPORT int SbStringConcat(char* out_destination,
                             const char* source,
                             int destination_size);

// Inline wrapper for an unsafe SbStringConcat that assumes |out_destination| is
// big enough.
// Note: This function's signature is NOT compatible with strcat.
static SB_C_INLINE int SbStringConcatUnsafe(char* out_destination,
                                            const char* source) {
  return SbStringConcat(out_destination, source, INT_MAX);
}

// Same as SbStringConcat but for wide characters.
SB_EXPORT int SbStringConcatWide(wchar_t* out_destination,
                                 const wchar_t* source,
                                 int destination_size);

// Copies the string |source| into a buffer allocated by this function that can
// be freed with SbMemoryFree. Meant to be a drop-in replacement for strdup.
SB_EXPORT char* SbStringDuplicate(const char* source);

// Finds the first occurrence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
// NOTE: The function signature of this function does NOT match the function
// strchr.
SB_EXPORT const char* SbStringFindCharacter(const char* str, char character);

// Finds the last occurrence of |character| in |str|, returning a pointer to the
// found character in the given string, or NULL if not found.
// NOTE: The function signature of this function does NOT match the function
// strrchr.
SB_EXPORT const char* SbStringFindLastCharacter(const char* str,
                                                char character);

// Finds the first occurrence of |str2| in |str1|, returning a pointer to the
// beginning of the found occurrencein |str1|, or NULL if not found.  Meant to
// be a drop-in replacement for strstr.
SB_EXPORT const char* SbStringFindString(const char* str1, const char* str2);

// Compares |string1| and |string2|, ignoring differences in case. Returns < 0
// if |string1| is ASCII-betically lower than |string2|, 0 if they are equal,
// and > 0 if |string1| is ASCII-betically higher than |string2|. Meant to be a
// drop-in replacement for strcasecmp.
SB_EXPORT int SbStringCompareNoCase(const char* string1, const char* string2);

// Compares the first |count| characters of |string1| and |string2|, ignoring
// differences in case. Returns < 0 if |string1| is ASCII-betically lower than
// |string2|, 0 if they are equal, and > 0 if |string1| is ASCII-betically
// higher than |string2|. Meant to be a drop-in replacement for strncasecmp.
SB_EXPORT int SbStringCompareNoCaseN(const char* string1,
                                     const char* string2,
                                     size_t count);

// Formats the given |format| and |arguments|, placing as much of the result
// will fit in |out_buffer|, whose size is specified by |buffer_size|. Returns
// the number of characters the format would produce if |buffer_size| was
// infinite. Meant to be a drop-in replacement for vsnprintf.
SB_EXPORT int SbStringFormat(char* out_buffer,
                             size_t buffer_size,
                             const char* format,
                             va_list arguments) SB_PRINTF_FORMAT(3, 0);

// Inline wrapper of SbStringFormat to convert from ellipsis to va_args.
// Meant to be a drop-in replacement for snprintf.
static SB_C_INLINE int SbStringFormatF(char* out_buffer,
                                       size_t buffer_size,
                                       const char* format,
                                       ...) SB_PRINTF_FORMAT(3, 4);
static SB_C_INLINE int SbStringFormatF(char* out_buffer,
                                       size_t buffer_size,
                                       const char* format,
                                       ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = SbStringFormat(out_buffer, buffer_size, format, arguments);
  va_end(arguments);
  return result;
}

// Inline wrapper of SbStringFormat to be a drop-in replacement for the unsafe
// but commonly used sprintf.
static SB_C_INLINE int SbStringFormatUnsafeF(char* out_buffer,
                                             const char* format,
                                             ...) SB_PRINTF_FORMAT(2, 3);
static SB_C_INLINE int SbStringFormatUnsafeF(char* out_buffer,
                                             const char* format,
                                             ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = SbStringFormat(out_buffer, UINT_MAX, format, arguments);
  va_end(arguments);
  return result;
}

// Same as SbStringFormat, but for wide characters. Meant to be a drop-in
// replacement for vswprintf.
SB_EXPORT int SbStringFormatWide(wchar_t* out_buffer,
                                 size_t buffer_size,
                                 const wchar_t* format,
                                 va_list arguments);

// Inline wrapper of SbStringFormatWide to convert from ellipsis to va_args.
static SB_C_INLINE int SbStringFormatWideF(wchar_t* out_buffer,
                                           size_t buffer_size,
                                           const wchar_t* format,
                                           ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = SbStringFormatWide(out_buffer, buffer_size, format, arguments);
  va_end(arguments);
  return result;
}

// Compares the first |count| characters of |string1| and |string2|, which are
// 16-bit character strings. Returns < 0 if |string1| is ASCII-betically lower
// than |string2|, 0 if they are equal, and > 0 if |string1| is ASCII-betically
// higher than |string2|. Meant to be a drop-in replacement of wcsncmp.
SB_EXPORT int SbStringCompareWide(const wchar_t* string1,
                                  const wchar_t* string2,
                                  size_t count);

// Compares the first |count| characters of |string1| and |string2|, which are
// 8-bit character strings. Returns < 0 if |string1| is ASCII-betically lower
// than |string2|, 0 if they are equal, and > 0 if |string1| is ASCII-betically
// higher than |string2|. Meant to be a drop-in replacement of strncmp.
SB_EXPORT int SbStringCompare(const char* string1,
                              const char* string2,
                              size_t count);

// Compares all the characters of |string1| and |string2|, which are 8-bit
// character strings, up to their natural termination. Returns < 0 if |string1|
// is ASCII-betically lower than |string2|, 0 if they are equal, and > 0 if
// |string1| is ASCII-betically higher than |string2|. Meant to be a drop-in
// replacement of strcmp.
SB_EXPORT int SbStringCompareAll(const char* string1, const char* string2);

// Scans |buffer| for |pattern|, placing the extracted values in |arguments|.
// Returns the number of successfully matched items, which may be zero. Meant to
// be a drop-in replacement for vsscanf.
SB_EXPORT int SbStringScan(const char* buffer,
                           const char* pattern,
                           va_list arguments);

// Inline wrapper of SbStringScan to convert from ellipsis to va_args. Meant to
// be a drop-in replacement for sscanf.
static SB_C_INLINE int SbStringScanF(const char* buffer,
                                     const char* pattern,
                                     ...) {
  va_list arguments;
  va_start(arguments, pattern);
  int result = SbStringScan(buffer, pattern, arguments);
  va_end(arguments);
  return result;
}

// Parses a string at the beginning of |start| into a signed integer in the
// given |base|.  It will place the pointer to the end of the consumed portion
// of the string in |out_end|, if it is provided. Meant to be a drop-in
// replacement for strtol.
// NOLINTNEXTLINE(runtime/int)
SB_EXPORT long SbStringParseSignedInteger(const char* start,
                                          char** out_end,
                                          int base);

// Shorthand replacement for atoi. Parses |value| into a base-10 int.
static SB_C_INLINE int SbStringAToI(const char* value) {
  // NOLINTNEXTLINE(readability/casting)
  return (int)SbStringParseSignedInteger(value, NULL, 10);
}

// Shorthand replacement for atol. Parses |value| into a base-10 int.
// NOLINTNEXTLINE(runtime/int)
static SB_C_INLINE long SbStringAToL(const char* value) {
  // NOLINTNEXTLINE(readability/casting)
  return SbStringParseSignedInteger(value, NULL, 10);
}

// Parses a string at the beginning of |start| into a unsigned integer in the
// given |base|.  It will place the pointer to the end of the consumed portion
// of the string in |out_end|, if it is provided. Meant to be a drop-in
// replacement for strtoul.
// NOLINTNEXTLINE(runtime/int)
SB_EXPORT unsigned long SbStringParseUnsignedInteger(const char* start,
                                                     char** out_end,
                                                     int base);

// Parses a string at the beginning of |start| into a unsigned 64-bit integer in
// the given |base|.  It will place the pointer to the end of the consumed
// portion of the string in |out_end|, if it is provided. Meant to be a drop-in
// replacement for strtoull, but explicity declared to return uint64_t.
SB_EXPORT uint64_t SbStringParseUInt64(const char* start,
                                       char** out_end,
                                       int base);

// Parses a string at the beginning of |start| into a double.
// It will place the pointer to the end of the consumed
// portion of the string in |out_end|, if it is provided. Meant to be a drop-in
// replacement for strtod, but explicity declared to return double.
SB_EXPORT double SbStringParseDouble(const char* start, char** out_end);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_STRING_H_

// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard String module
//
// Defines functions for interacting with c-style strings.

#ifndef STARBOARD_STRING_H_
#define STARBOARD_STRING_H_

#include <stdarg.h>
#include <stdio.h>
#if SB_API_VERSION <= 15
#include <wchar.h>
#endif

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SB_API_VERSION < 16
// Copies |source| into a buffer that is allocated by this function and that
// can be freed with SbMemoryDeallocate. This function is meant to be a drop-in
// replacement for |strdup|.
//
// |source|: The string to be copied.
SB_EXPORT char* SbStringDuplicate(const char* source);
#endif  // SB_API_VERSION < 16

// Compares two strings, ignoring differences in case. The return value is:
// - |< 0| if |string1| is ASCII-betically lower than |string2|.
// - |0| if the two strings are equal.
// - |> 0| if |string1| is ASCII-betically higher than |string2|.
//
// This function is meant to be a drop-in replacement for |strcasecmp|.
//
// |string1|: The first string to compare.
// |string2|: The second string to compare.
SB_EXPORT int SbStringCompareNoCase(const char* string1, const char* string2);

// Compares the first |count| characters of two strings, ignoring differences
// in case. The return value is:
// - |< 0| if |string1| is ASCII-betically lower than |string2|.
// - |0| if the two strings are equal.
// - |> 0| if |string1| is ASCII-betically higher than |string2|.
//
// This function is meant to be a drop-in replacement for |strncasecmp|.
//
// |string1|: The first string to compare.
// |string2|: The second string to compare.
// |count|: The number of characters to compare.
SB_EXPORT int SbStringCompareNoCaseN(const char* string1,
                                     const char* string2,
                                     size_t count);

#if SB_API_VERSION <= 15
// Produces a string formatted with |format| and |arguments|, placing as much
// of the result that will fit into |out_buffer|. The return value specifies
// the number of characters that the format would produce if |buffer_size| were
// infinite.
//
// This function is meant to be a drop-in replacement for |vsnprintf|.
//
// |out_buffer|: The location where the formatted string is stored.
// |buffer_size|: The size of |out_buffer|.
// |format|: A string that specifies how the data should be formatted.
// |arguments|: Variable arguments used in the string.
SB_EXPORT int SbStringFormat(char* out_buffer,
                             size_t buffer_size,
                             const char* format,
                             va_list arguments) SB_PRINTF_FORMAT(3, 0);
// An inline wrapper of SbStringFormat that converts from ellipsis to va_args.
// This function is meant to be a drop-in replacement for |snprintf|.
//
// |out_buffer|: The location where the formatted string is stored.
// |buffer_size|: The size of |out_buffer|.
// |format|: A string that specifies how the data should be formatted.
// |...|: Arguments used in the string.
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
  int result = vsnprintf(out_buffer, buffer_size, format, arguments);
  va_end(arguments);
  return result;
}

// An inline wrapper of SbStringFormat that is meant to be a drop-in
// replacement for the unsafe but commonly used |sprintf|.
//
// |out_buffer|: The location where the formatted string is stored.
// |format|: A string that specifies how the data should be formatted.
// |...|: Arguments used in the string.
static SB_C_INLINE int SbStringFormatUnsafeF(char* out_buffer,
                                             const char* format,
                                             ...) SB_PRINTF_FORMAT(2, 3);
static SB_C_INLINE int SbStringFormatUnsafeF(char* out_buffer,
                                             const char* format,
                                             ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = vsnprintf(out_buffer, SSIZE_MAX, format, arguments);
  va_end(arguments);
  return result;
}

// This function is identical to SbStringFormat, but is for wide characters.
// It is meant to be a drop-in replacement for |vswprintf|.
//
// |out_buffer|: The location where the formatted string is stored.
// |buffer_size|: The size of |out_buffer|.
// |format|: A string that specifies how the data should be formatted.
// |arguments|: Variable arguments used in the string.
SB_EXPORT int SbStringFormatWide(wchar_t* out_buffer,
                                 size_t buffer_size,
                                 const wchar_t* format,
                                 va_list arguments);

// An inline wrapper of SbStringFormatWide that converts from ellipsis to
// |va_args|.
//
// |out_buffer|: The location where the formatted string is stored.
// |buffer_size|: The size of |out_buffer|.
// |format|: A string that specifies how the data should be formatted.
// |...|: Arguments used in the string.

static SB_C_INLINE int SbStringFormatWideF(wchar_t* out_buffer,
                                           size_t buffer_size,
                                           const wchar_t* format,
                                           ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = vswprintf(out_buffer, buffer_size, format, arguments);
  va_end(arguments);
  return result;
}
#endif

// Scans |buffer| for |pattern|, placing the extracted values in |arguments|.
// The return value specifies the number of successfully matched items, which
// may be |0|.
//
// This function is meant to be a drop-in replacement for |vsscanf|.
//
// |buffer|: The string to scan for the pattern.
// |pattern|: The string to search for in |buffer|.
// |arguments|: Values matching |pattern| that were extracted from |buffer|.
SB_EXPORT int SbStringScan(const char* buffer,
                           const char* pattern,
                           va_list arguments);

// An inline wrapper of SbStringScan that converts from ellipsis to |va_args|.
// This function is meant to be a drop-in replacement for |sscanf|.
// |buffer|: The string to scan for the pattern.
// |pattern|: The string to search for in |buffer|.
// |...|: Values matching |pattern| that were extracted from |buffer|.
static SB_C_INLINE int SbStringScanF(const char* buffer,
                                     const char* pattern,
                                     ...) {
  va_list arguments;
  va_start(arguments, pattern);
  int result = SbStringScan(buffer, pattern, arguments);
  va_end(arguments);
  return result;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_STRING_H_

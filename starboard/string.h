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
size_t SbStringGetLength(const char* str);

// Same as SbStringGetLength but for wide characters. This assumes that there
// are no multi-element characters.
size_t SbStringGetLengthWide(const wchar_t* str);

// Copies as much |source| as possible into |out_destination| and
// null-terminates it, given that it has |destination_size| characters of
// storage available. Returns the length of |source|.
int SbStringCopy(char* out_destination,
                 const char* source,
                 int destination_size);

// Same as SbStringCopy but for wide characters.
int SbStringCopyWide(wchar_t* out_destination,
                     const wchar_t* source,
                     int destination_size);

// Concatenates |source| onto the end of |out_destination|, presuming it has
// |destination_size| total characters of storage available. Returns the length
// of |source|.
int SbStringConcat(char* out_destination,
                   const char* source,
                   int destination_size);

// Same as SbStringConcat but for wide characters.
int SbStringConcatWide(wchar_t* out_destination,
                       const wchar_t* source,
                       int destination_size);

// Copies the string |source| into a buffer allocated by this function that can
// be freed with SbMemoryFree. Meant to be a drop-in replacement for strdup.
char* SbStringDuplicate(const char* source);

// Finds the first occurence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
const char* SbStringFindCharacter(const char* str, char character);

// Finds the last occurence of |character| in |str|, returning a pointer to
// the found character in the given string, or NULL if not found.
const char* SbStringFindLastCharacter(const char* str, char character);

// Compares |string1| and |string2|, ignoring differences in case. Returns < 0
// if |string1| is ASCII-betically lower than |string2|, 0 if they are equal,
// and > 0 if |string1| is ASCII-betically higher than |string2|. Meant to be a
// drop-in replacement for strcasecmp.
int SbStringCompareNoCase(const char* string1, const char* string2);

// Compares the first |count| characters of |string1| and |string2|, ignoring
// differences in case. Returns < 0 if |string1| is ASCII-betically lower than
// |string2|, 0 if they are equal, and > 0 if |string1| is ASCII-betically
// higher than |string2|. Meant to be a drop-in replacement for strncasecmp.
int SbStringCompareNoCaseN(const char* string1,
                           const char* string2,
                           size_t count);

// Formats the given |format| and |arguments|, placing as much of the result
// will fit in |out_buffer|, whose size is specified by |buffer_size|. Returns
// the number of characters the format would produce if |buffer_size| was
// infinite. Meant to be a drop-in replacement for vsnprintf.
int SbStringFormat(char* out_buffer,
                   size_t buffer_size,
                   const char* format,
                   va_list arguments) SB_PRINTF_FORMAT(3, 0);

// Inline wrapper of SbStringFormat to convert from ellipsis to va_args.
inline int SbStringFormatF(char* out_buffer,
                           size_t buffer_size,
                           const char* format,
                           ...) SB_PRINTF_FORMAT(3, 4);
inline int SbStringFormatF(char* out_buffer,
                           size_t buffer_size,
                           const char* format,
                           ...) {
  va_list args;
  va_start(args, format);
  int result = SbStringFormat(out_buffer, buffer_size, format, args);
  va_end(args);
  return result;
}

// Same as SbStringFormat, but for wide characters. Meant to be a drop-in
// replacement for vswprintf.
int SbStringFormatWide(wchar_t* out_buffer,
                       size_t buffer_size,
                       const wchar_t* format,
                       va_list arguments);

// Inline wrapper of SbStringFormatWide to convert from ellipsis to va_args.
inline int SbStringFormatWideF(wchar_t* out_buffer,
                               size_t buffer_size,
                               const wchar_t* format,
                               ...) {
  va_list args;
  va_start(args, format);
  int result = SbStringFormatWide(out_buffer, buffer_size, format, args);
  va_end(args);
  return result;
}

// Compares the first |count| characters of |string1| and |string2|, which are
// 16-bit character strings. Returns < 0 if |string1| is ASCII-betically lower
// than |string2|, 0 if they are equal, and > 0 if |string1| is ASCII-betically
// higher than |string2|. Meant to be a drop-in replacement of wcsncmp.
int SbStringCompareWide(const wchar_t* string1,
                        const wchar_t* string2,
                        size_t count);

// Compares the first |count| characters of |string1| and |string2|, which are
// 8-bit character strings. Returns < 0 if |string1| is ASCII-betically lower
// than |string2|, 0 if they are equal, and > 0 if |string1| is ASCII-betically
// higher than |string2|. Meant to be a drop-in replacement of strncmp.
int SbStringCompare(const char* string1, const char* string2, size_t count);

// Compares all the characters of |string1| and |string2|, which are 8-bit
// character strings, up to their natural termination. Returns < 0 if |string1|
// is ASCII-betically lower than |string2|, 0 if they are equal, and > 0 if
// |string1| is ASCII-betically higher than |string2|. Meant to be a drop-in
// replacement of strcmp.
int SbStringCompareAll(const char* string1, const char* string2);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_STRING_H_

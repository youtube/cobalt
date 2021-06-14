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

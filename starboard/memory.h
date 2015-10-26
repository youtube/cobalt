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

// Memory allocation, alignment, copying, and comparing.

#ifndef STARBOARD_MEMORY_H_
#define STARBOARD_MEMORY_H_

#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Checks whether |memory| is aligned to |alignment| bytes.
SB_C_FORCE_INLINE bool SbMemoryIsAligned(const void *memory,
                                         size_t alignment) {
  return ((uintptr_t)memory) % alignment == 0;
}

// Allocates a chunk of memory of at least |size| bytes, returning it. If unable
// to allocate the memory, it returns NULL. If |size| is 0, it may return NULL
// or it may return a unique pointer value that can be passed to
// SbMemoryFree. Meant to be a drop-in replacement for malloc.
SB_EXPORT void *SbMemoryAllocate(size_t size);

// Attempts to resize |memory| to be at least |size| bytes, without touching the
// contents. If it cannot perform the fast resize, it will allocate a new chunk
// of memory, copy the contents over, and free the previous chunk, returning a
// pointer to the new chunk. If it cannot perform the slow resize, it will
// return NULL, leaving the given memory chunk unchanged. |memory| may be NULL,
// in which case it behaves exactly like SbMemoryAllocate.  If |size| is 0, it
// may return NULL or it may return a unique pointer value that can be passed to
// SbMemoryFree. Meant to be a drop-in replacement for realloc.
SB_EXPORT void *SbMemoryReallocate(void *memory, size_t size);

// Frees a previously allocated chunk of memory. If |memory| is NULL, then it
// does nothing.  Meant to be a drop-in replacement for free.
SB_EXPORT void SbMemoryFree(void *memory);

// Allocates a chunk of memory of at least |size| bytes, aligned to |alignment|,
// returning it. |alignment| must be a power of two, otherwise behavior is
// undefined. If unable to allocate the memory, it returns NULL. If |size| is 0,
// it may return NULL or it may return a unique aligned pointer value that can
// be passed to SbMemoryFreeAligned. Meant to be a drop-in replacement for
// memalign.
SB_EXPORT void *SbMemoryAllocateAligned(size_t alignment, size_t size);

// Frees a previously allocated chunk of aligned memory. If |memory| is NULL,
// then it does nothing.  Meant to be a drop-in replacement for _aligned_free.
SB_EXPORT void SbMemoryFreeAligned(void *memory);

// Copies |count| sequential bytes from |source| to |destination|, without
// support for the |source| and |destination| regions overlapping.  Behavior is
// undefined if |destination| or |source| are NULL. Does nothing if |count| is
// 0. Returns |destination|, for some reason. Meant to be a drop-in replacement
// for memcpy.
SB_EXPORT void *SbMemoryCopy(void *destination,
                             const void *source,
                             size_t count);

// Copies |count| sequential bytes from |source| to |destination|, with support
// for the |source| and |destination| regions overlapping.  Behavior is
// undefined if |destination| or |source| are NULL. Does nothing if |count| is
// 0. Returns |destination|, for some reason. Meant to be a drop-in replacement
// for memmove.
SB_EXPORT void *SbMemoryMove(void *destination,
                             const void *source,
                             size_t count);

// Fills |count| sequential bytes starting at |destination|, with the unsigned
// char coercion of |byte_value|. Behavior is undefined if |destination| is
// NULL. Returns |destination|, for some reason. Does nothing if |count| is 0.
// Meant to be a drop-in replacement for memset.
SB_EXPORT void *SbMemorySet(void *destination, int byte_value, size_t count);

// Compares the contents of the first |count| bytes of |buffer1| and |buffer2|.
// returns -1 if |buffer1| is "less-than" |buffer2|, 0 if they are equal, or 1
// if |buffer1| is "greater-than" |buffer2|.  Meant to be a drop-in replacement
// for memcmp.
SB_EXPORT int SbMemoryCompare(const void *buffer1,
                              const void *buffer2,
                              size_t count);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_MEMORY_H_

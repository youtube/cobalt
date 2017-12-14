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

// Module Overview: Starboard Memory module
//
// Defines functions for memory allocation, alignment, copying, and comparing.
//
// # Porters
//
// All of the "Unchecked" and "Free" functions must be implemented, but they
// should not be called directly. The Starboard platform wraps them with extra
// accounting under certain circumstances.
//
// # Porters and Application Developers
//
// Nobody should call the "Checked", "Unchecked" or "Free" functions directly
// because that evades Starboard's memory tracking. In both port
// implementations and Starboard client application code, you should always
// call SbMemoryAllocate and SbMemoryDeallocate rather than
// SbMemoryAllocateUnchecked and SbMemoryFree.
//
// - The "checked" functions are SbMemoryAllocateChecked(),
//   SbMemoryReallocateChecked(), and SbMemoryAllocateAlignedChecked().
// - The "unchecked" functions are SbMemoryAllocateUnchecked(),
//   SbMemoryReallocateUnchecked(), and SbMemoryAllocateAlignedUnchecked().
// - The "free" functions are SbMemoryFree() and SbMemoryFreeAligned().

#ifndef STARBOARD_MEMORY_H_
#define STARBOARD_MEMORY_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/system.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SB_MEMORY_MAP_FAILED ((void*)-1)  // NOLINT(readability/casting)

// The bitwise OR of these flags should be passed to SbMemoryMap to indicate
// how the mapped memory can be used.
typedef enum SbMemoryMapFlags {
  kSbMemoryMapProtectRead = 1 << 0,   // Mapped memory can be read.
  kSbMemoryMapProtectWrite = 1 << 1,  // Mapped memory can be written to.
#if SB_CAN(MAP_EXECUTABLE_MEMORY)
  kSbMemoryMapProtectExec = 1 << 2,  // Mapped memory can be executed.
#endif
  kSbMemoryMapProtectReadWrite =
      kSbMemoryMapProtectRead | kSbMemoryMapProtectWrite,
} SbMemoryMapFlags;

// Checks whether |memory| is aligned to |alignment| bytes.
static SB_C_FORCE_INLINE bool SbMemoryIsAligned(const void* memory,
                                                size_t alignment) {
  return ((uintptr_t)memory) % alignment == 0;
}

// Rounds |size| up to SB_MEMORY_PAGE_SIZE.
static SB_C_FORCE_INLINE size_t SbMemoryAlignToPageSize(size_t size) {
  return (size + SB_MEMORY_PAGE_SIZE - 1) & ~(SB_MEMORY_PAGE_SIZE - 1);
}

static SB_C_FORCE_INLINE void SbAbortIfAllocationFailed(size_t requested_bytes,
                                                        void* address) {
  if (SB_UNLIKELY(requested_bytes > 0 && address == NULL)) {
    // Will abort the program if no debugger is attached.
    SbSystemBreakIntoDebugger();
  }
}

// Allocates and returns a chunk of memory of at least |size| bytes. This
// function should be called from the client codebase. It is intended to be a
// drop-in replacement for |malloc|.
//
// Note that this function returns |NULL| if it is unable to allocate the
// memory.
//
// |size|: The amount of memory to be allocated. If |size| is 0, the function
//   may return |NULL| or it may return a unique pointer value that can be
//   passed to SbMemoryDeallocate.
SB_EXPORT void* SbMemoryAllocate(size_t size);

// Same as SbMemoryAllocate() but will not report memory to the tracker. Avoid
// using this unless absolutely necessary.
SB_EXPORT void* SbMemoryAllocateNoReport(size_t size);

// Attempts to resize |memory| to be at least |size| bytes, without touching
// the contents of memory.
// - If the function cannot perform the fast resize, it allocates a new chunk
//   of memory, copies the contents over, and frees the previous chunk,
//   returning a pointer to the new chunk.
// - If the function cannot perform the slow resize, it returns |NULL|,
//   leaving the given memory chunk unchanged.
//
// This function should be called from the client codebase. It is meant to be a
// drop-in replacement for |realloc|.
//
// |memory|: The chunk of memory to be resized. |memory| may be NULL, in which
//   case it behaves exactly like SbMemoryAllocateUnchecked.
// |size|: The size to which |memory| will be resized. If |size| is |0|,
//   the function may return |NULL| or it may return a unique pointer value
//   that can be passed to SbMemoryDeallocate.
SB_EXPORT void* SbMemoryReallocate(void* memory, size_t size);

// Allocates and returns a chunk of memory of at least |size| bytes, aligned to
// |alignment|. This function should be called from the client codebase. It is
// meant to be a drop-in replacement for |memalign|.
//
// The function returns |NULL| if it cannot allocate the memory. In addition,
// the function's behavior is undefined if |alignment| is not a power of two.
//
// |alignment|: The way that data is arranged and accessed in memory. The value
//   must be a power of two.
// |size|: The size of the memory to be allocated. If |size| is |0|, the
//   function may return |NULL| or it may return a unique aligned pointer value
//   that can be passed to SbMemoryDeallocateAligned.
SB_EXPORT void* SbMemoryAllocateAligned(size_t alignment, size_t size);

// Frees a previously allocated chunk of memory. If |memory| is NULL, then the
// operation is a no-op. This function should be called from the client
// codebase. It is meant to be a drop-in replacement for |free|.
//
// |memory|: The chunk of memory to be freed.
SB_EXPORT void SbMemoryDeallocate(void* memory);

// Same as SbMemoryDeallocate() but will not report memory deallocation to the
// tracker. This function must be matched with SbMemoryAllocateNoReport().
SB_EXPORT void SbMemoryDeallocateNoReport(void* memory);

// Frees a previously allocated chunk of aligned memory. This function should
// be called from the client codebase. It is meant to be a drop-in replacement
// for |_aligned_free|.

// |memory|: The chunk of memory to be freed. If |memory| is NULL, then the
//   function is a no-op.
SB_EXPORT void SbMemoryDeallocateAligned(void* memory);

/////////////////////////////////////////////////////////////////
// The following functions must be provided by Starboard ports.
/////////////////////////////////////////////////////////////////

// This is the implementation of SbMemoryAllocate that must be
// provided by Starboard ports.
//
// DO NOT CALL. Call SbMemoryAllocate(...) instead.
SB_DEPRECATED_EXTERNAL(
    SB_EXPORT void* SbMemoryAllocateUnchecked(size_t size));

// This is the implementation of SbMemoryReallocate that must be
// provided by Starboard ports.
//
// DO NOT CALL. Call SbMemoryReallocate(...) instead.
SB_DEPRECATED_EXTERNAL(
    SB_EXPORT void* SbMemoryReallocateUnchecked(void* memory, size_t size));

// This is the implementation of SbMemoryAllocateAligned that must be
// provided by Starboard ports.
//
// DO NOT CALL. Call SbMemoryAllocateAligned(...) instead.
SB_DEPRECATED_EXTERNAL(
    SB_EXPORT void* SbMemoryAllocateAlignedUnchecked(size_t alignment,
                                                     size_t size));

// This is the implementation of SbMemoryDeallocate that must be provided by
// Starboard ports.
//
// DO NOT CALL. Call SbMemoryDeallocate(...) instead.
SB_DEPRECATED_EXTERNAL(
    SB_EXPORT void SbMemoryFree(void* memory));

// This is the implementation of SbMemoryFreeAligned that must be provided by
// Starboard ports.
//
// DO NOT CALL. Call SbMemoryDeallocateAligned(...) instead.
SB_DEPRECATED_EXTERNAL(
    SB_EXPORT void SbMemoryFreeAligned(void* memory));

#if SB_HAS(MMAP)
// Allocates |size_bytes| worth of physical memory pages and maps them into an
// available virtual region. This function returns |SB_MEMORY_MAP_FAILED| on
// failure. |NULL| is a valid return value.
//
// |size_bytes|: The amount of physical memory pages to be allocated.
// |flags|: The bitwise OR of the protection flags for the mapped memory
//   as specified in |SbMemoryMapFlags|.
// |name|: A value that appears in the debugger on some platforms. The value
//   can be up to 32 bytes.
SB_EXPORT void* SbMemoryMap(int64_t size_bytes, int flags, const char* name);

// Unmap |size_bytes| of physical pages starting from |virtual_address|,
// returning |true| on success. After this function completes,
// [virtual_address, virtual_address + size_bytes) will not be read/writable.
// This function can unmap multiple contiguous regions that were mapped with
// separate calls to SbMemoryMap(). For example, if one call to
// |SbMemoryMap(0x1000)| returns |(void*)0xA000|, and another call to
// |SbMemoryMap(0x1000)| returns |(void*)0xB000|,
// |SbMemoryUnmap(0xA000, 0x2000)| should free both regions.
SB_EXPORT bool SbMemoryUnmap(void* virtual_address, int64_t size_bytes);

#if SB_CAN(MAP_EXECUTABLE_MEMORY)
// Flushes any data in the given virtual address range that is cached locally in
// the current processor core to physical memory, ensuring that data and
// instruction caches are cleared. This is required to be called on executable
// memory that has been written to and might be executed in the future.
SB_EXPORT void SbMemoryFlush(void* virtual_address, int64_t size_bytes);
#endif
#endif  // SB_HAS(MMAP)

// Gets the stack bounds for the current thread.
//
// |out_high|: The highest addressable byte + 1 for the current thread.
// |out_low|: The lowest addressable byte for the current thread.
SB_EXPORT void SbMemoryGetStackBounds(void** out_high, void** out_low);

// Copies |count| sequential bytes from |source| to |destination|, without
// support for the |source| and |destination| regions overlapping. This
// function is meant to be a drop-in replacement for |memcpy|.
//
// The function's behavior is undefined if |destination| or |source| are NULL,
// and the function is a no-op if |count| is 0. The return value is
// |destination|.
//
// |destination|: The destination of the copied memory.
// |source|: The source of the copied memory.
// |count|: The number of sequential bytes to be copied.
SB_EXPORT void* SbMemoryCopy(void* destination,
                             const void* source,
                             size_t count);

// Copies |count| sequential bytes from |source| to |destination|, with support
// for the |source| and |destination| regions overlapping. This function is
// meant to be a drop-in replacement for |memmove|.
//
// The function's behavior is undefined if |destination| or |source| are NULL,
// and the function is a no-op if |count| is 0. The return value is
// |destination|.
//
// |destination|: The destination of the copied memory.
// |source|: The source of the copied memory.
// |count|: The number of sequential bytes to be copied.
SB_EXPORT void* SbMemoryMove(void* destination,
                             const void* source,
                             size_t count);

// Fills |count| sequential bytes starting at |destination|, with the unsigned
// char coercion of |byte_value|. This function is meant to be a drop-in
// replacement for |memset|.
//
// The function's behavior is undefined if |destination| is NULL, and the
// function is a no-op if |count| is 0. The return value is |destination|.
//
// |destination|: The destination of the copied memory.
// |count|: The number of sequential bytes to be set.
SB_EXPORT void* SbMemorySet(void* destination, int byte_value, size_t count);

// Compares the contents of the first |count| bytes of |buffer1| and |buffer2|.
// This function returns:
// - |-1| if |buffer1| is "less-than" |buffer2|
// - |0| if |buffer1| and |buffer2| are equal
// - |1| if |buffer1| is "greater-than" |buffer2|.
//
// This function is meant to be a drop-in replacement for |memcmp|.
//
// |buffer1|: The first buffer to be compared.
// |buffer2|: The second buffer to be compared.
// |count|: The number of bytes to be compared.
SB_EXPORT int SbMemoryCompare(const void* buffer1,
                              const void* buffer2,
                              size_t count);

// Finds the lower 8-bits of |value| in the first |count| bytes of |buffer|
// and returns either a pointer to the first found occurrence or |NULL| if
// the value is not found. This function is meant to be a drop-in replacement
// for |memchr|.
SB_EXPORT const void* SbMemoryFindByte(const void* buffer,
                                       int value,
                                       size_t count);

// A wrapper that implements a drop-in replacement for |calloc|, which is used
// in some packages.
static SB_C_INLINE void* SbMemoryCalloc(size_t count, size_t size) {
  size_t total = count * size;
  void* result = SbMemoryAllocate(total);
  if (result) {
    SbMemorySet(result, 0, total);
  }
  return result;
}

// Returns true if the first |count| bytes of |buffer| are set to zero.
static SB_C_INLINE bool SbMemoryIsZero(const void* buffer, size_t count) {
  if (count == 0) {
    return true;
  }
  const char* char_buffer = (const char*)(buffer);
  return char_buffer[0] == 0 &&
         SbMemoryCompare(char_buffer, char_buffer + 1, count - 1) == 0;
}

/////////////////////////////////////////////////////////////////
// Deprecated. Do not use.
/////////////////////////////////////////////////////////////////

// Same as SbMemoryAllocateUnchecked, but will abort() in the case of an
// allocation failure.
//
// DO NOT CALL. Call SbMemoryAllocate(...) instead.
SB_DEPRECATED_EXTERNAL(
    SB_EXPORT void* SbMemoryAllocateChecked(size_t size));

// Same as SbMemoryReallocateUnchecked, but will abort() in the case of an
// allocation failure.
//
// DO NOT CALL. Call SbMemoryReallocate(...) instead.
SB_DEPRECATED_EXTERNAL(
    SB_EXPORT void* SbMemoryReallocateChecked(void* memory, size_t size));

// Same as SbMemoryAllocateAlignedUnchecked, but will abort() in the case of an
// allocation failure.
//
// DO NOT CALL. Call SbMemoryAllocateAligned(...) instead.
SB_DEPRECATED_EXTERNAL(
    SB_EXPORT void* SbMemoryAllocateAlignedChecked(size_t alignment,
                                                   size_t size));

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_MEMORY_H_

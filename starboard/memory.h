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

#include <string.h>

#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
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
  // No flags set: Reserves virtual address space. SbMemoryProtect() can later
  // make it accessible.
  kSbMemoryMapProtectReserved = 0,
  kSbMemoryMapProtectRead = 1 << 0,   // Mapped memory can be read.
  kSbMemoryMapProtectWrite = 1 << 1,  // Mapped memory can be written to.
#if SB_CAN(MAP_EXECUTABLE_MEMORY)
  kSbMemoryMapProtectExec = 1 << 2,  // Mapped memory can be executed.
#endif
  kSbMemoryMapProtectReadWrite =
      kSbMemoryMapProtectRead | kSbMemoryMapProtectWrite,
} SbMemoryMapFlags;

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

// DEPRECATED: Same as SbMemoryAllocate().
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

// DEPRECATED: Same as SbMemoryDeallocate()
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
SB_DEPRECATED_EXTERNAL(SB_EXPORT void* SbMemoryAllocateUnchecked(size_t size));

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
SB_DEPRECATED_EXTERNAL(SB_EXPORT void SbMemoryFree(void* memory));

// This is the implementation of SbMemoryFreeAligned that must be provided by
// Starboard ports.
//
// DO NOT CALL. Call SbMemoryDeallocateAligned(...) instead.
SB_DEPRECATED_EXTERNAL(SB_EXPORT void SbMemoryFreeAligned(void* memory));

// Allocates |size_bytes| worth of physical memory pages and maps them into
// an available virtual region. This function returns |SB_MEMORY_MAP_FAILED|
// on failure. |NULL| is a valid return value.
//
// |size_bytes|: The amount of physical memory pages to be allocated.
// |flags|: The bitwise OR of the protection flags for the mapped memory
//   as specified in |SbMemoryMapFlags|. Allocating executable memory is not
//   allowed and will fail. If executable memory is needed, map non-executable
//   memory first and then switch access to executable using SbMemoryProtect.
//   When kSbMemoryMapProtectReserved is used, the address space will not be
//   accessible and, if possible, the platform should not count it against any
//   memory budget.
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

// Change the protection of |size_bytes| of memory regions, starting from
// |virtual_address|, to |flags|, returning |true| on success.
SB_EXPORT bool SbMemoryProtect(void* virtual_address,
                               int64_t size_bytes,
                               int flags);

#if SB_CAN(MAP_EXECUTABLE_MEMORY)
// Flushes any data in the given virtual address range that is cached locally in
// the current processor core to physical memory, ensuring that data and
// instruction caches are cleared. This is required to be called on executable
// memory that has been written to and might be executed in the future.
SB_EXPORT void SbMemoryFlush(void* virtual_address, int64_t size_bytes);
#endif

#if SB_API_VERSION < 15

// Gets the stack bounds for the current thread.
//
// |out_high|: The highest addressable byte + 1 for the current thread.
// |out_low|: The lowest addressable byte for the current thread.
SB_EXPORT void SbMemoryGetStackBounds(void** out_high, void** out_low);

#endif  // SB_API_VERSION < 15

// A wrapper that implements a drop-in replacement for |calloc|, which is used
// in some packages.
static SB_C_INLINE void* SbMemoryCalloc(size_t count, size_t size) {
  size_t total = count * size;
  void* result = SbMemoryAllocate(total);
  if (result) {
    memset(result, 0, total);
  }
  return result;
}

/////////////////////////////////////////////////////////////////
// Deprecated. Do not use.
/////////////////////////////////////////////////////////////////

// Same as SbMemoryAllocateUnchecked, but will abort() in the case of an
// allocation failure.
//
// DO NOT CALL. Call SbMemoryAllocate(...) instead.
SB_DEPRECATED_EXTERNAL(SB_EXPORT void* SbMemoryAllocateChecked(size_t size));

// Same as SbMemoryReallocateUnchecked, but will abort() in the case of an
// allocation failure.
//
// DO NOT CALL. Call SbMemoryReallocate(...) instead.
SB_DEPRECATED_EXTERNAL(SB_EXPORT void* SbMemoryReallocateChecked(void* memory,
                                                                 size_t size));

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

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

// This internal header defines a cross-platform interface for implementing
// virtual memory management. A platform must implement this if it wants to use
// dlmalloc.

#ifndef STARBOARD_SHARED_DLMALLOC_PAGE_INTERNAL_H_
#define STARBOARD_SHARED_DLMALLOC_PAGE_INTERNAL_H_

#include "starboard/memory.h"
#include "starboard/shared/internal_only.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SB_API_VERSION >= 12 || SB_HAS(MMAP)
// Allocates |size_bytes| worth of physical memory pages and maps them into an
// available virtual region. On some platforms, |name| appears in the debugger
// and can be up to 32 bytes. Returns SB_MEMORY_MAP_FAILED on failure, as NULL
// is a valid return value.
void* SbPageMap(size_t size_bytes, int flags, const char* name);

// Same as SbMap() but "untracked" means size will not be reflected in
// SbGetMappedBytes(). This should only be called by dlmalloc so that memory
// allocated by dlmalloc isn't counted twice.
void* SbPageMapUntracked(size_t size_bytes, int flags, const char* name);

// Memory maps a file to the specified |addr| starting with |file_offset| and
// mapping |size| bytes. The |addr| should be reserved before calling. If
// NULL |addr| is passed a new memory block would be allocated and the address
// returned. The file_offset must be a multiple of |kSbMemoryPageSize|.
// On error returns NULL.
void* SbPageMapFile(void* addr,
                    const char* path,
                    SbMemoryMapFlags flags,
                    int64_t file_offset,
                    int64_t size);

// Unmap |size_bytes| of physical pages starting from |virtual_address|,
// returning true on success. After this, [virtual_address, virtual_address +
// size_bytes) will not be read/writable. SbUnmap() can unmap multiple
// contiguous regions that were mapped with separate calls to
// SbPageMap(). E.g. if one call to SbPageMap(0x1000) returns (void*)0xA000 and
// another call to SbPageMap(0x1000) returns (void*)0xB000, SbPageUnmap(0xA000,
// 0x2000) should free both.
bool SbPageUnmap(void* virtual_address, size_t size_bytes);

// Same as SbUnmap(), but should be used only by dlmalloc to unmap pages
// allocated via MapUntracked().
bool SbPageUnmapUntracked(void* virtual_address, size_t size_bytes);

// Change the protection of |size_bytes| of physical pages, starting from
// |virtual_address|, to |flags|, returning |true| on success.
bool SbPageProtect(void* virtual_address, int64_t size_bytes, int flags);
#endif  // SB_API_VERSION >= 12 || SB_HAS(MMAP)

// Returns the total amount, in bytes, of physical memory available. Should
// always be a multiple of kSbMemoryPageSize.
size_t SbPageGetTotalPhysicalMemoryBytes();

// Returns the amount, in bytes, of physical memory that hasn't yet been mapped.
// Should always be a multiple of the platform's physical page size.
int64_t SbPageGetUnallocatedPhysicalMemoryBytes();

// Returns the total amount, in bytes, currently allocated via Map().  Should
// always be a multiple of kSbMemoryPageSize.
size_t SbPageGetMappedBytes();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_DLMALLOC_PAGE_INTERNAL_H_

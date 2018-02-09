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

// This internal header defines a cross-platform interface for implementing
// virtual memory management. A platform must implement this if it wants to use
// dlmalloc.

#ifndef STARBOARD_SHARED_DLMALLOC_PAGE_INTERNAL_H_
#define STARBOARD_SHARED_DLMALLOC_PAGE_INTERNAL_H_

#include "starboard/shared/internal_only.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// A virtual memory address.
typedef void* SbPageVirtualMemory;

// Internal Virtual memory API
//
// This was designed to provide common wrappers around OS functions relied upon
// by dlmalloc. However the APIs can also be used for other custom allocators,
// but, due to platform restrictions, this is not completely generic.
//
// When dlmalloc requires memory from the system, it uses two different
// approaches to get it. It either extends a growing heap, or it uses mmap() to
// request a bunch of pages from the system.
//
// Its default behavior is to place small allocations into a contiguous heap,
// and allocate large (256K+) blocks with mmap. Separating large blocks from the
// main heap has advantages for reducing fragmentation.
//
// In dlmalloc, extending the heap is called "MORECORE" and, by default on POSIX
// systems, uses sbrk().
//
// Since almost none of our platforms support sbrk(), we implement MORECORE by
// reserving a large virtual region and giving that to dlmalloc. This region
// starts off unmapped, i.e. there are no physical pages backing it, so reading
// or writing from that region is invalid.  As dlmalloc requests memory, we
// allocate physical pages from the OS and map them to the top of the heap,
// thereby growing the usable heap area. When the heap shrinks, we can unmap
// those pages and free them back to the OS.
//
// mmap(), by contrast, allocates N pages from the OS, and the OS then maps them
// into some arbitrary virtual address space. There is no guarantee that two
// consecutive mmap() calls will return a contiguous block.
//
// SbMap() is our implementation of mmap(). On platforms such as Linux that
// actually have mmap(), we call that directly. Otherwise we use
// platform-specific system allocators.
//
// Platforms that have OS support for the virtual region ("MORECORE") behavior
// will enable SB_HAS_VIRTUAL_REGIONS in configuration_public.h.
//
// Platforms that support SbMap() must enable SB_HAS_MMAP in their
// configuration_public.h file. dlmalloc is very flexible and if a platform
// can't implement virtual regions, it will use Map() for all allocations,
// merging adjacent allocations when it can.
//
// If a platform can't use Map(), it will just use MORECORE for everything.
// Currently we believe a mixture of both provides best behavior, but more
// testing would be useful.
//
// See also dlmalloc_config.h which controls some dlmalloc behavior.

#if SB_HAS(VIRTUAL_REGIONS)
// Reserves a virtual address space |size_bytes| big, without mapping any
// physical pages to that range, returning a pointer to the beginning of the
// reserved virtual address range. To get memory that is actually usable and
// backed by physical memory, a reserved virtual address needs to passed into
// AllocateAndMap().
// [Presumably size_bytes should be a multiple of a physical page size? -DG]
SbPageVirtualMemory SbPageReserveVirtualRegion(size_t size_bytes);

// Releases a virtual address space reserved with ReserveVirtualRegion().
// [What happens if that address space is wholly or partially mapped? -DG]
void SbPageReleaseVirtualRegion(SbPageVirtualMemory range_start);

// Allocate |size_bytes| of physical memory and map it to a virtual address
// range starting at |virtual_address|. |virtual_address| should be a pointer
// into the range returned by ReserveVirtualRegion().
int SbPageAllocatePhysicalAndMap(SbPageVirtualMemory virtual_address,
                                 size_t size_bytes);

// Frees |size_bytes| of physical memory that had been mapped to
// |virtual_address| and return them to the system. After this,
// [virtual_address, virtual_address + size_bytes) will not be read/writable.
int SbPageUnmapAndFreePhysical(SbPageVirtualMemory virtual_address,
                               size_t size_bytes);

// How big of a virtual region dlmalloc should allocate.
size_t SbPageGetVirtualRegionSize();
#endif

#if SB_HAS(MMAP)
// Allocates |size_bytes| worth of physical memory pages and maps them into an
// available virtual region. On some platforms, |name| appears in the debugger
// and can be up to 32 bytes. Returns SB_MEMORY_MAP_FAILED on failure, as NULL
// is a valid return value.
void* SbPageMap(size_t size_bytes, int flags, const char* name);

// Same as SbMap() but "untracked" means size will not be reflected in
// SbGetMappedBytes(). This should only be called by dlmalloc so that memory
// allocated by dlmalloc isn't counted twice.
void* SbPageMapUntracked(size_t size_bytes, int flags, const char* name);

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
#endif

// Returns the total amount, in bytes, of physical memory available. Should
// always be a multiple of SB_MEMORY_PAGE_SIZE.
size_t SbPageGetTotalPhysicalMemoryBytes();

// Returns the amount, in bytes, of physical memory that hasn't yet been mapped.
// Should always be a multiple of the platform's physical page size.
int64_t SbPageGetUnallocatedPhysicalMemoryBytes();

// Returns the total amount, in bytes, currently allocated via Map().  Should
// always be a multiple of SB_MEMORY_PAGE_SIZE.
size_t SbPageGetMappedBytes();

// Declaration of the allocator API.

#if defined(ADDRESS_SANITIZER)
#define SB_ALLOCATOR_PREFIX
#include <stdlib.h>
#else
#define SB_ALLOCATOR_PREFIX dl
#endif

#define SB_ALLOCATOR_MANGLER_2(prefix, fn) prefix##fn
#define SB_ALLOCATOR_MANGLER(prefix, fn) SB_ALLOCATOR_MANGLER_2(prefix, fn)
#define SB_ALLOCATOR(fn) SB_ALLOCATOR_MANGLER(SB_ALLOCATOR_PREFIX, fn)

void SB_ALLOCATOR(_malloc_init)();
void SB_ALLOCATOR(_malloc_finalize)();
void* SB_ALLOCATOR(malloc)(size_t size);
void* SB_ALLOCATOR(memalign)(size_t align, size_t size);
void* SB_ALLOCATOR(realloc)(void* ptr, size_t size);
void SB_ALLOCATOR(free)(void* ptr);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_DLMALLOC_PAGE_INTERNAL_H_

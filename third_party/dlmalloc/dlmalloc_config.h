/*
 * Copyright 2013 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _DLMALLOC_CONFIG_H_
#define _DLMALLOC_CONFIG_H_

#if defined(STARBOARD)
#include <sys/types.h>  // for ssize_t, maybe should add to starboard/types.h
#include "starboard/configuration.h"
#include "starboard/mutex.h"
// Define STARBOARD_IMPLEMENTATION to allow inclusion of an internal Starboard
// header. This is "okay" because dlmalloc is essentially an implementation
// detail of Starboard, and it would be statically linked into the Starboard DSO
// if Starboard was made into one.
#define STARBOARD_IMPLEMENTATION
#include "starboard/shared/dlmalloc/page_internal.h"
#else
#include <fcntl.h> // For HeapWalker to open files
#include <inttypes.h>
#include <stdint.h>

#include "lb_memory_pages.h"
#include "lb_mutex.h"
#include "lb_platform.h"
#endif

// Want this for all our targets so these symbols
// dont conflict with the normal malloc functions.
#define USE_DL_PREFIX 1

#if !defined (__LB_SHELL__FOR_RELEASE__)
#define MALLOC_INSPECT_ALL 1
#endif

// Only turn this on if needed- it's quite slow.
//#define DEBUG 1
#define ABORT_ON_ASSERT_FAILURE 0

#define USE_LOCKS 2 // Use our custom lock implementation
#define USE_SPIN_LOCKS 0
/* Locks */
#if defined(STARBOARD)
#define MLOCK_T SbMutex
#define INITIAL_LOCK(lk) (SbMutexCreate(lk) ? 0 : 1)
#define DESTROY_LOCK(lk) (SbMutexDestroy(lk) ? 0 : 1)
#define ACQUIRE_LOCK(lk) (SbMutexIsSuccess(SbMutexAcquire(lk)) ? 0 : 1)
#define RELEASE_LOCK(lk) (SbMutexRelease(lk) ? 0 : 1)
#define TRY_LOCK(lk)     (SbMutexIsSuccess(SbMutexAcquireTry(lk)) ? 0 : 1)
#elif defined(__LB_XB360__)
#define MLOCK_T               CRITICAL_SECTION
// The ..., 0 in some of these is to provide a "return value" of 0 to mimic
// pthread-style API.
#define ACQUIRE_LOCK(lk)      (EnterCriticalSection(lk), 0)
#define RELEASE_LOCK(lk)      LeaveCriticalSection(lk)
#define TRY_LOCK(lk)          TryEnterCriticalSection(lk)
#define INITIAL_LOCK(lk)      (InitializeCriticalSection(lk), 0)
#define DESTROY_LOCK(lk)      (DeleteCriticalSection(lk), 0)
#else
#define MLOCK_T lb_shell_mutex_t
#define INITIAL_LOCK(lk) lb_shell_mutex_init(lk)
#define DESTROY_LOCK(lk)  lb_shell_mutex_destroy(lk)
#define ACQUIRE_LOCK(lk)  lb_shell_mutex_lock(lk)
#define RELEASE_LOCK(lk)  lb_shell_mutex_unlock(lk)
#define TRY_LOCK(lk) lb_shell_mutex_trylock(lk)
#endif

#define NEED_GLOBAL_LOCK_INIT

#define HAVE_MREMAP 0
#define USE_BUILTIN_FFS 1
#if defined(STARBOARD)
#define malloc_getpagesize SB_MEMORY_PAGE_SIZE
#else
#define malloc_getpagesize LB_PAGE_SIZE
#endif

// Adapt Starboard configuration to old LBShell-style configuration.
#if defined(STARBOARD)
#if SB_HAS(MMAP)
#define LB_HAS_MMAP
#endif
#if SB_HAS(VIRTUAL_REGIONS)
#define LB_HAS_VIRTUAL_REGIONS
#endif
#endif  // defined(STARBOARD)

#if defined(LB_HAS_MMAP)
#define HAVE_MMAP 2  // 2 == use our custom implementation
#else
#define HAVE_MMAP 0
#endif

#if defined(LB_HAS_VIRTUAL_REGIONS)
#define HAVE_MORECORE 1
#define MORECORE lbshell_morecore
#else
#define HAVE_MORECORE 0
#endif

#if defined(STARBOARD)
// This is handled in SbAllocateChecked and friends.
#define MALLOC_FAILURE_ACTION NULL
#else
#define MALLOC_FAILURE_ACTION ABORT
#endif

#if defined(STARBOARD)

#if SB_HAS(MMAP)
#define DEFAULT_MMAP_THRESHOLD SB_DEFAULT_MMAP_THRESHOLD
#endif

#define MALLOC_ALIGNMENT SB_MALLOC_ALIGNMENT
#define FORCEINLINE SB_C_FORCE_INLINE
#define NOINLINE SB_C_NOINLINE
#define LACKS_UNISTD_H 1
#define LACKS_FCNTL_H 1
#define LACKS_SYS_PARAM_H 1
#define LACKS_SYS_MMAN_H 1
#define LACKS_STRINGS_H 1
#define LACKS_SYS_TYPES_H 1
#define LACKS_ERRNO_H 1
#define LACKS_STDLIB_H 1
#define LACKS_SCHED_H 1
#define LACKS_TIME_H 1
#define EINVAL 1
#define ENOMEM 1

// Compatibility patching back to LBShell names.
// TODO: Collapse this out when LBShell is gone.
#define lb_virtual_mem_t SbPageVirtualMemory
#define lb_get_total_system_memory SbPageGetTotalPhysicalMemoryBytes
#define lb_get_mmapped_bytes SbPageGetMappedBytes
#define lb_get_virtual_region_size SbPageGetVirtualRegionSize
#define lb_reserve_virtual_region SbPageReserveVirtualRegion
#define lb_unmap_and_free_physical SbPageUnmapAndFreePhysical
#define lb_release_virtual_region SbPageReleaseVirtualRegion
#define lb_allocate_physical_and_map SbPageAllocatePhysicalAndMap

// Other compatibility adjustments, forcing system calls back to Starboard.
#include "starboard/directory.h"
#include "starboard/file.h"
#include "starboard/log.h"
#define write SbFileWrite
#define close SbFileClose

void oom_fprintf(int ignored, const char *format, ...) {
  SB_UNREFERENCED_PARAMETER(ignored);
  va_list arguments;
  va_start(arguments, format);
  SbLogRawFormat(format, arguments);
  va_end(arguments);
}

#elif defined(__LB_ANDROID__)
#define MALLOC_ALIGNMENT ((size_t)16U)

#elif defined(__LB_LINUX__)
#define MALLOC_ALIGNMENT ((size_t)16U)

#elif defined(__LB_PS3__)
#define MALLOC_ALIGNMENT ((size_t)16U)
#define DEFAULT_MMAP_THRESHOLD ((size_t)(256 * 1024U))

#elif defined(__LB_PS4__)
#define MALLOC_ALIGNMENT ((size_t)16U)
// "Large" blocks come from a separate VM region.
#define DEFAULT_MMAP_THRESHOLD ((size_t)(256 * 1024U))

#elif defined(__LB_WIIU__)
#define MALLOC_ALIGNMENT ((size_t)8U)
#define DEFAULT_MMAP_THRESHOLD MAX_SIZE_T

#elif defined(__LB_XB1__)
#define MALLOC_ALIGNMENT ((size_t)16U)
#define DEFAULT_MMAP_THRESHOLD ((size_t)(256 * 1024U))

#define FORCEINLINE __forceinline

#elif defined(__LB_XB360__)
#define MALLOC_ALIGNMENT ((size_t)16U)
#define DEFAULT_MMAP_THRESHOLD ((size_t)(256 * 1024U))
#else
#error Platform-specific settings for dlmalloc must be defined here.
#endif

#ifdef __cplusplus
extern "C" {
#endif
void dl_malloc_init();
void dl_malloc_finalize();
void dlmalloc_ranges_np(uintptr_t *start1, uintptr_t *end1,
                        uintptr_t *start2, uintptr_t *end2,
                        uintptr_t *start3, uintptr_t *end3);
#if !defined(__LB_SHELL__FOR_RELEASE__)
void lbshell_inspect_mmap_heap(void(*handler)(void*, void *, size_t, void*),
                           void* arg);
void dldump_heap();
#endif
int dlmalloc_stats_np(size_t *system_size, size_t *in_use_size);

static void* lbshell_morecore(ssize_t size);
static void* lbshell_mmap(size_t size);
static int lbshell_munmap(void* addr, size_t size);
void oom_fprintf(int fd, const char *fmt, ...);
#ifdef __cplusplus
}  /* end of extern "C" */
#endif /* __cplusplus */

#endif /* ifndef _DLMALLOC_CONFIG_H_ */

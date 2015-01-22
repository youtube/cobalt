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

#include <fcntl.h> // For HeapWalker to open files
#include <inttypes.h>
#include <stdint.h>

#include "lb_memory_pages.h"
#include "lb_mutex.h"
#include "lb_platform.h"

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
#if defined(__LB_XB360__)
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
#define malloc_getpagesize LB_PAGE_SIZE

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

#if defined(__LB_ANDROID__)
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

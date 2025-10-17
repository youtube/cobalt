﻿// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/tls.h"

#if USE_LOCAL_TLS_EMULATION()

#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/immediate_crash.h"
#include "build/build_config.h"

#include <sys/mman.h>

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
#include <sys/prctl.h>
#endif

namespace base::allocator::dispatcher::internal {

void* MMapAllocator::AllocateMemory(size_t size_in_bytes) {
  void* const mmap_res = mmap(nullptr, size_in_bytes, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
#if defined(PR_SET_VMA) && defined(PR_SET_VMA_ANON_NAME)
  if (mmap_res != MAP_FAILED) {
    // Allow the anonymous memory region allocated by mmap(MAP_ANONYMOUS) to
    // be identified in /proc/$PID/smaps.  This helps improve visibility into
    // Chromium's memory usage on Android.
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, mmap_res, size_in_bytes,
          "tls-mmap-allocator");
  }
#endif
#endif

  return (mmap_res != MAP_FAILED) ? mmap_res : nullptr;
}

bool MMapAllocator::FreeMemoryForTesting(void* pointer_to_allocated,
                                         size_t size_in_bytes) {
  auto const munmap_res = munmap(pointer_to_allocated, size_in_bytes);
  return (munmap_res == 0);
}

bool PThreadTLSSystem::Setup(
    OnThreadTerminationFunction thread_termination_function) {
#if DCHECK_IS_ON()
  // Initialize must happen outside of the allocation path. Therefore, it is
  // secure to verify with DCHECK.
  DCHECK(!initialized_.exchange(true, std::memory_order_acq_rel));
#endif

  auto const key_create_res =
      pthread_key_create(&data_access_key_, thread_termination_function);

  // On some platforms creating a new pthread-key requires an allocation when a
  // given number of keys has been created. I.e. in glibc this limit is denoted
  // by PTHREAD_KEY_2NDLEVEL_SIZE. However, this value is neither present on all
  // systems nor accessible from here. Hence, we do not do any checks here.
  // However, we strongly recommend to setup the TLS system as early as possible
  // to avoid exceeding this limit.

  return (0 == key_create_res);
}

bool PThreadTLSSystem::TearDownForTesting() {
#if DCHECK_IS_ON()
  // TearDownForTesting must happen outside of the allocation path. Therefore,
  // it is secure to verify with DCHECK.
  DCHECK(initialized_.exchange(false, std::memory_order_acq_rel));
#endif

  auto const key_delete_res = pthread_key_delete(data_access_key_);
  return (0 == key_delete_res);
}

void* PThreadTLSSystem::GetThreadSpecificData() {
#if DCHECK_IS_ON()
  if (!initialized_.load(std::memory_order_acquire)) {
    return nullptr;
  }
#endif

  return pthread_getspecific(data_access_key_);
}

bool PThreadTLSSystem::SetThreadSpecificData(void* data) {
#if DCHECK_IS_ON()
  if (!initialized_.load(std::memory_order_acquire)) {
    return false;
  }
#endif

  return (0 == pthread_setspecific(data_access_key_, data));
}

}  // namespace base::allocator::dispatcher::internal

#endif  // USE_LOCAL_TLS_EMULATION()

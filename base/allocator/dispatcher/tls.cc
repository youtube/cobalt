// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/tls.h"

#include <string_view>

#if USE_LOCAL_TLS_EMULATION()

#include <sys/mman.h>

#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/debug/crash_logging.h"
#include "base/immediate_crash.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
#include <sys/prctl.h>
#endif

namespace base::allocator::dispatcher::internal {
namespace {
void Swap(std::atomic_bool& lh_op, std::atomic_bool& rh_op) {
  auto lh_op_value = lh_op.load(std::memory_order_relaxed);
  auto rh_op_value = rh_op.load(std::memory_order_relaxed);

  CHECK(lh_op.compare_exchange_strong(lh_op_value, rh_op_value));
  CHECK(rh_op.compare_exchange_strong(rh_op_value, lh_op_value));
}
}  // namespace

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

PThreadTLSSystem::PThreadTLSSystem() = default;

PThreadTLSSystem::PThreadTLSSystem(PThreadTLSSystem&& other) {
  std::swap(crash_key_, other.crash_key_);
  std::swap(data_access_key_, other.data_access_key_);

  Swap(initialized_, other.initialized_);
}

PThreadTLSSystem& PThreadTLSSystem::operator=(PThreadTLSSystem&& other) {
  std::swap(crash_key_, other.crash_key_);
  std::swap(data_access_key_, other.data_access_key_);

  Swap(initialized_, other.initialized_);

  return *this;
}

bool PThreadTLSSystem::Setup(
    OnThreadTerminationFunction thread_termination_function,
    std::string_view instance_id) {
  if (initialized_.exchange(true, std::memory_order_acq_rel)) {
    return true;
  }

  auto const key_create_res =
      pthread_key_create(&data_access_key_, thread_termination_function);

  return (0 == key_create_res);
}

bool PThreadTLSSystem::TearDownForTesting() {
  if (!initialized_.exchange(false, std::memory_order_acq_rel)) {
    return true;
  }

  crash_key_ = nullptr;

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

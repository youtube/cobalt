// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/shim/allocator_shim.h"

#include <ostream>

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/shim/winheap_stubs_win.h"

namespace {

using allocator_shim::AllocatorDispatch;

void* DefaultWinHeapMallocImpl(const AllocatorDispatch*,
                               size_t size,
                               void* context) {
  return allocator_shim::WinHeapMalloc(size);
}

void* DefaultWinHeapCallocImpl(const AllocatorDispatch* self,
                               size_t n,
                               size_t elem_size,
                               void* context) {
  // Overflow check.
  const size_t size = n * elem_size;
  if (elem_size != 0 && size / elem_size != n)
    return nullptr;

  void* result = DefaultWinHeapMallocImpl(self, size, context);
  if (result) {
    memset(result, 0, size);
  }
  return result;
}

void* DefaultWinHeapMemalignImpl(const AllocatorDispatch* self,
                                 size_t alignment,
                                 size_t size,
                                 void* context) {
  PA_CHECK(false) << "The windows heap does not support memalign.";
  return nullptr;
}

void* DefaultWinHeapReallocImpl(const AllocatorDispatch* self,
                                void* address,
                                size_t size,
                                void* context) {
  return allocator_shim::WinHeapRealloc(address, size);
}

void DefaultWinHeapFreeImpl(const AllocatorDispatch*,
                            void* address,
                            void* context) {
  allocator_shim::WinHeapFree(address);
}

size_t DefaultWinHeapGetSizeEstimateImpl(const AllocatorDispatch*,
                                         void* address,
                                         void* context) {
  return allocator_shim::WinHeapGetSizeEstimate(address);
}

void* DefaultWinHeapAlignedMallocImpl(const AllocatorDispatch*,
                                      size_t size,
                                      size_t alignment,
                                      void* context) {
  return allocator_shim::WinHeapAlignedMalloc(size, alignment);
}

void* DefaultWinHeapAlignedReallocImpl(const AllocatorDispatch*,
                                       void* ptr,
                                       size_t size,
                                       size_t alignment,
                                       void* context) {
  return allocator_shim::WinHeapAlignedRealloc(ptr, size, alignment);
}

void DefaultWinHeapAlignedFreeImpl(const AllocatorDispatch*,
                                   void* ptr,
                                   void* context) {
  allocator_shim::WinHeapAlignedFree(ptr);
}

}  // namespace

// Guarantee that default_dispatch is compile-time initialized to avoid using
// it before initialization (allocations before main in release builds with
// optimizations disabled).
constexpr AllocatorDispatch AllocatorDispatch::default_dispatch = {
    &DefaultWinHeapMallocImpl,
    &DefaultWinHeapMallocImpl, /* alloc_unchecked_function */
    &DefaultWinHeapCallocImpl,
    &DefaultWinHeapMemalignImpl,
    &DefaultWinHeapReallocImpl,
    &DefaultWinHeapFreeImpl,
    &DefaultWinHeapGetSizeEstimateImpl,
    nullptr, /* claimed_address */
    nullptr, /* batch_malloc_function */
    nullptr, /* batch_free_function */
    nullptr, /* free_definite_size_function */
    nullptr, /* try_free_default_function */
    &DefaultWinHeapAlignedMallocImpl,
    &DefaultWinHeapAlignedReallocImpl,
    &DefaultWinHeapAlignedFreeImpl,
    nullptr, /* next */
};

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_SHIM_NONSCANNABLE_ALLOCATOR_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_SHIM_NONSCANNABLE_ALLOCATOR_H_

#include <atomic>
#include <cstddef>
#include <memory>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/no_destructor.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc.h"

#if BUILDFLAG(USE_STARSCAN)
#include "base/allocator/partition_allocator/src/partition_alloc/starscan/metadata_allocator.h"
#endif
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace allocator_shim {

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
namespace internal {

// Represents allocator that contains memory for data-like objects (that don't
// contain pointers/references) and therefore doesn't require scanning by
// PCScan. An example would be strings or socket/IPC/file buffers. Use with
// caution.
template <bool quarantinable>
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) NonScannableAllocatorImpl final {
 public:
  static NonScannableAllocatorImpl& Instance();

  NonScannableAllocatorImpl(const NonScannableAllocatorImpl&) = delete;
  NonScannableAllocatorImpl& operator=(const NonScannableAllocatorImpl&) =
      delete;

  void* Alloc(size_t size);
  void Free(void*);

  // Returns PartitionRoot corresponding to the allocator, or nullptr if the
  // allocator is not enabled.
  partition_alloc::PartitionRoot* root() {
#if BUILDFLAG(USE_STARSCAN)
    if (!allocator_.get()) {
      return nullptr;
    }
    return allocator_->root();
#else
    return nullptr;
#endif  // BUILDFLAG(USE_STARSCAN)
  }

  void NotifyPCScanEnabled();

 private:
  template <typename>
  friend class partition_alloc::internal::base::NoDestructor;

  NonScannableAllocatorImpl();
  ~NonScannableAllocatorImpl();

#if BUILDFLAG(USE_STARSCAN)
  std::unique_ptr<partition_alloc::PartitionAllocator,
                  partition_alloc::internal::PCScanMetadataDeleter>
      allocator_;
  std::atomic_bool pcscan_enabled_{false};
#endif  // BUILDFLAG(USE_STARSCAN)
};

extern template class NonScannableAllocatorImpl<true>;
extern template class NonScannableAllocatorImpl<false>;

}  // namespace internal

using NonScannableAllocator = internal::NonScannableAllocatorImpl<true>;
using NonQuarantinableAllocator = internal::NonScannableAllocatorImpl<false>;

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace allocator_shim

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_SHIM_NONSCANNABLE_ALLOCATOR_H_

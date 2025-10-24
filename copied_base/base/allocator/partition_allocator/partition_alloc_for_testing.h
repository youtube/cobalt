// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FOR_TESTING_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FOR_TESTING_H_

#include "base/allocator/partition_allocator/partition_alloc.h"

namespace partition_alloc {
namespace internal {

constexpr bool AllowLeaks = true;
constexpr bool DisallowLeaks = false;

// A subclass of PartitionAllocator for testing. It will free all resources,
// i.e. allocated memory, memory inside freelist, and so on, when destructing
// it or when manually invoking reset().
// If need to check if there are any memory allocated but not freed yet,
// use allow_leaks=false. We will see CHECK failure inside reset() if any
// leak is detected. Otherwise (e.g. intentional leaks), use allow_leaks=true.
template <bool thread_safe, bool allow_leaks>
struct PartitionAllocatorForTesting : public PartitionAllocator<thread_safe> {
  PartitionAllocatorForTesting() : PartitionAllocator<thread_safe>() {}

  explicit PartitionAllocatorForTesting(PartitionOptions opts)
      : PartitionAllocator<thread_safe>() {
    PartitionAllocator<thread_safe>::init(opts);
  }

  ~PartitionAllocatorForTesting() { reset(); }

  PA_ALWAYS_INLINE void reset() {
    PartitionAllocator<thread_safe>::root()->ResetForTesting(allow_leaks);
  }
};

}  // namespace internal

using PartitionAllocatorForTesting =
    internal::PartitionAllocatorForTesting<internal::ThreadSafe,
                                           internal::DisallowLeaks>;

using PartitionAllocatorAllowLeaksForTesting =
    internal::PartitionAllocatorForTesting<internal::ThreadSafe,
                                           internal::AllowLeaks>;

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FOR_TESTING_H_

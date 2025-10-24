// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_ALLOCATION_GUARD_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_ALLOCATION_GUARD_H_

#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "build/build_config.h"

namespace partition_alloc {

#if PA_CONFIG(HAS_ALLOCATION_GUARD)

// Disallow allocations in the scope. Does not nest.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) ScopedDisallowAllocations {
 public:
  ScopedDisallowAllocations();
  ~ScopedDisallowAllocations();
};

// Disallow allocations in the scope. Does not nest.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) ScopedAllowAllocations {
 public:
  ScopedAllowAllocations();
  ~ScopedAllowAllocations();

 private:
  bool saved_value_;
};

#else

struct [[maybe_unused]] ScopedDisallowAllocations {};
struct [[maybe_unused]] ScopedAllowAllocations {};

#endif  // PA_CONFIG(HAS_ALLOCATION_GUARD)

}  // namespace partition_alloc

namespace base::internal {

using ::partition_alloc::ScopedAllowAllocations;
using ::partition_alloc::ScopedDisallowAllocations;

}  // namespace base::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_ALLOCATION_GUARD_H_

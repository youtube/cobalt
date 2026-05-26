// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/reuse_allocator_base.h"

#include <cstddef>
#include <utility>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"

namespace starboard {

ReuseAllocatorBase::ReuseAllocatorBase(Allocator* fallback_allocator,
                                       size_t max_capacity)
    : fallback_allocator_(fallback_allocator), max_capacity_(max_capacity) {
  SB_DCHECK(fallback_allocator_);
}

ReuseAllocatorBase::~ReuseAllocatorBase() {
  for (const auto& fallback_allocation : fallback_allocations_) {
    fallback_allocator_->Free(fallback_allocation.address);
  }
}

std::pair<void*, intptr_t> ReuseAllocatorBase::AllocateFallbackBlock(
    size_t* size_to_try,
    size_t alignment) {
  SB_DCHECK(size_to_try);
  SB_DCHECK_GT(*size_to_try, 0U);

  if (max_capacity_ && capacity_ + *size_to_try > max_capacity_) {
    return {nullptr, -1};
  }

  void* ptr = fallback_allocator_->AllocateForAlignment(size_to_try, alignment);
  if (ptr != nullptr) {
    intptr_t index = static_cast<intptr_t>(fallback_allocations_.size());
    fallback_allocations_.emplace_back(ptr, *size_to_try);
    capacity_ += *size_to_try;
    return {ptr, index};
  }
  return {nullptr, -1};
}

void ReuseAllocatorBase::DecommitFallbackAllocations() {
  SB_LOG(INFO) << "Allocator reached idle state, decommitting "
               << fallback_allocations_.size() << " fallback allocations.";
  for (const auto& fallback_allocation : fallback_allocations_) {
    fallback_allocator_->Decommit(fallback_allocation.address,
                                  fallback_allocation.size);
  }
}

}  // namespace starboard

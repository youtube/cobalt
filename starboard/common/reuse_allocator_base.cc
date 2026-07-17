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
#include "starboard/common/memory.h"

namespace starboard {

ReuseAllocatorBase::ReuseAllocatorBase(Allocator* fallback_allocator,
                                       size_t max_capacity,
                                       size_t retain_blocks,
                                       size_t conservative_decommit_blocks,
                                       bool aggressive_decommit_on_suspend)
    : fallback_allocator_(fallback_allocator),
      max_capacity_(max_capacity),
      retain_blocks_(retain_blocks),
      conservative_decommit_blocks_(conservative_decommit_blocks),
      aggressive_decommit_on_suspend_(aggressive_decommit_on_suspend) {
  SB_DCHECK(fallback_allocator_);
}

ReuseAllocatorBase::~ReuseAllocatorBase() {
  for (const auto& fallback_allocation : fallback_allocations_) {
    fallback_allocator_->Free(fallback_allocation.address);
  }
}

void ReuseAllocatorBase::DecommitAllDecommitableBlocks() {
  if (aggressive_decommit_on_suspend_) {
    // Aggressively decommit all idle blocks (including retained and
    // lazily-freed blocks) with MADV_DONTNEED (conservative=false). This
    // instantly drops the process RSS to prevent the OS Out-Of-Memory killer
    // from terminating the app in background/suspended states.
    for (auto& fallback_block : fallback_allocations_) {
      if (fallback_block.state != kActiveInUse &&
          fallback_block.state != kIdleDecommitted) {
        fallback_allocator_->Decommit(fallback_block.address,
                                      fallback_block.size,
                                      /*conservative=*/false);
        fallback_block.state = kIdleDecommitted;
      }
    }
    has_pending_decommits_ = false;
  } else {
    if (!has_pending_decommits_) {
      return;
    }
    for (auto it = fallback_allocations_.rbegin();
         it != fallback_allocations_.rend(); ++it) {
      if (it->state == kIdlePendingDecommit) {
        fallback_allocator_->Decommit(it->address, it->size,
                                      /*conservative=*/false);
        it->state = kIdleDecommitted;
      } else if (it->state == kIdlePendingFree) {
        fallback_allocator_->Decommit(it->address, it->size,
                                      /*conservative=*/true);
        it->state = kIdleFreed;
      }
    }
    has_pending_decommits_ = false;
  }
}

std::pair<void*, intptr_t> ReuseAllocatorBase::AllocateFallbackBlock(
    size_t* size_to_try,
    size_t alignment) {
  SB_DCHECK(size_to_try);
  SB_DCHECK_GT(*size_to_try, 0U);

  for (size_t i = 0; i < fallback_allocations_.size(); ++i) {
    auto& fallback_block = fallback_allocations_[i];
    if (fallback_block.state != kActiveInUse &&
        fallback_block.size >= *size_to_try &&
        MemoryIsAligned(fallback_block.address, alignment)) {
      fallback_block.state = kActiveInUse;
      *size_to_try = fallback_block.size;
      return {fallback_block.address, static_cast<intptr_t>(i)};
    }
  }

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

void ReuseAllocatorBase::ReclaimFallbackBlocks() {
  SB_LOG(INFO) << "Allocator reached idle state, reclaiming "
               << fallback_allocations_.size() << " fallback allocations.";
  allocation_counter_ = 0;
  for (size_t i = 0; i < fallback_allocations_.size(); ++i) {
    auto& fallback_block = fallback_allocations_[i];
    if (i < retain_blocks_) {
      fallback_block.state = kIdleRetained;
    } else if (i < retain_blocks_ + conservative_decommit_blocks_) {
      fallback_block.state = kIdlePendingFree;
      has_pending_decommits_ = true;
    } else {
      fallback_block.state = kIdlePendingDecommit;
      has_pending_decommits_ = true;
    }
  }
}

void ReuseAllocatorBase::TryToDecommitOneBlock() {
  if (!has_pending_decommits_) {
    return;
  }
  ++allocation_counter_;
  if (allocation_counter_ % 100 == 0) {
    for (auto it = fallback_allocations_.rbegin();
         it != fallback_allocations_.rend(); ++it) {
      if (it->state == kIdlePendingDecommit) {
        fallback_allocator_->Decommit(it->address, it->size,
                                      /*conservative=*/false);
        it->state = kIdleDecommitted;
        return;
      }
      if (it->state == kIdlePendingFree) {
        fallback_allocator_->Decommit(it->address, it->size,
                                      /*conservative=*/true);
        it->state = kIdleFreed;
        return;
      }
    }
    has_pending_decommits_ = false;
  }
}

}  // namespace starboard

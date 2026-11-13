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
#include <cstring>
#include <utility>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/memory.h"

namespace starboard {

ReuseAllocatorBase::ReuseAllocatorBase(Allocator* fallback_allocator,
                                       size_t max_capacity,
                                       size_t retain_blocks,
                                       size_t conservative_decommit_blocks,
                                       bool aggressive_decommit_on_suspend,
                                       bool memset_on_reclaim,
                                       bool mark_as_cold_on_reclaim)
    : fallback_allocator_(fallback_allocator),
      max_capacity_(max_capacity),
      retain_blocks_(retain_blocks),
      conservative_decommit_blocks_(conservative_decommit_blocks),
      aggressive_decommit_on_suspend_(aggressive_decommit_on_suspend),
      memset_on_reclaim_(memset_on_reclaim),
      mark_as_cold_on_reclaim_(mark_as_cold_on_reclaim) {
  SB_DCHECK(fallback_allocator_);
}

ReuseAllocatorBase::~ReuseAllocatorBase() {
  for (const auto& fallback_allocation : fallback_allocations_) {
    fallback_allocator_->Free(fallback_allocation.address);
  }
}

void ReuseAllocatorBase::SetSuspended(bool is_suspended) {
  if (is_suspended_ == is_suspended) {
    return;
  }
  is_suspended_ = is_suspended;
  if (is_suspended_) {
    DecommitAllDecommitableBlocks();
  }
}

void ReuseAllocatorBase::DecommitAllDecommitableBlocks(
    bool include_retained_blocks) {
  if ((is_suspended_ && aggressive_decommit_on_suspend_) ||
      include_retained_blocks) {
    // Aggressively decommit all idle blocks (including retained and
    // lazily-freed blocks) with MADV_DONTNEED (kAggressive). This
    // instantly drops the process RSS to prevent the OS Out-Of-Memory killer
    // from terminating the app in background/suspended states.
    for (auto& fallback_block : fallback_allocations_) {
      if (fallback_block.state != kActiveInUse &&
          fallback_block.state != kIdleDecommitted) {
        DecommitBlock(fallback_block, DecommitMode::kAggressive);
      }
    }
    has_pending_decommits_ = false;
  } else {
    if (!has_pending_decommits_) {
      return;
    }
    for (auto it = fallback_allocations_.rbegin();
         it != fallback_allocations_.rend(); ++it) {
      TryDecommitPendingBlock(*it);
    }
    has_pending_decommits_ = false;
  }
}

void ReuseAllocatorBase::TryToDecommitOneBlock() {
  if (!has_pending_decommits_) {
    return;
  }
  if (decommit_grace_ticks_remaining_ > 0) {
    --decommit_grace_ticks_remaining_;
    return;
  }
  for (auto it = fallback_allocations_.rbegin();
       it != fallback_allocations_.rend(); ++it) {
    if (TryDecommitPendingBlock(*it)) {
      return;
    }
  }
  has_pending_decommits_ = false;
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
      if (fallback_block.state == kIdleDecommitted) {
        SB_DCHECK_GE(decommitted_bytes_, fallback_block.size);
        decommitted_bytes_ -= fallback_block.size;
      }
      fallback_block.state = kActiveInUse;
      decommit_grace_ticks_remaining_ = kDecommitGraceTicks;
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
    decommit_grace_ticks_remaining_ = kDecommitGraceTicks;
    return {ptr, index};
  }
  return {nullptr, -1};
}

void ReuseAllocatorBase::ReclaimFallbackBlocks() {
  SB_LOG(INFO) << "Allocator reached idle state, reclaiming "
               << fallback_allocations_.size() << " fallback allocations.";
  decommit_grace_ticks_remaining_ = kDecommitGraceTicks;
  has_pending_decommits_ = false;
  for (size_t i = 0; i < fallback_allocations_.size(); ++i) {
    auto& fallback_block = fallback_allocations_[i];

    // Do nothing if the block has already been decommitted or freed.
    if (fallback_block.state == kIdleDecommitted ||
        fallback_block.state == kIdleFreed) {
      continue;
    }

    // Skip zeroing or marking cold if `DecommitAllDecommitableBlocks()` below
    // is about to discard the block immediately with MADV_DONTNEED.
    const bool will_aggressively_decommit_now =
        is_suspended_ && (aggressive_decommit_on_suspend_ ||
                          i >= retain_blocks_ + conservative_decommit_blocks_);
    if (fallback_block.state == kActiveInUse &&
        !will_aggressively_decommit_now) {
      if (memset_on_reclaim_) {
        memset(fallback_block.address, 0, fallback_block.size);
      }
      if (mark_as_cold_on_reclaim_) {
        fallback_allocator_->Decommit(fallback_block.address,
                                      fallback_block.size, DecommitMode::kCold);
      }
    }
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
  if (is_suspended_) {
    DecommitAllDecommitableBlocks();
  }
}

void ReuseAllocatorBase::DecommitBlock(FallbackAllocation& block,
                                       DecommitMode mode) {
  fallback_allocator_->Decommit(block.address, block.size, mode);
  if (mode == DecommitMode::kAggressive) {
    block.state = kIdleDecommitted;
    decommitted_bytes_ += block.size;
    SB_DCHECK_LE(decommitted_bytes_, capacity_);
  } else {
    block.state = kIdleFreed;
  }
}

bool ReuseAllocatorBase::TryDecommitPendingBlock(FallbackAllocation& block) {
  if (block.state == kIdlePendingDecommit) {
    DecommitBlock(block, DecommitMode::kAggressive);
    return true;
  }
  if (block.state == kIdlePendingFree) {
    DecommitBlock(block, DecommitMode::kConservative);
    return true;
  }
  return false;
}

}  // namespace starboard

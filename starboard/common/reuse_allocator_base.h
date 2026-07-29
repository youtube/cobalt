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

#ifndef STARBOARD_COMMON_REUSE_ALLOCATOR_BASE_H_
#define STARBOARD_COMMON_REUSE_ALLOCATOR_BASE_H_

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "starboard/common/allocator.h"
#include "starboard/configuration.h"

namespace starboard {

// Abstract base class for memory reuse allocators.
//
// Reason for existence: Manages the lifetime of backing memory blocks
// sub-allocated from a fallback Allocator and enforces overall memory pool
// capacity constraints.
//
// Expected lifetime and ownership: Owned by concrete reuse allocator subclasses
// or memory pool strategies. Backing fallback blocks remain owned by this class
// until its destruction.
//
// Threading model: Not thread-safe. External synchronization is required when
// accessing or allocating memory concurrently across multiple threads.
class ReuseAllocatorBase : public Allocator {
 public:
  // Allocator methods.
  size_t GetCapacity() const override { return capacity_; }

  // Immediately decommit all remaining decommitable blocks (e.g. on app
  // suspend).
  void DecommitAllDecommitableBlocks();

 protected:
  ReuseAllocatorBase(Allocator* fallback_allocator,
                     size_t max_capacity,
                     size_t retain_blocks,
                     size_t conservative_decommit_blocks,
                     bool aggressive_decommit_on_suspend = false);
  ~ReuseAllocatorBase() override;

  bool CapacityExceeded() const {
    return max_capacity_ && (capacity_ > max_capacity_);
  }

  bool IsValidFallbackIndex(intptr_t fallback_index) const {
    return fallback_index >= 0 &&
           static_cast<size_t>(fallback_index) < fallback_allocations_.size();
  }

  // Request a new backing buffer from the fallback allocator with capacity and
  // alignment constraints, and register it for ownership tracking.
  // Returns pair of {backing_address, fallback_index} to allow caller unpacking
  // via structured binding. Returns {nullptr, -1} on failure or if pool
  // capacity is exceeded.
  std::pair<void*, intptr_t> AllocateFallbackBlock(size_t* size_to_try,
                                                   size_t alignment);

  // Reclaims all backing allocations to the base idle pool (subclass-driven).
  void ReclaimFallbackBlocks();

  // Step counter to amortize decommits across sequential active allocations.
  void TryToDecommitOneBlock();

  // Enumerates fallback backing allocations. Templated to allow zero-cost
  // compiler inlining for capturing lambdas and avoid std::function overhead.
  template <typename Func>
  void EnumerateFallbackAllocations(Func callback) const {
    for (size_t i = 0; i < fallback_allocations_.size(); ++i) {
      callback(static_cast<intptr_t>(i), fallback_allocations_[i].address,
               fallback_allocations_[i].size);
    }
  }

 private:
  enum BlockState {
    // Actively allocated and managed by subclass.
    kActiveInUse,
    // Reclaimed to base pool, retained fully committed in memory.
    kIdleRetained,
    // Reclaimed to base pool, pending MADV_FREE decommit.
    kIdlePendingFree,
    // Reclaimed to base pool, pending MADV_DONTNEED decommit.
    kIdlePendingDecommit,
    // Reclaimed to base pool, successfully decommitted via MADV_FREE.
    kIdleFreed,
    // Reclaimed to base pool, successfully decommitted via MADV_DONTNEED.
    kIdleDecommitted,
  };

  struct FallbackAllocation {
    void* address;
    size_t size;
    BlockState state = kActiveInUse;

    FallbackAllocation(void* init_address, size_t init_size)
        : address(init_address), size(init_size) {}
  };

  // We will allocate from the given allocator whenever we can't find pre-used
  // memory to allocate.
  Allocator* const fallback_allocator_;

  // If non-zero, this is an upper bound on how large we will let the total
  // memory pool capacity expand.
  const size_t max_capacity_;

  // How many blocks to retain (not decommit) when the pool becomes idle.
  const size_t retain_blocks_ = 0;

  // How many blocks to decommit with MADV_FREE after retaining.
  const size_t conservative_decommit_blocks_ = 0;

  // Whether to aggressively decommit all idle blocks on app suspend.
  const bool aggressive_decommit_on_suspend_ = false;

  // A list of all backing memory blocks allocated from the fallback allocator.
  // We keep track of this so we can decommit on idle and free them upon
  // destruction.
  std::vector<FallbackAllocation> fallback_allocations_;

  // How many total bytes we currently have allocated from the fallback
  // allocator.
  size_t capacity_ = 0;

  // Counter to amortize decommits over multiple allocations.
  size_t allocation_counter_ = 0;

  // Set to true when idle reclamation stages blocks for decommit.
  bool has_pending_decommits_ = false;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_REUSE_ALLOCATOR_BASE_H_

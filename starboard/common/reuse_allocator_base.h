// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <vector>

#include "starboard/common/allocator.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"

namespace starboard {

// TODO: b/369245553 - Cobalt: Add unit tests once Starboard unittests are
//                             enabled.

// The base class of allocators designed to accommodate cases where the memory
// allocated may not be efficient or safe to access via the CPU.  It solves
// this problem by maintaining all allocation meta data is outside of the
// allocated memory.  It is passed a fallback allocator that it can request
// additional memory from as needed.
class ReuseAllocatorBase : public Allocator {
 public:
  void* Allocate(size_t size) override;
  void* Allocate(size_t size, size_t alignment) override;

  // Marks the memory block as being free and it will then become recyclable
  void Free(void* memory) override;

  size_t GetCapacity() const override { return capacity_; }
  size_t GetAllocated() const override { return total_allocated_; }

  bool CapacityExceeded() const {
    return max_capacity_ && (capacity_ > max_capacity_);
  }

  void PrintAllocations(bool align_allocated_size,
                        int max_allocations_to_print) const override;

  bool TryFree(void* memory);

  size_t max_capacity() const { return max_capacity_; }

 protected:
  class MemoryBlock {
   public:
    MemoryBlock() = default;
    MemoryBlock(int fallback_allocation_index, void* address, size_t size)
        : fallback_allocation_index_(fallback_allocation_index),
          address_(address),
          size_(size) {}
    ~MemoryBlock() { SB_DCHECK_GE(fallback_allocation_index_, 0); }

    void* address() const {
      SB_DCHECK_GE(fallback_allocation_index_, 0);
      return address_;
    }
    size_t size() const {
      SB_DCHECK_GE(fallback_allocation_index_, 0);
      return size_;
    }

    bool operator<(const MemoryBlock& other) const {
      SB_DCHECK_GE(fallback_allocation_index_, 0);
      SB_DCHECK_GE(other.fallback_allocation_index_, 0);

      if (fallback_allocation_index_ < other.fallback_allocation_index_) {
        return true;
      }
      if (fallback_allocation_index_ > other.fallback_allocation_index_) {
        return false;
      }
      return address_ < other.address_;
    }

    // If the current block and |other| can be combined into a continuous memory
    // block, store the conmbined block in the current block and return true.
    // Otherwise return false.
    bool Merge(const MemoryBlock& other);
    // Return true if the current block can be used to fulfill an allocation
    // with the given size and alignment.
    bool CanFulfill(size_t request_size, size_t alignment) const;
    // Allocate a block from this block with the given size and alignment.
    // Store the allocated block in |allocated|.  If the rest space is large
    // enough to form a block, it will be stored into |free|.  Otherwise the
    // whole block is stored into |allocated|.
    // Note that the call of this function has to ensure that CanFulfill() is
    // already called on this block and returns true.
    void Allocate(size_t request_size,
                  size_t alignment,
                  bool allocate_from_front,
                  MemoryBlock* allocated,
                  MemoryBlock* free) const;

   private:
    // TODO: b/369245553 - Cobalt: Optimize memory usage for bookkeeping
    // as there can be ~8000 or more allocations during playback.
    int fallback_allocation_index_ = -1;
    void* address_ = nullptr;
    size_t size_ = 0;
  };

  // Freelist sorted by address.
  typedef std::set<MemoryBlock> FreeBlockSet;

  ReuseAllocatorBase(Allocator* fallback_allocator,
                     size_t initial_capacity,
                     size_t allocation_increment,
                     size_t max_capacity = 0);
  ~ReuseAllocatorBase() override;

  // The inherited class should implement this function to inform the base
  // class which free block to take.  It returns |end| if no suitable free
  // block is found.  When |allocate_from_front| is set to true, the allocation
  // will take place in the front of a free block if the free block is big
  // enough to fulfill this allocation and produce another free block.
  // Otherwise the allocation will take place from the back.
  virtual FreeBlockSet::iterator FindFreeBlock(size_t size,
                                               size_t alignment,
                                               FreeBlockSet::iterator begin,
                                               FreeBlockSet::iterator end,
                                               bool* allocate_from_front) = 0;

 private:
  // Map from pointers we returned to the user, back to memory blocks.
  typedef std::map<void*, MemoryBlock> AllocatedBlockMap;

  FreeBlockSet::iterator ExpandToFit(size_t size, size_t alignment);

  void AddAllocatedBlock(void* address, const MemoryBlock& block);
  FreeBlockSet::iterator AddFreeBlock(MemoryBlock block_to_add);
  void RemoveFreeBlock(FreeBlockSet::iterator it);

  AllocatedBlockMap allocated_blocks_;
  FreeBlockSet free_blocks_;

  // We will allocate from the given allocator whenever we can't find pre-used
  // memory to allocate.
  Allocator* const fallback_allocator_;
  const size_t allocation_increment_;

  // If non-zero, this is an upper bound on how large we will let the capacity
  // expand.
  const size_t max_capacity_;

  // A list of allocations made from the fallback allocator.  We keep track of
  // this so that we can free them all upon our destruction.
  std::vector<void*> fallback_allocations_;

  // How much we have allocated from the fallback allocator.
  size_t capacity_;

  // How much has been allocated from us.
  size_t total_allocated_;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_REUSE_ALLOCATOR_BASE_H_

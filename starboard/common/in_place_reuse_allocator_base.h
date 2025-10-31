// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_IN_PLACE_REUSE_ALLOCATOR_BASE_H_
#define STARBOARD_COMMON_IN_PLACE_REUSE_ALLOCATOR_BASE_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

// The base class of memory-pool based allocators, where the underlying memory
// pool can be efficiently and safely accessed by the CPU, so the allocation
// metadata can be kept alongside the allocated memory.  It is passed a fallback
// allocator that it can request additional memory from as needed.
class InPlaceReuseAllocatorBase : public Allocator {
 public:
  void* Allocate(size_t size) override;
  void* Allocate(size_t size, size_t alignment) override;

  // Marks the memory block as being free and it will then become recyclable
  void Free(void* memory) override;

  size_t GetCapacity() const override { return capacity_in_bytes_; }
  size_t GetAllocated() const override { return total_allocated_in_bytes_; }

  bool CapacityExceeded() const {
    return max_capacity_in_bytes_ &&
           (capacity_in_bytes_ > max_capacity_in_bytes_);
  }

  void PrintAllocations(bool align_allocated_size,
                        int max_allocations_to_print) const override;

  bool TryFree(void* memory);

  size_t max_capacity() const { return max_capacity_in_bytes_; }

 protected:
  // The metadata of an allocated block stored at the very beginning of the
  // block when it's allocated.  It natually maintains a double linked list to
  // allow traverse through all allocated blocks.
  struct BlockMetadata {
    const InPlaceReuseAllocatorBase* signature;
    intptr_t fallback_index;
    intptr_t size;
    BlockMetadata* previous;
    BlockMetadata* next;
  };

  class MemoryBlock {
   public:
    MemoryBlock() = default;
    MemoryBlock(intptr_t fallback_allocation_index, void* address, size_t size)
        : fallback_allocation_index_(fallback_allocation_index),
          address_(address),
          size_(size) {}
    ~MemoryBlock() { SB_DCHECK_GE(fallback_allocation_index_, 0); }

    intptr_t fallback_allocation_index() const {
      return fallback_allocation_index_;
    }
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
    intptr_t fallback_allocation_index_ = -1;
    void* address_ = nullptr;
    size_t size_ = 0;
  };

  // Freelist sorted by address.
  typedef std::set<MemoryBlock> FreeBlockSet;

  InPlaceReuseAllocatorBase(Allocator* fallback_allocator,
                            size_t initial_capacity,
                            size_t allocation_increment,
                            size_t max_capacity = 0);
  ~InPlaceReuseAllocatorBase() override;

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
  // Pointer and size of the block allocated from the fallback allocator.
  struct FallbackAllocation {
    void* address;
    size_t size;

    FallbackAllocation(void* addr, size_t s) : address(addr), size(s) {}
  };

  FreeBlockSet::iterator ExpandToFit(size_t size, size_t alignment);

  void AddAllocatedBlock(const MemoryBlock& block);
  FreeBlockSet::iterator AddFreeBlock(MemoryBlock block_to_add);
  void RemoveFreeBlock(FreeBlockSet::iterator it);

  BlockMetadata* allocated_block_head_ = nullptr;
  FreeBlockSet free_blocks_;

  // The free operations are batched, so it can be freed by discarding all
  // allocated blocks (i.e. setting `allocated_block_head_` to nullptr) when all
  // blocks are freed.  This is much faster than individually freeing thousands
  // of blocks.
  std::vector<void*> pending_frees_;

  // We will allocate from the given allocator whenever we can't find pre-used
  // memory to allocate.
  Allocator* const fallback_allocator_;
  const size_t allocation_increment_;

  // If non-zero, this is an upper bound on how large we will let the capacity
  // expand.
  const size_t max_capacity_in_bytes_;

  // A list of allocations made from the fallback allocator.  We keep track of
  // this so that we can free them all upon our destruction.
  std::vector<FallbackAllocation> fallback_allocations_;

  // How much we have allocated from the fallback allocator.
  size_t capacity_in_bytes_ = 0;

  // How many bytes has been allocated from us.
  size_t total_allocated_in_bytes_ = 0;

  // How many allocations has been allocated from us.
  size_t total_allocated_blocks_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_IN_PLACE_REUSE_ALLOCATOR_BASE_H_

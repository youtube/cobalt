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

#ifndef STARBOARD_COMMON_EXTERNAL_METADATA_REUSE_ALLOCATOR_BASE_H_
#define STARBOARD_COMMON_EXTERNAL_METADATA_REUSE_ALLOCATOR_BASE_H_

#include <cstddef>
#include <map>
#include <set>

#include "starboard/common/allocator.h"
#include "starboard/common/check_op.h"
#include "starboard/common/reuse_allocator_base.h"

namespace starboard {

// TODO: b/369245553 - Cobalt: Add unit tests once Starboard unittests are
//                             enabled.

// Base class for memory-pool based allocators with external metadata tracking.
//
// Reason for existence: Accommodates cases where allocated memory may not be
// efficient or safe to access directly via the CPU (e.g., specialized GPU or
// media buffers). Solves this by maintaining all allocation bookkeeping
// metadata in an external data structure outside the allocated buffers.
//
// Expected lifetime and ownership: Typically instantiated and owned by media
// pipeline buffer pools, and held for the entire app lifetime across multiple
// playback sessions.
//
// Threading model: Not thread-safe. Callers must provide external
// synchronization if methods are called from concurrent threads.
class ExternalMetadataReuseAllocatorBase : public ReuseAllocatorBase {
 public:
  void* Allocate(size_t size) override;
  void* Allocate(size_t size, size_t alignment) override;

  // Marks the memory block as being free and it will then become recyclable
  void Free(void* memory) override;

  size_t GetAllocated() const override { return total_allocated_; }

  void PrintAllocations(bool align_allocated_size,
                        int max_allocations_to_print) const override;

  bool TryFree(void* memory);

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

  ExternalMetadataReuseAllocatorBase(Allocator* fallback_allocator,
                                     size_t initial_capacity,
                                     size_t allocation_increment,
                                     size_t max_capacity);
  ExternalMetadataReuseAllocatorBase(
      Allocator* fallback_allocator,
      size_t initial_capacity,
      size_t allocation_increment,
      size_t max_capacity,
      bool enable_decommit_on_idle,
      size_t retain_blocks,
      size_t conservative_decommit_blocks,
      bool aggressive_decommit_on_suspend = false);
  ~ExternalMetadataReuseAllocatorBase() override;

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
  const size_t allocation_increment_;

  // Whether to decommit memory when the pool becomes idle.
  const bool enable_decommit_on_idle_;

  // How much has been allocated from us.
  size_t total_allocated_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_EXTERNAL_METADATA_REUSE_ALLOCATOR_BASE_H_

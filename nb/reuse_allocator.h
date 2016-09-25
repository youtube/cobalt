/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NB_REUSE_ALLOCATOR_H_
#define NB_REUSE_ALLOCATOR_H_

#include <map>
#include <set>
#include <vector>

#include "nb/allocator.h"

namespace nb {

// An allocator designed to accomodate cases where the memory allocated may not
// be efficient or safe to access via the CPU.  It solves this problem by
// maintaining all allocation meta data is outside of the allocated memory.
// It is passed a fallback allocator that it can request additional memory
// from as needed.
// The default allocation strategy for the allocator is first-fit, i.e. it will
// scan for free blocks sorted by addresses and allocate from the first free
// block that can fulfill the allocation.  However, in some situations the
// majority of the allocations can be small ones with some large allocations.
// This may cause serious fragmentations and the failure of large allocations.
// If |small_allocation_threshold| in the ctor is set to a non-zero value, the
// class will allocate small allocations whose sizes are less than or equal to
// the threshold using last-fit, i.e. it will scan from the back to the front
// for free blocks.  This way the allocation for large blocks and small blocks
// are separated thus cause much less fragmentations.
class ReuseAllocator : public Allocator {
 public:
  explicit ReuseAllocator(Allocator* fallback_allocator);
  // When |small_allocation_threshold| is non-zero, this class will allocate
  // its full capacity from the |fallback_allocator| in the ctor so it is
  // possible for the class to use the last-fit allocation strategy.  See the
  // class comment above for more details.
  ReuseAllocator(Allocator* fallback_allocator,
                 std::size_t capacity,
                 std::size_t small_allocation_threshold);
  virtual ~ReuseAllocator();

  // Search free memory blocks for an existing one, and if none are large
  // enough, allocate a new one from no-free memory and return that.
  void* Allocate(std::size_t size);
  void* Allocate(std::size_t size, std::size_t alignment);

  // Marks the memory block as being free and it will then become recyclable
  void Free(void* memory);

  std::size_t GetCapacity() const { return capacity_; }
  std::size_t GetAllocated() const { return total_allocated_; }

  void PrintAllocations() const;

 private:
  class MemoryBlock {
   public:
    MemoryBlock() : address_(0), size_(0) {}
    MemoryBlock(void* address, std::size_t size)
        : address_(address), size_(size) {}

    void* address() const { return address_; }
    std::size_t size() const { return size_; }

    void set_address(void* address) { address_ = address; }
    void set_size(std::size_t size) { size_ = size; }

    bool operator<(const MemoryBlock& other) const {
      return address_ < other.address_;
    }
    // If the current block and |other| can be combined into a continuous memory
    // block, store the conmbined block in the current block and return true.
    // Otherwise return false.
    bool Merge(const MemoryBlock& other);
    // Return true if the current block can be used to fulfill an allocation
    // with the given size and alignment.
    bool CanFullfill(std::size_t request_size, std::size_t alignment) const;
    // Allocate a block from this block with the given size and alignment.
    // Store the allocated block in |allocated|.  If the rest space is large
    // enough to form a block, it will be stored into |free|.  Otherwise the
    // whole block is stored into |allocated|.
    // Note that the call of this function has to ensure that CanFulfill() is
    // already called on this block and returns true.
    void Allocate(std::size_t request_size,
                  std::size_t alignment,
                  bool allocate_from_front,
                  MemoryBlock* allocated,
                  MemoryBlock* free) const;

   private:
    void* address_;
    std::size_t size_;
  };

  // Freelist sorted by address.
  typedef std::set<MemoryBlock> FreeBlockSet;
  // Map from pointers we returned to the user, back to memory blocks.
  typedef std::map<void*, MemoryBlock> AllocatedBlockMap;

  FreeBlockSet::iterator AddFreeBlock(MemoryBlock block_to_add);
  void RemoveFreeBlock(FreeBlockSet::iterator it);

  FreeBlockSet free_blocks_;
  AllocatedBlockMap allocated_blocks_;

  // We will allocate from the given allocator whenever we can't find pre-used
  // memory to allocate.
  Allocator* fallback_allocator_;

  // Any allocations with size less than or equal to the following threshold
  // will be allocated from the back of the pool.  See the comment of the class
  // for more details.
  std::size_t small_allocation_threshold_;

  // A list of allocations made from the fallback allocator.  We keep track of
  // this so that we can free them all upon our destruction.
  std::vector<void*> fallback_allocations_;

  // How much we have allocated from the fallback allocator.
  std::size_t capacity_;

  // How much has been allocated from us.
  std::size_t total_allocated_;
};

}  // namespace nb

#endif  // NB_REUSE_ALLOCATOR_H_

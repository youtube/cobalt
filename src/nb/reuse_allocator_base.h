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

#ifndef NB_REUSE_ALLOCATOR_BASE_H_
#define NB_REUSE_ALLOCATOR_BASE_H_

#include <map>
#include <set>
#include <vector>

#include "nb/allocator.h"
#include "starboard/configuration.h"
#include "starboard/types.h"

namespace nb {

// The base class of allocators designed to accommodate cases where the memory
// allocated may not be efficient or safe to access via the CPU.  It solves
// this problem by maintaining all allocation meta data is outside of the
// allocated memory.  It is passed a fallback allocator that it can request
// additional memory from as needed.
class ReuseAllocatorBase : public Allocator {
 public:
  void* Allocate(std::size_t size) override;
  void* Allocate(std::size_t size, std::size_t alignment) override;

  // Marks the memory block as being free and it will then become recyclable
  void Free(void* memory) override;

  std::size_t GetCapacity() const override { return capacity_; }
  std::size_t GetAllocated() const override { return total_allocated_; }

  bool CapacityExceeded() const {
    return max_capacity_ && (capacity_ > max_capacity_);
  }

  void PrintAllocations() const override;

  bool TryFree(void* memory);

  // Try to allocate a memory block for the |*size_hint| passed in.  If there is
  // no such block available, the function may return a block whose size is less
  // than |*size_hint| and set |*size_hint| to that size.  |context| will be
  // passed to FindBestFreeBlock() as is, which is useful when the user of a
  // sub-class wants to pass extra information along with the allocation request
  // to FindBestFreeBlock().  The function never sets |*size_hint| to a value
  // greater than the value passed in.
  // This allows the caller to allocate multiple smaller blocks to fulfill a
  // large allocation request.
  void* AllocateBestBlock(std::size_t alignment,
                          intptr_t context,
                          std::size_t* size_hint);

  std::size_t max_capacity() const { return max_capacity_; }
  void set_max_capacity(std::size_t max_capacity) {
    max_capacity_ = max_capacity;
  }

 protected:
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
    std::size_t requested_size_;
  };

  // Freelist sorted by address.
  typedef std::set<MemoryBlock> FreeBlockSet;

  ReuseAllocatorBase(Allocator* fallback_allocator,
                     std::size_t initial_capacity,
                     std::size_t allocation_increment,
                     std::size_t max_capacity = 0);
  ~ReuseAllocatorBase() override;

  // The inherited class should implement this function to inform the base
  // class which free block to take.  It returns |end| if no suitable free
  // block is found.  When |allocate_from_front| is set to true, the allocation
  // will take place in the front of a free block if the free block is big
  // enough to fulfill this allocation and produce another free block.
  // Otherwise the allocation will take place from the back.
  virtual FreeBlockSet::iterator FindFreeBlock(std::size_t size,
                                               std::size_t alignment,
                                               FreeBlockSet::iterator begin,
                                               FreeBlockSet::iterator end,
                                               bool* allocate_from_front) = 0;

  // The inherited class can implement this function to return a block whose
  // size might be smaller than the |size| passed in.  AllocateBestBlock() uses
  // this functional internally.  The default implementation simply calls
  // FindFreeBlock() and fails if there isn't a block that is large enough for
  // |size| bytes.
  virtual FreeBlockSet::iterator FindBestFreeBlock(std::size_t size,
                                                   std::size_t alignment,
                                                   intptr_t context,
                                                   FreeBlockSet::iterator begin,
                                                   FreeBlockSet::iterator end,
                                                   bool* allocate_from_front) {
    SB_UNREFERENCED_PARAMETER(context);
    return FindFreeBlock(size, alignment, begin, end, allocate_from_front);
  }

 private:
  // Map from pointers we returned to the user, back to memory blocks.
  typedef std::map<void*, MemoryBlock> AllocatedBlockMap;

  FreeBlockSet::iterator ExpandToFit(std::size_t size, std::size_t alignment);

  void AddAllocatedBlock(void* address, const MemoryBlock& block);
  FreeBlockSet::iterator AddFreeBlock(MemoryBlock block_to_add);
  void RemoveFreeBlock(FreeBlockSet::iterator it);

  AllocatedBlockMap allocated_blocks_;
  FreeBlockSet free_blocks_;

  // We will allocate from the given allocator whenever we can't find pre-used
  // memory to allocate.
  Allocator* fallback_allocator_;
  std::size_t allocation_increment_;

  // If non-zero, this is an upper bound on how large we will let the capacity
  // expand.
  std::size_t max_capacity_;

  // A list of allocations made from the fallback allocator.  We keep track of
  // this so that we can free them all upon our destruction.
  std::vector<void*> fallback_allocations_;

  // How much we have allocated from the fallback allocator.
  std::size_t capacity_;

  // How much has been allocated from us.
  std::size_t total_allocated_;
};

}  // namespace nb

#endif  // NB_REUSE_ALLOCATOR_BASE_H_

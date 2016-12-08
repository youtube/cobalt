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

#include "nb/reuse_allocator.h"

#include <algorithm>

#include "nb/pointer_arithmetic.h"
#include "starboard/log.h"
#include "starboard/types.h"

namespace nb {

// Minimum block size to avoid extremely small blocks inside the block list and
// to ensure that a zero sized allocation will return a non-zero sized block.
const std::size_t kMinBlockSizeBytes = 16;

bool ReuseAllocator::MemoryBlock::Merge(const MemoryBlock& other) {
  if (AsInteger(address_) + size_ == AsInteger(other.address_)) {
    size_ += other.size_;
    return true;
  }
  if (AsInteger(other.address_) + other.size_ == AsInteger(address_)) {
    address_ = other.address_;
    size_ += other.size_;
    return true;
  }
  return false;
}

bool ReuseAllocator::MemoryBlock::CanFullfill(std::size_t request_size,
                                              std::size_t alignment) const {
  const std::size_t extra_bytes_for_alignment =
      AlignUp(AsInteger(address_), alignment) - AsInteger(address_);
  const std::size_t aligned_size = request_size + extra_bytes_for_alignment;
  return size_ >= aligned_size;
}

void ReuseAllocator::MemoryBlock::Allocate(std::size_t request_size,
                                           std::size_t alignment,
                                           bool allocate_from_front,
                                           MemoryBlock* allocated,
                                           MemoryBlock* free) const {
  SB_DCHECK(allocated);
  SB_DCHECK(free);
  SB_DCHECK(CanFullfill(request_size, alignment));

  // First we assume that the block is just enough to fulfill the allocation and
  // leaves no free block.
  allocated->address_ = address_;
  allocated->size_ = size_;
  free->address_ = NULL;
  free->size_ = 0;

  if (allocate_from_front) {
    // |address_|
    //     |     <- allocated_size ->     | <- remaining_size -> |
    //     -------------------------------------------------------
    //          |   <-  request_size ->   |                      |
    //  |aligned_address|       |end_of_allocation|      |address_ + size_|
    std::size_t aligned_address = AlignUp(AsInteger(address_), alignment);
    std::size_t end_of_allocation = aligned_address + request_size;
    std::size_t allocated_size = end_of_allocation - AsInteger(address_);
    std::size_t remaining_size = size_ - allocated_size;
    if (remaining_size < kMinBlockSizeBytes) {
      return;
    }
    allocated->size_ = allocated_size;
    free->address_ = AsPointer(end_of_allocation);
    free->size_ = remaining_size;
  } else {
    // |address_|
    //     |   <- remaining_size ->  |   <- allocated_size ->    |
    //     -------------------------------------------------------
    //                               |  <-  request_size -> |    |
    //                       |aligned_address|           |address_ + size_|
    std::size_t aligned_address =
        AlignDown(AsInteger(address_) + size_ - request_size, alignment);
    std::size_t allocated_size = AsInteger(address_) + size_ - aligned_address;
    std::size_t remaining_size = size_ - allocated_size;
    if (remaining_size < kMinBlockSizeBytes) {
      return;
    }
    allocated->address_ = AsPointer(aligned_address);
    allocated->size_ = allocated_size;
    free->address_ = address_;
    free->size_ = remaining_size;
  }
}

ReuseAllocator::ReuseAllocator(Allocator* fallback_allocator)
    : fallback_allocator_(fallback_allocator),
      small_allocation_threshold_(0),
      capacity_(0),
      total_allocated_(0) {}

ReuseAllocator::ReuseAllocator(Allocator* fallback_allocator,
                               std::size_t capacity,
                               std::size_t small_allocation_threshold)
    : fallback_allocator_(fallback_allocator),
      small_allocation_threshold_(small_allocation_threshold),
      capacity_(0),
      total_allocated_(0) {
  // If |small_allocation_threshold_| is non-zero, this class will use last-fit
  // strategy to fulfill small allocations.  Pre-allocator full capacity so
  // last-fit makes sense.
  if (small_allocation_threshold_ > 0) {
    void* p = Allocate(capacity, 1);
    SB_DCHECK(p);
    Free(p);
  }
}

ReuseAllocator::~ReuseAllocator() {
  // Assert that everything was freed.
  // Note that in some unit tests this may
  // not be the case.
  if (allocated_blocks_.size() != 0) {
    SB_DLOG(ERROR) << allocated_blocks_.size() << " blocks still allocated.";
  }

  for (std::vector<void*>::iterator iter = fallback_allocations_.begin();
       iter != fallback_allocations_.end(); ++iter) {
    fallback_allocator_->Free(*iter);
  }
}

ReuseAllocator::FreeBlockSet::iterator ReuseAllocator::AddFreeBlock(
    MemoryBlock block_to_add) {
  if (free_blocks_.size() == 0) {
    return free_blocks_.insert(block_to_add).first;
  }

  // See if we can merge this block with one on the right or left.
  FreeBlockSet::iterator it = free_blocks_.lower_bound(block_to_add);
  // lower_bound will return an iterator to our neighbor on the right,
  // if one exists.
  FreeBlockSet::iterator right_to_erase = free_blocks_.end();
  FreeBlockSet::iterator left_to_erase = free_blocks_.end();

  if (it != free_blocks_.end()) {
    MemoryBlock right_block = *it;
    if (block_to_add.Merge(right_block)) {
      right_to_erase = it;
    }
  }

  // Now look to our left.
  if (it != free_blocks_.begin()) {
    it--;
    MemoryBlock left_block = *it;
    // Are we contiguous with the block to our left?
    if (block_to_add.Merge(left_block)) {
      left_to_erase = it;
    }
  }

  if (right_to_erase != free_blocks_.end()) {
    free_blocks_.erase(right_to_erase);
  }
  if (left_to_erase != free_blocks_.end()) {
    free_blocks_.erase(left_to_erase);
  }

  return free_blocks_.insert(block_to_add).first;
}

void ReuseAllocator::RemoveFreeBlock(FreeBlockSet::iterator it) {
  free_blocks_.erase(it);
}

void* ReuseAllocator::Allocate(std::size_t size) {
  return Allocate(size, 1);
}

void* ReuseAllocator::Allocate(std::size_t size, std::size_t alignment) {
  if (alignment == 0) {
    alignment = 1;
  }

  // Try to satisfy request from free list.
  // First look for a block that is appropriately aligned.
  // If we can't, look for a block that is big enough that we can carve out an
  // aligned block.
  // If there is no such block, allocate more from our fallback allocator.
  void* user_address = 0;

  // Keeping things rounded and aligned will help us avoid creating tiny and/or
  // badly misaligned free blocks.  Also ensure even for a 0-byte request we
  // return a unique block.
  const std::size_t kMinAlignment = 16;
  size = std::max(size, kMinBlockSizeBytes);
  size = AlignUp(size, kMinBlockSizeBytes);
  alignment = AlignUp(alignment, kMinAlignment);

  // Worst case how much memory we need.
  MemoryBlock allocated_block;

  bool scan_from_front = size > small_allocation_threshold_;
  FreeBlockSet::iterator free_block_iter = free_blocks_.end();

  if (scan_from_front) {
    // Start looking through the free list from the front.
    for (FreeBlockSet::iterator it = free_blocks_.begin();
         it != free_blocks_.end(); ++it) {
      if (it->CanFullfill(size, alignment)) {
        free_block_iter = it;
        break;
      }
    }
  } else {
    // Start looking through the free list from the back.
    for (FreeBlockSet::reverse_iterator it = free_blocks_.rbegin();
         it != free_blocks_.rend(); ++it) {
      if (it->CanFullfill(size, alignment)) {
        free_block_iter = it.base();
        --free_block_iter;
        break;
      }
    }
  }

  if (free_block_iter == free_blocks_.end()) {
    // No free blocks found, allocate one from the fallback allocator.
    std::size_t block_size = size;
    void* ptr =
        fallback_allocator_->AllocateForAlignment(&block_size, alignment);
    if (ptr == NULL) {
      return NULL;
    }
    free_block_iter = AddFreeBlock(MemoryBlock(ptr, block_size));
    capacity_ += block_size;
    fallback_allocations_.push_back(ptr);
  }

  MemoryBlock block = *free_block_iter;
  // The block is big enough.  We may waste some space due to alignment.
  RemoveFreeBlock(free_block_iter);

  MemoryBlock free_block;
  block.Allocate(size, alignment, scan_from_front, &allocated_block,
                 &free_block);
  if (free_block.size() > 0) {
    SB_DCHECK(free_block.address());
    AddFreeBlock(free_block);
  }
  user_address = AlignUp(allocated_block.address(), alignment);

  SB_DCHECK(allocated_blocks_.find(user_address) == allocated_blocks_.end());
  allocated_blocks_[user_address] = allocated_block;
  total_allocated_ += allocated_block.size();
  return user_address;
}

void ReuseAllocator::Free(void* memory) {
  bool result = TryFree(memory);
  SB_DCHECK(result);
}

void ReuseAllocator::PrintAllocations() const {
  typedef std::map<std::size_t, std::size_t> SizesHistogram;
  SizesHistogram sizes_histogram;
  for (AllocatedBlockMap::const_iterator iter = allocated_blocks_.begin();
       iter != allocated_blocks_.end(); ++iter) {
    std::size_t block_size = iter->second.size();
    if (sizes_histogram.find(block_size) == sizes_histogram.end()) {
      sizes_histogram[block_size] = 0;
    }
    sizes_histogram[block_size] = sizes_histogram[block_size] + 1;
  }

  for (SizesHistogram::const_iterator iter = sizes_histogram.begin();
       iter != sizes_histogram.end(); ++iter) {
    SB_LOG(INFO) << iter->first << " : " << iter->second;
  }
  SB_LOG(INFO) << "Total allocations: " << allocated_blocks_.size();
}

bool ReuseAllocator::TryFree(void* memory) {
  if (!memory) {
    return true;
  }

  AllocatedBlockMap::iterator it = allocated_blocks_.find(memory);
  if (it == allocated_blocks_.end()) {
    return false;
  }

  // Mark this block as free and remove it from the allocated set.
  const MemoryBlock& block = (*it).second;
  AddFreeBlock(block);

  SB_DCHECK(block.size() <= total_allocated_);
  total_allocated_ -= block.size();

  allocated_blocks_.erase(it);
  return true;
}

}  // namespace nb

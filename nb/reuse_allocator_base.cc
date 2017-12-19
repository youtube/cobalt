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

#include "nb/reuse_allocator_base.h"

#include <algorithm>

#include "nb/pointer_arithmetic.h"
#include "starboard/log.h"
#include "starboard/types.h"

namespace nb {

namespace {

// Minimum block size to avoid extremely small blocks inside the block list and
// to ensure that a zero sized allocation will return a non-zero sized block.
const std::size_t kMinBlockSizeBytes = 16;
// Using a minimum value for size and alignment keeps things rounded and aligned
// and help us avoid creating tiny and/or badly misaligned free blocks.  Also
// ensures even for a 0-byte request will get a unique block.
const std::size_t kMinAlignment = 16;
// The max lines of allocation to print inside PrintAllocations().  Set to 0 to
// print all allocations.
const int kMaxAllocationLinesToPrint = 0;

}  // namespace

bool ReuseAllocatorBase::MemoryBlock::Merge(const MemoryBlock& other) {
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

bool ReuseAllocatorBase::MemoryBlock::CanFullfill(std::size_t request_size,
                                                  std::size_t alignment) const {
  const std::size_t extra_bytes_for_alignment =
      AlignUp(AsInteger(address_), alignment) - AsInteger(address_);
  const std::size_t aligned_size = request_size + extra_bytes_for_alignment;
  return size_ >= aligned_size;
}

void ReuseAllocatorBase::MemoryBlock::Allocate(std::size_t request_size,
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

void* ReuseAllocatorBase::Allocate(std::size_t size) {
  return Allocate(size, 1);
}

void* ReuseAllocatorBase::Allocate(std::size_t size, std::size_t alignment) {
  size = AlignUp(std::max(size, kMinAlignment), kMinAlignment);
  alignment = AlignUp(std::max<std::size_t>(alignment, 1), kMinAlignment);

  bool allocate_from_front;
  FreeBlockSet::iterator free_block_iter =
      FindFreeBlock(size, alignment, free_blocks_.begin(), free_blocks_.end(),
                    &allocate_from_front);

  if (free_block_iter == free_blocks_.end()) {
    if (CapacityExceeded()) {
      return NULL;
    }
    free_block_iter = ExpandToFit(size, alignment);
    if (free_block_iter == free_blocks_.end()) {
      return NULL;
    }
  }

  MemoryBlock block = *free_block_iter;
  // The block is big enough.  We may waste some space due to alignment.
  RemoveFreeBlock(free_block_iter);

  MemoryBlock allocated_block;
  MemoryBlock free_block;
  block.Allocate(size, alignment, allocate_from_front, &allocated_block,
                 &free_block);
  if (free_block.size() > 0) {
    SB_DCHECK(free_block.address());
    AddFreeBlock(free_block);
  }
  void* user_address = AlignUp(allocated_block.address(), alignment);
  AddAllocatedBlock(user_address, allocated_block);

  return user_address;
}

void ReuseAllocatorBase::Free(void* memory) {
  bool result = TryFree(memory);
  SB_DCHECK(result);
}

void ReuseAllocatorBase::PrintAllocations() const {
  typedef std::map<std::size_t, std::size_t> SizesHistogram;
  SizesHistogram sizes_histogram;

  for (auto iter = allocated_blocks_.begin(); iter != allocated_blocks_.end();
       ++iter) {
    std::size_t block_size = iter->second.size();
    if (sizes_histogram.find(block_size) == sizes_histogram.end()) {
      sizes_histogram[block_size] = 0;
    }
    sizes_histogram[block_size] = sizes_histogram[block_size] + 1;
  }

  SB_LOG(INFO) << "Total allocation: " << total_allocated_ << " bytes in "
               << allocated_blocks_.size() << " blocks";

  int lines = 0;
  std::size_t accumulated_blocks = 0;
  for (SizesHistogram::const_iterator iter = sizes_histogram.begin();
       iter != sizes_histogram.end(); ++iter) {
    if (lines == kMaxAllocationLinesToPrint - 1 &&
        sizes_histogram.size() > kMaxAllocationLinesToPrint) {
      SB_LOG(INFO) << "\t" << iter->first << ".."
                   << sizes_histogram.rbegin()->first << " : "
                   << allocated_blocks_.size() - accumulated_blocks;
      break;
    }
    SB_LOG(INFO) << "\t" << iter->first << " : " << iter->second;
    ++lines;
    accumulated_blocks += iter->second;
  }

  SB_LOG(INFO) << "Total free blocks: " << free_blocks_.size();
  sizes_histogram.clear();
  for (auto iter = free_blocks_.begin(); iter != free_blocks_.end(); ++iter) {
    if (sizes_histogram.find(iter->size()) == sizes_histogram.end()) {
      sizes_histogram[iter->size()] = 0;
    }
    sizes_histogram[iter->size()] = sizes_histogram[iter->size()] + 1;
  }

  lines = 0;
  accumulated_blocks = 0;
  for (SizesHistogram::const_iterator iter = sizes_histogram.begin();
       iter != sizes_histogram.end(); ++iter) {
    if (lines == kMaxAllocationLinesToPrint - 1 &&
        sizes_histogram.size() > kMaxAllocationLinesToPrint) {
      SB_LOG(INFO) << "\t" << iter->first << ".."
                   << sizes_histogram.rbegin()->first << " : "
                   << allocated_blocks_.size() - accumulated_blocks;
      break;
    }
    SB_LOG(INFO) << "\t" << iter->first << " : " << iter->second;
    ++lines;
    accumulated_blocks += iter->second;
  }
}

bool ReuseAllocatorBase::TryFree(void* memory) {
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

void* ReuseAllocatorBase::AllocateBestBlock(std::size_t alignment,
                                            intptr_t context,
                                            std::size_t* size_hint) {
  const std::size_t kMinAlignment = 16;
  std::size_t size =
      AlignUp(std::max(*size_hint, kMinAlignment), kMinAlignment);
  alignment = AlignUp(std::max<std::size_t>(alignment, 1), kMinAlignment);

  bool allocate_from_front;
  FreeBlockSet::iterator free_block_iter =
      FindBestFreeBlock(size, alignment, context, free_blocks_.begin(),
                        free_blocks_.end(), &allocate_from_front);

  if (free_block_iter == free_blocks_.end()) {
    if (CapacityExceeded()) {
      return NULL;
    }
    free_block_iter = ExpandToFit(*size_hint, alignment);
    if (free_block_iter == free_blocks_.end()) {
      return NULL;
    }
  }

  MemoryBlock block = *free_block_iter;
  // The block is big enough.  We may waste some space due to alignment.
  RemoveFreeBlock(free_block_iter);

  MemoryBlock allocated_block;
  void* user_address;

  if (block.CanFullfill(size, alignment)) {
    MemoryBlock free_block;
    block.Allocate(size, alignment, allocate_from_front, &allocated_block,
                   &free_block);
    if (free_block.size() > 0) {
      SB_DCHECK(free_block.address());
      AddFreeBlock(free_block);
    }
    user_address = AlignUp(allocated_block.address(), alignment);
  } else {
    allocated_block = block;
    user_address = AlignUp(allocated_block.address(), alignment);
  }
  SB_DCHECK(AsInteger(user_address) >= AsInteger(allocated_block.address()));
  uintptr_t offset =
      AsInteger(user_address) - AsInteger(allocated_block.address());
  SB_DCHECK(allocated_block.size() >= offset);
  if (allocated_block.size() - offset < *size_hint) {
    *size_hint = allocated_block.size() - offset;
  }

  AddAllocatedBlock(user_address, allocated_block);

  return user_address;
}

ReuseAllocatorBase::ReuseAllocatorBase(Allocator* fallback_allocator,
                                       std::size_t initial_capacity,
                                       std::size_t allocation_increment,
                                       std::size_t max_capacity)
    : fallback_allocator_(fallback_allocator),
      allocation_increment_(allocation_increment),
      max_capacity_(max_capacity),
      capacity_(0),
      total_allocated_(0) {
  if (initial_capacity > 0) {
    FreeBlockSet::iterator iter = ExpandToFit(initial_capacity, 1);
    SB_DCHECK(iter != free_blocks_.end());
  }
}

ReuseAllocatorBase::~ReuseAllocatorBase() {
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

ReuseAllocatorBase::FreeBlockSet::iterator ReuseAllocatorBase::ExpandToFit(
    std::size_t size,
    std::size_t alignment) {
  void* ptr = NULL;
  std::size_t size_to_try = 0;
  if (allocation_increment_ > size) {
    size_to_try = std::max(size, allocation_increment_);
    ptr = fallback_allocator_->AllocateForAlignment(&size_to_try, alignment);
  }
  if (ptr == NULL) {
    size_to_try = size;
    ptr = fallback_allocator_->AllocateForAlignment(&size_to_try, alignment);
  }
  if (ptr != NULL) {
    fallback_allocations_.push_back(ptr);
    capacity_ += size_to_try;
    return AddFreeBlock(MemoryBlock(ptr, size_to_try));
  }

  if (free_blocks_.empty()) {
    return free_blocks_.end();
  }

  // We failed to allocate for |size| from the fallback allocator, try to
  // allocate the difference between |size| and the size of the right most block
  // in the hope that they are continuous and can be connect to a block that is
  // large enough to fulfill |size|.
  size_t size_difference = size - free_blocks_.rbegin()->size();
  ptr = fallback_allocator_->AllocateForAlignment(&size_difference, alignment);
  if (ptr == NULL) {
    return free_blocks_.end();
  }

  fallback_allocations_.push_back(ptr);
  capacity_ += size_difference;
  AddFreeBlock(MemoryBlock(ptr, size_difference));
  FreeBlockSet::iterator iter = free_blocks_.end();
  --iter;
  return iter->CanFullfill(size, alignment) ? iter : free_blocks_.end();
}

void ReuseAllocatorBase::AddAllocatedBlock(void* address,
                                           const MemoryBlock& block) {
  SB_DCHECK(allocated_blocks_.find(address) == allocated_blocks_.end());
  allocated_blocks_[address] = block;
  total_allocated_ += block.size();
}

ReuseAllocatorBase::FreeBlockSet::iterator ReuseAllocatorBase::AddFreeBlock(
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

void ReuseAllocatorBase::RemoveFreeBlock(FreeBlockSet::iterator it) {
  free_blocks_.erase(it);
}

}  // namespace nb

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

namespace nb {

ReuseAllocator::ReuseAllocator(Allocator* fallback_allocator) {
  fallback_allocator_ = fallback_allocator;
  capacity_ = 0;
  total_allocated_ = 0;
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

void ReuseAllocator::AddFreeBlock(uintptr_t address, size_t size) {
  MemoryBlock new_block;
  new_block.address = address;
  new_block.size = size;

  if (free_blocks_.size() == 0) {
    free_blocks_.insert(new_block);
    return;
  }

  // See if we can merge this block with one on the right or left.
  FreeBlockSet::iterator it = free_blocks_.lower_bound(new_block);
  // lower_bound will return an iterator to our neighbor on the right,
  // if one exists.
  FreeBlockSet::iterator right_to_erase = free_blocks_.end();
  FreeBlockSet::iterator left_to_erase = free_blocks_.end();

  if (it != free_blocks_.end()) {
    MemoryBlock right_block = *it;
    if (new_block.address + new_block.size == right_block.address) {
      new_block.size += right_block.size;
      right_to_erase = it;
    }
  }

  // Now look to our left.
  if (it != free_blocks_.begin()) {
    it--;
    MemoryBlock left_block = *it;
    // Are we contiguous with the block to our left?
    if (left_block.address + left_block.size == new_block.address) {
      new_block.address = left_block.address;
      new_block.size += left_block.size;
      left_to_erase = it;
    }
  }

  if (right_to_erase != free_blocks_.end()) {
    free_blocks_.erase(right_to_erase);
  }
  if (left_to_erase != free_blocks_.end()) {
    free_blocks_.erase(left_to_erase);
  }

  free_blocks_.insert(new_block);
}

void ReuseAllocator::RemoveFreeBlock(FreeBlockSet::iterator it) {
  free_blocks_.erase(it);
}

void* ReuseAllocator::Allocate(size_t size) {
  return Allocate(size, 0);
}

void* ReuseAllocator::AllocateForAlignment(size_t size, size_t alignment) {
  return Allocate(size, alignment);
}

void* ReuseAllocator::Allocate(size_t size, size_t alignment) {
  // Try to satisfy request from free list.
  // First look for a block that is appropriately aligned.
  // If we can't, look for a block that is big enough that we can
  // carve out an aligned block.
  // If there is no such block, allocate more from our fallback allocator.
  uintptr_t user_address = 0;

  // Keeping things rounded and aligned will help us
  // avoid creating tiny and/or badly misaligned free blocks.
  // Also ensure even for a 0-byte request we return a unique block.
  const size_t kMinBlockSizeBytes = 16;
  const size_t kMinAlignment = 16;
  size = std::max(size, kMinBlockSizeBytes);
  size = AlignUp(size, kMinBlockSizeBytes);
  alignment = AlignUp(alignment, kMinAlignment);

  // Worst case how much memory we need.
  const size_t required_aligned_size = size + alignment;
  MemoryBlock allocated_block;

  // Start looking through the free list.
  // If this is slow, we can store another map sorted by size.
  for (FreeBlockSet::iterator it = free_blocks_.begin();
       it != free_blocks_.end(); ++it) {
    MemoryBlock block = *it;
    if (block.size >= size) {
      // Promising. Might be big enough.
      if (IsAligned(block.address, alignment)) {
        // Perfect. The block is big enough and aligned.
        RemoveFreeBlock(it);
        const size_t remaining_bytes = block.size - size;
        if (remaining_bytes >= kMinBlockSizeBytes) {
          // Insert a new free block with the leftover space.
          AddFreeBlock(block.address + size, remaining_bytes);
          allocated_block.size = size;
        } else {
          allocated_block.size = block.size;
        }
        user_address = block.address;
        allocated_block.address = block.address;
        SB_DCHECK(allocated_block.size <= block.size);
        break;
      } else if (block.size >= required_aligned_size) {
        // The block isn't aligned fully, but it's big enough.
        // We'll waste some space due to alignment.
        RemoveFreeBlock(it);
        const size_t remaining_bytes = block.size - required_aligned_size;
        if (remaining_bytes >= kMinBlockSizeBytes) {
          AddFreeBlock(block.address + required_aligned_size, remaining_bytes);
          allocated_block.size = required_aligned_size;
        } else {
          allocated_block.size = block.size;
        }
        user_address = AlignUp(block.address, alignment);
        allocated_block.address = block.address;
        SB_DCHECK(allocated_block.size <= block.size);
        break;
      }
    }
  }

  if (user_address == 0) {
    // No free blocks found.
    // Allocate one from the fallback allocator.
    size = AlignUp(size, alignment);
    void* ptr = fallback_allocator_->AllocateForAlignment(size, alignment);
    if (ptr == NULL) {
      return NULL;
    }
    uintptr_t memory_address = AsInteger(ptr);
    user_address = AlignUp(memory_address, alignment);
    allocated_block.size = size;
    allocated_block.address = user_address;

    if (memory_address != user_address) {
      size_t alignment_padding_size = user_address - memory_address;
      if (alignment_padding_size >= kMinBlockSizeBytes) {
        // Register the memory range skipped for alignment as a free block for
        // later use.
        AddFreeBlock(memory_address, alignment_padding_size);
        capacity_ += alignment_padding_size;
      } else {
        // The memory range skipped for alignment is too small for a free block.
        // Adjust the allocated block to include the alignment padding.
        allocated_block.size += alignment_padding_size;
        allocated_block.address -= alignment_padding_size;
      }
    }

    capacity_ += allocated_block.size;
    fallback_allocations_.push_back(ptr);
  }
  SB_DCHECK(allocated_blocks_.find(user_address) == allocated_blocks_.end());
  allocated_blocks_[user_address] = allocated_block;
  total_allocated_ += allocated_block.size;
  return AsPointer(user_address);
}

void ReuseAllocator::Free(void* memory) {
  if (!memory) {
    return;
  }

  uintptr_t mem_as_int = AsInteger(memory);
  AllocatedBlockMap::iterator it = allocated_blocks_.find(mem_as_int);
  SB_DCHECK(it != allocated_blocks_.end());

  // Mark this block as free and remove it from the allocated set.
  const MemoryBlock& block = (*it).second;
  AddFreeBlock(block.address, block.size);

  SB_DCHECK(block.size <= total_allocated_);
  total_allocated_ -= block.size;

  allocated_blocks_.erase(it);
}

void ReuseAllocator::PrintAllocations() const {
  typedef std::map<size_t, size_t> SizesHistogram;
  SizesHistogram sizes_histogram;
  for (AllocatedBlockMap::const_iterator iter = allocated_blocks_.begin();
       iter != allocated_blocks_.end(); ++iter) {
    size_t block_size = iter->second.size;
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

}  // namespace nb

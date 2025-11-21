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

#include "starboard/common/in_place_reuse_allocator_base.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "starboard/common/check_op.h"
#include "starboard/common/pointer_arithmetic.h"

namespace starboard {

namespace {

int ceil_power_2(int i) {
  SB_DCHECK_GE(i, 0);

  for (size_t power = 0; power < sizeof(i) * 8 - 1; ++power) {
    if ((1 << power) >= i) {
      return 1 << power;
    }
  }

  SB_NOTREACHED();
  return -1;
}

}  // namespace

bool InPlaceReuseAllocatorBase::MemoryBlock::Merge(const MemoryBlock& other) {
  SB_DCHECK_GE(fallback_allocation_index_, 0);
  SB_DCHECK_GE(other.fallback_allocation_index_, 0);

  if (AsInteger(address_) + size_ == AsInteger(other.address_)) {
    SB_DCHECK_LE(fallback_allocation_index_, other.fallback_allocation_index_);

    size_ += other.size_;
    return true;
  }
  if (AsInteger(other.address_) + other.size_ == AsInteger(address_)) {
    SB_DCHECK_GE(fallback_allocation_index_, other.fallback_allocation_index_);

    fallback_allocation_index_ = other.fallback_allocation_index_;
    address_ = other.address_;
    size_ += other.size_;
    return true;
  }
  return false;
}

bool InPlaceReuseAllocatorBase::MemoryBlock::CanFulfill(
    size_t request_size,
    size_t alignment) const {
  SB_DCHECK_GE(fallback_allocation_index_, 0);

  const size_t extra_bytes_for_alignment =
      AlignUp(AsInteger(address_), alignment) - AsInteger(address_);
  const size_t aligned_size = request_size + extra_bytes_for_alignment;
  return size_ >= aligned_size;
}

void InPlaceReuseAllocatorBase::MemoryBlock::Allocate(size_t request_size,
                                                      size_t alignment,
                                                      bool allocate_from_front,
                                                      MemoryBlock* allocated,
                                                      MemoryBlock* free) const {
  SB_DCHECK_GE(fallback_allocation_index_, 0);
  SB_DCHECK(allocated);
  SB_DCHECK(free);
  SB_DCHECK(CanFulfill(request_size, alignment));

  // Minimum block size to avoid extremely small blocks inside the block list
  // and to ensure that a zero sized allocation will return a non-zero sized
  // block.
  // TODO: b/399430536 - Cobalt: Tune this for a better value.
  const size_t kMinBlockSizeBytes = 16 + sizeof(BlockMetadata);

  // First we assume that the block is just enough to fulfill the allocation and
  // leaves no free block.
  allocated->fallback_allocation_index_ = fallback_allocation_index_;
  allocated->address_ = address_;
  allocated->size_ = size_;
  free->fallback_allocation_index_ = fallback_allocation_index_;
  free->address_ = NULL;
  free->size_ = 0;

  if (allocate_from_front) {
    // |address_|
    //     |     <- allocated_size ->     | <- remaining_size -> |
    //     -------------------------------------------------------
    //          |   <-  request_size ->   |                      |
    //  |aligned_address|       |end_of_allocation|      |address_ + size_|
    size_t aligned_address = AlignUp(AsInteger(address_), alignment);
    size_t end_of_allocation = aligned_address + request_size;
    size_t allocated_size = end_of_allocation - AsInteger(address_);
    size_t remaining_size = size_ - allocated_size;
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
    size_t aligned_address =
        AlignDown(AsInteger(address_) + size_ - request_size, alignment);
    size_t allocated_size = AsInteger(address_) + size_ - aligned_address;
    size_t remaining_size = size_ - allocated_size;
    if (remaining_size < kMinBlockSizeBytes) {
      return;
    }
    allocated->address_ = AsPointer(aligned_address);
    allocated->size_ = allocated_size;
    free->address_ = address_;
    free->size_ = remaining_size;
  }
}

void* InPlaceReuseAllocatorBase::Allocate(size_t size) {
  return Allocate(size, 1);
}

void* InPlaceReuseAllocatorBase::Allocate(size_t size, size_t alignment) {
  SB_DCHECK_EQ(sizeof(BlockMetadata) % alignment, 0U);

  if (!pending_frees_.empty()) {
    for (auto address_to_free : pending_frees_) {
      bool freed = TryFree(address_to_free);
      SB_DCHECK(freed);
    }
    pending_frees_.clear();
  }

  size = AlignUp(std::max(size, kMinAlignment), kMinAlignment) +
         sizeof(BlockMetadata);
  alignment = AlignUp(std::max<size_t>(alignment, 1), kMinAlignment);

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

  SB_DCHECK_EQ(
      reinterpret_cast<intptr_t>(allocated_block.address()) % alignment, 0U);
  SB_DCHECK_EQ(sizeof(BlockMetadata) % alignment, 0U);

  void* user_address =
      static_cast<uint8_t*>(allocated_block.address()) + sizeof(BlockMetadata);

  AddAllocatedBlock(allocated_block);

  return user_address;
}

void InPlaceReuseAllocatorBase::Free(void* memory) {
  if (memory == nullptr) {
    return;
  }

  pending_frees_.push_back(memory);

  if (pending_frees_.size() == total_allocated_blocks_) {
    if (total_allocated_blocks_ > 32) {
      SB_LOG_IF(INFO, total_allocated_blocks_ > 2048)
          << "Batched free triggered for " << total_allocated_blocks_
          << " blocks.";
      allocated_block_head_ = nullptr;
      free_blocks_.clear();
      pending_frees_.clear();

      total_allocated_in_bytes_ = 0;
      total_allocated_blocks_ = 0;

      for (int i = 0; i < static_cast<int>(fallback_allocations_.size()); ++i) {
        AddFreeBlock(MemoryBlock(i, fallback_allocations_[i].address,
                                 fallback_allocations_[i].size));
      }
    } else {
      for (auto address_to_free : pending_frees_) {
        bool freed = TryFree(address_to_free);
        SB_DCHECK(freed);
      }
      pending_frees_.clear();
    }
  }
}

void InPlaceReuseAllocatorBase::PrintAllocations(
    bool align_allocated_size,
    int max_allocations_to_print) const {
  struct HistogramEntry {
    int count = 0;
    size_t min = std::numeric_limits<size_t>::max();
    size_t max = 0;
    size_t total = 0;
  };
  typedef std::map<size_t, HistogramEntry, std::greater<size_t>>
      AllocatedHistogram;
  AllocatedHistogram allocated_histogram;

  max_allocations_to_print = std::max(max_allocations_to_print, 1);

  // Logging the allocated blocks
  BlockMetadata* head = allocated_block_head_;

  while (head != nullptr) {
    size_t size = head->size;
    size_t size_as_key = align_allocated_size ? ceil_power_2(size) : size;
    HistogramEntry& entry = allocated_histogram[size_as_key];

    ++entry.count;
    entry.min = std::min(entry.min, size);
    entry.max = std::max(entry.max, size);
    entry.total += size;

    head = head->next;
  }

  int64_t allocated_percentage =
      capacity_in_bytes_ == 0
          ? 0
          : static_cast<int64_t>(total_allocated_in_bytes_) * 100 /
                capacity_in_bytes_;
  SB_LOG(INFO) << "Allocated " << total_allocated_in_bytes_ << " bytes ("
               << allocated_percentage << "%) from a pool of capacity "
               << capacity_in_bytes_ << " bytes.  There are "
               << capacity_in_bytes_ - total_allocated_in_bytes_
               << " free bytes.";
  SB_LOG(INFO) << "Total allocated block: " << total_allocated_blocks_;

  int lines = 0;
  size_t accumulated_blocks = 0;

  for (auto&& iter : allocated_histogram) {
    if (lines == max_allocations_to_print - 1 &&
        static_cast<int>(allocated_histogram.size()) >
            max_allocations_to_print) {
      SB_LOG(INFO) << "\t[" << allocated_histogram.rbegin()->second.min << ", "
                   << iter.second.max
                   << "] : " << total_allocated_blocks_ - accumulated_blocks;
      break;
    }

    if (iter.second.count == 1) {
      SB_LOG(INFO) << "\t" << iter.second.total << " : 1";
    } else {
      SB_LOG(INFO) << "\t[" << iter.second.min << ", " << iter.second.max
                   << "] : " << iter.second.count
                   << " (average: " << iter.second.total / iter.second.count
                   << ")";
    }
    ++lines;
    accumulated_blocks += iter.second.count;
  }

  // Logging the free blocks
  typedef std::map<size_t, int, std::greater<size_t>> FreeHistogram;
  FreeHistogram free_histogram;

  SB_LOG(INFO) << "Total free blocks: " << free_blocks_.size();

  for (auto&& block : free_blocks_) {
    ++free_histogram[block.size()];
  }

  lines = 0;
  accumulated_blocks = 0;

  for (auto&& iter : free_histogram) {
    if (lines == max_allocations_to_print - 1 &&
        static_cast<int>(free_histogram.size()) > max_allocations_to_print) {
      SB_LOG(INFO) << "\t[" << free_histogram.rbegin()->first << ", "
                   << iter.first
                   << "] : " << free_blocks_.size() - accumulated_blocks;
      break;
    }

    SB_LOG(INFO) << "\t" << iter.first << " : " << iter.second;
    ++lines;
    accumulated_blocks += iter.second;
  }
}

bool InPlaceReuseAllocatorBase::TryFree(void* memory) {
  if (!memory) {
    return true;
  }

  BlockMetadata* metadata = static_cast<BlockMetadata*>(memory) - 1;

  if (metadata->signature != this || metadata->fallback_index < 0 ||
      static_cast<size_t>(metadata->fallback_index) >=
          fallback_allocations_.size()) {
    return false;
  }

  // Detach the node from the linked list
  if (metadata->previous != nullptr) {
    SB_DCHECK_EQ(metadata->previous->next, metadata);
    metadata->previous->next = metadata->next;
  }
  if (metadata->next != nullptr) {
    SB_DCHECK_EQ(metadata->next->previous, metadata);
    metadata->next->previous = metadata->previous;
  }

  // Update the head, if the node is the head.
  if (metadata->previous == nullptr) {
    SB_DCHECK_EQ(metadata, allocated_block_head_);

    allocated_block_head_ = metadata->next;

    if (metadata->next) {
      // It should have been updated during detaching above.
      SB_DCHECK_EQ(metadata->next->previous, nullptr);
    }
  }

  // Mark this block as free and update the allocated data accordingly.
  MemoryBlock block(metadata->fallback_index, metadata, metadata->size);
  AddFreeBlock(block);

  SB_DCHECK_LE(block.size(), total_allocated_in_bytes_);
  total_allocated_in_bytes_ -= block.size();
  --total_allocated_blocks_;

  return true;
}

InPlaceReuseAllocatorBase::InPlaceReuseAllocatorBase(
    Allocator* fallback_allocator,
    size_t initial_capacity,
    size_t allocation_increment,
    size_t max_capacity)
    : fallback_allocator_(fallback_allocator),
      allocation_increment_(allocation_increment),
      max_capacity_in_bytes_(max_capacity) {
  if (initial_capacity > 0) {
    FreeBlockSet::iterator iter = ExpandToFit(initial_capacity, kMinAlignment);
    SB_DCHECK(iter != free_blocks_.end());
  }

  pending_frees_.reserve(16 * 1024);
}

InPlaceReuseAllocatorBase::~InPlaceReuseAllocatorBase() {
  if (ExtraLogLevel() >= 1) {
    SB_LOG(INFO) << "Destroying reuse allocator ...";
    PrintAllocations(true, 16);
  }

  // Check that everything was freed.  Note that in some unit tests this may not
  // be the case.
  SB_LOG_IF(ERROR, allocated_block_head_ != nullptr)
      << total_allocated_blocks_ << " blocks still allocated.";

  for (auto fallback_allocation : fallback_allocations_) {
    fallback_allocator_->Free(fallback_allocation.address);
  }
}

InPlaceReuseAllocatorBase::FreeBlockSet::iterator
InPlaceReuseAllocatorBase::ExpandToFit(size_t size, size_t alignment) {
  if (ExtraLogLevel() >= 1) {
    int capacity = GetCapacity();
    int allocated = GetAllocated();
    int64_t free_percentage =
        capacity == 0
            ? 0
            : static_cast<int64_t>(capacity - allocated) * 100 / capacity;

    SB_LOG(INFO) << "Try to expand for an allocation of " << size
                 << " bytes when capacity is " << capacity << " and "
                 << capacity - allocated << " bytes free (" << free_percentage
                 << "%).";
  }

  void* ptr = NULL;
  size_t size_to_try = 0;
  // We try to allocate in unit of |allocation_increment_| to minimize
  // fragmentation.
  if (allocation_increment_ > size) {
    size_to_try = allocation_increment_;
    if (!max_capacity_in_bytes_ ||
        capacity_in_bytes_ + size_to_try <= max_capacity_in_bytes_) {
      ptr = fallback_allocator_->AllocateForAlignment(&size_to_try, alignment);
    }
  }
  // |ptr| being null indicates the above allocation failed, or in the rare case
  // |size| is larger than |allocation_increment_|. Try to allocate a block of
  // |size| instead for both cases.
  if (ptr == NULL) {
    size_to_try = size;
    if (!max_capacity_in_bytes_ ||
        capacity_in_bytes_ + size_to_try <= max_capacity_in_bytes_) {
      ptr = fallback_allocator_->AllocateForAlignment(&size_to_try, alignment);
    }
  }
  if (ptr != NULL) {
    fallback_allocations_.emplace_back(ptr, size_to_try);
    capacity_in_bytes_ += size_to_try;
    auto free_block_iter = AddFreeBlock(MemoryBlock(
        static_cast<int>(fallback_allocations_.size() - 1), ptr, size_to_try));

    if (ExtraLogLevel() >= 1) {
      int capacity = GetCapacity();
      int allocated = GetAllocated();
      int64_t free_percentage =
          capacity == 0
              ? 0
              : static_cast<int64_t>(capacity - allocated) * 100 / capacity;

      SB_LOG(INFO) << "Allocated " << size_to_try
                   << " bytes from fallback allocator (" << ptr
                   << "), capacity expanded to " << capacity << " with "
                   << capacity - allocated << " bytes free (" << free_percentage
                   << "%)";
      PrintAllocations(true, 16);
    }

    return free_block_iter;
  }

  if (free_blocks_.empty()) {
    SB_LOG_IF(INFO, ExtraLogLevel() >= 1) << "Failed to expand.";
    return free_blocks_.end();
  }

  // If control reaches here, then the prior allocation attempts have failed.
  // We failed to allocate for |size| from the fallback allocator, try to
  // allocate the difference between |size| and the size of the right most block
  // in the hope that they are continuous and can be connect to a block that is
  // large enough to fulfill |size|.
  size_t free_address = AsInteger(free_blocks_.rbegin()->address());
  size_t free_size = free_blocks_.rbegin()->size();
  size_t aligned_address = AlignUp(free_address, alignment);
  // In order to calculate |size_to_allocate|, we need to account for two
  // possible scenarios: when |aligned_address| is within the free block region,
  // or when it is after the free block region.
  //
  // Scenario 1:
  //
  // |free_address|      |free_address + free_size|
  //   |                 |
  //   | <- free_size -> | <- size_to_allocate -> |
  //   --------------------------------------------
  //               |<-          size           -> |
  //               |
  // |aligned_address|
  //
  // Scenario 2:
  //
  // |free_address|
  //   |
  //   | <- free_size -> | <- size_to_allocate -> |
  //   --------------------------------------------
  //                     |           | <- size -> |
  //                     |           |
  // |free_address + free_size|  |aligned_address|
  size_t size_to_allocate = aligned_address + size - free_address - free_size;
  if (max_capacity_in_bytes_ &&
      capacity_in_bytes_ + size_to_allocate > max_capacity_in_bytes_) {
    SB_LOG_IF(INFO, ExtraLogLevel() >= 1) << "Failed to expand.";
    return free_blocks_.end();
  }
  SB_DCHECK_GT(size_to_allocate, 0U);
  ptr = fallback_allocator_->AllocateForAlignment(&size_to_allocate, 1);
  if (ptr == NULL) {
    return free_blocks_.end();
  }

  fallback_allocations_.emplace_back(ptr, size_to_allocate);
  capacity_in_bytes_ += size_to_allocate;
  AddFreeBlock(MemoryBlock(static_cast<int>(fallback_allocations_.size() - 1),
                           ptr, size_to_allocate));
  FreeBlockSet::iterator iter = free_blocks_.end();
  --iter;

  if (ExtraLogLevel() >= 1) {
    int capacity = GetCapacity();
    int allocated = GetAllocated();
    int64_t free_percentage =
        capacity == 0
            ? 0
            : static_cast<int64_t>(capacity - allocated) * 100 / capacity;

    SB_LOG(INFO) << "Allocated " << size_to_allocate
                 << " bytes from fallback allocator (" << ptr
                 << "), capacity expanded to " << capacity << " with "
                 << capacity - allocated << " bytes free (" << free_percentage
                 << "%)";
    PrintAllocations(true, 16);
  }

  if (iter->CanFulfill(size, alignment)) {
    return iter;
  } else {
    SB_LOG_IF(INFO, ExtraLogLevel() >= 1)
        << "Failed to allocate after expanding.";
    return free_blocks_.end();
  }
}

void InPlaceReuseAllocatorBase::AddAllocatedBlock(const MemoryBlock& block) {
  BlockMetadata* metadata = static_cast<BlockMetadata*>(block.address());

  metadata->signature = this;
  metadata->fallback_index = block.fallback_allocation_index();
  metadata->size = block.size();

  // The node will be the new head, so let the existing head be its next node.
  if (allocated_block_head_) {
    SB_DCHECK_EQ(allocated_block_head_->previous, nullptr);

    allocated_block_head_->previous = metadata;
    metadata->next = allocated_block_head_;
  } else {
    metadata->next = nullptr;
  }

  // Now set it to the new head
  metadata->previous = nullptr;
  allocated_block_head_ = metadata;

  total_allocated_in_bytes_ += block.size();
  ++total_allocated_blocks_;
}

InPlaceReuseAllocatorBase::FreeBlockSet::iterator
InPlaceReuseAllocatorBase::AddFreeBlock(MemoryBlock block_to_add) {
  if (free_blocks_.size() == 0) {
    return free_blocks_.insert(block_to_add).first;
  }

  // See if we can merge this block with one on the right or left.
  FreeBlockSet::iterator it = free_blocks_.lower_bound(block_to_add);
  // lower_bound will return an iterator to our neighbor on the right,
  // if one exists.
  FreeBlockSet::iterator right_to_erase = free_blocks_.end();
  FreeBlockSet::iterator left_to_erase = free_blocks_.end();
  FreeBlockSet::iterator insert_hint = it;

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
    insert_hint = right_to_erase;
    ++insert_hint;
    free_blocks_.erase(right_to_erase);
  }
  if (left_to_erase != free_blocks_.end()) {
    free_blocks_.erase(left_to_erase);
  }

  return free_blocks_.insert(insert_hint, block_to_add);
}

void InPlaceReuseAllocatorBase::RemoveFreeBlock(FreeBlockSet::iterator it) {
  free_blocks_.erase(it);
}

}  // namespace starboard

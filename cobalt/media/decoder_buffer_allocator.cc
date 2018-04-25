// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/media/decoder_buffer_allocator.h"

#include <vector>

#include "nb/allocator.h"
#include "nb/memory_scope.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {

namespace {

const bool kEnableMultiblockAllocate = false;
const bool kEnableAllocationLog = false;

const std::size_t kAllocationRecordGranularity = 512 * 1024;
const std::size_t kSmallAllocationThreshold = 512;

bool IsLargeAllocation(std::size_t size) {
  return size > kSmallAllocationThreshold;
}

}  // namespace

DecoderBufferAllocator::DecoderBufferAllocator() {
#if COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
#if COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND
  DLOG(INFO) << "Allocated media buffer pool on demand.";
#else   // COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND
  TRACK_MEMORY_SCOPE("Media");

  reuse_allcator_.reset(new ReuseAllcator(&fallback_allocator_,
                                          COBALT_MEDIA_BUFFER_INITIAL_CAPACITY,
                                          COBALT_MEDIA_BUFFER_ALLOCATION_UNIT));
  DLOG(INFO) << "Allocated " << COBALT_MEDIA_BUFFER_INITIAL_CAPACITY
             << " bytes for media buffer pool as its initial buffer.";
#endif  // COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND
#else   // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
  DLOG(INFO) << "Allocated media buffer memory using SbMemory* functions.";
#endif  // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
}

DecoderBufferAllocator::~DecoderBufferAllocator() {
#if COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
  TRACK_MEMORY_SCOPE("Media");

  starboard::ScopedLock scoped_lock(mutex_);

  if (reuse_allcator_) {
    DCHECK_EQ(reuse_allcator_->GetAllocated(), 0);
    reuse_allcator_.reset();
  }
#endif  // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
}

DecoderBuffer::Allocator::Allocations DecoderBufferAllocator::Allocate(
    size_t size, size_t alignment, intptr_t context) {
  TRACK_MEMORY_SCOPE("Media");

#if COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
  starboard::ScopedLock scoped_lock(mutex_);

  if (!reuse_allcator_) {
    reuse_allcator_.reset(new ReuseAllcator(
        &fallback_allocator_, COBALT_MEDIA_BUFFER_INITIAL_CAPACITY,
        COBALT_MEDIA_BUFFER_ALLOCATION_UNIT));
    DLOG(INFO) << "Returned " << COBALT_MEDIA_BUFFER_INITIAL_CAPACITY
               << " bytes from media buffer pool to system.";
  }

  if (!kEnableMultiblockAllocate || kEnableAllocationLog) {
    void* p = reuse_allcator_->Allocate(size, alignment);
    LOG_IF(INFO, kEnableAllocationLog)
        << "======== Media Allocation Log " << p << " " << size << " "
        << alignment << " " << context;
    UpdateAllocationRecord();
    return Allocations(p, size);
  }

  std::size_t allocated_size = size;
  void* p =
      reuse_allcator_->AllocateBestBlock(alignment, context, &allocated_size);
  DCHECK_LE(allocated_size, size);
  if (allocated_size == size) {
    UpdateAllocationRecord();
    return Allocations(p, size);
  }

  std::vector<void*> buffers = {p};
  std::vector<int> buffer_sizes = {static_cast<int>(allocated_size)};
  size -= allocated_size;

  while (size > 0) {
    allocated_size = size;
    void* p =
        reuse_allcator_->AllocateBestBlock(alignment, context, &allocated_size);
    DCHECK_LE(allocated_size, size);
    buffers.push_back(p);
    buffer_sizes.push_back(allocated_size);

    size -= allocated_size;
  }
  UpdateAllocationRecord(buffers.size());
  return Allocations(static_cast<int>(buffers.size()), buffers.data(),
                     buffer_sizes.data());
#else   // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
  return Allocations(SbMemoryAllocateAligned(alignment, size), size);
#endif  // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
}

void DecoderBufferAllocator::Free(Allocations allocations) {
  TRACK_MEMORY_SCOPE("Media");

#if COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
  starboard::ScopedLock scoped_lock(mutex_);

  DCHECK(reuse_allcator_);

  if (kEnableAllocationLog) {
    DCHECK_EQ(allocations.number_of_buffers(), 1);
    LOG(INFO) << "======== Media Allocation Log " << allocations.buffers()[0];
  }
  for (int i = 0; i < allocations.number_of_buffers(); ++i) {
    reuse_allcator_->Free(allocations.buffers()[i]);
  }
#if COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND
  if (reuse_allcator_->GetAllocated() == 0) {
    DLOG(INFO) << "Freed " << reuse_allcator_->GetCapacity()
               << " bytes of media buffer pool `on demand`.";
    reuse_allcator_.reset();
  }
#endif  // COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND
#else   // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
  for (int i = 0; i < allocations.number_of_buffers(); ++i) {
    SbMemoryDeallocateAligned(allocations.buffers()[i]);
  }
#endif  // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
}

#if COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
DecoderBufferAllocator::ReuseAllcator::ReuseAllcator(
    Allocator* fallback_allocator, std::size_t initial_capacity,
    std::size_t allocation_increment)
    : BidirectionalFitReuseAllocator(fallback_allocator, initial_capacity,
                                     kSmallAllocationThreshold,
                                     allocation_increment) {}

DecoderBufferAllocator::ReuseAllcator::FreeBlockSet::iterator
DecoderBufferAllocator::ReuseAllcator::FindBestFreeBlock(
    std::size_t size, std::size_t alignment, intptr_t context,
    FreeBlockSet::iterator begin, FreeBlockSet::iterator end,
    bool* allocate_from_front) {
  SB_DCHECK(allocate_from_front);

  auto free_block_iter =
      FindFreeBlock(size, alignment, begin, end, allocate_from_front);
  if (free_block_iter != end) {
    return free_block_iter;
  }

  *allocate_from_front = IsLargeAllocation(size);
  *allocate_from_front = context == 1;
  if (*allocate_from_front) {
    for (FreeBlockSet::iterator it = begin; it != end; ++it) {
      if (it->CanFullfill(1, alignment)) {
        return it;
      }
    }

    return end;
  }

  FreeBlockSet::reverse_iterator rbegin(end);
  FreeBlockSet::reverse_iterator rend(begin);
  for (FreeBlockSet::reverse_iterator it = rbegin; it != rend; ++it) {
    if (it->CanFullfill(1, alignment)) {
      return --it.base();
    }
  }
  return end;
}

void DecoderBufferAllocator::UpdateAllocationRecord(
    std::size_t blocks /*= 1*/) const {
#if !defined(COBALT_BUILD_TYPE_GOLD)
  // This code is not quite multi-thread safe but is safe enough for tracking
  // purposes.
  static std::size_t max_allocated = COBALT_MEDIA_BUFFER_INITIAL_CAPACITY / 2;
  static std::size_t max_capacity = COBALT_MEDIA_BUFFER_INITIAL_CAPACITY;
  static std::size_t max_blocks = 1;

  bool new_max_reached = false;
  if (reuse_allcator_->GetAllocated() >
      max_allocated + kAllocationRecordGranularity) {
    max_allocated = reuse_allcator_->GetAllocated();
    new_max_reached = true;
  }
  if (reuse_allcator_->GetCapacity() >
      max_capacity + kAllocationRecordGranularity) {
    max_capacity = reuse_allcator_->GetCapacity();
    new_max_reached = true;
  }
  if (blocks > max_blocks) {
    max_blocks = blocks;
    new_max_reached = true;
  }
  if (new_max_reached) {
    SB_LOG(ERROR) << "======== New Media Buffer Allocation Record ========\n"
                  << "\tMax Allocated: " << max_allocated
                  << "  Max Capacity: " << max_capacity
                  << "  Max Blocks: " << max_blocks;
    // TODO: Enable the following line once PrintAllocations() accepts max line
    // as a parameter.
    // reuse_allcator_->PrintAllocations();
  }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
}
#endif  // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
}  // namespace media
}  // namespace cobalt

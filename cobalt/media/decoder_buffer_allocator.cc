// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/math/size.h"
#include "cobalt/media/base/starboard_utils.h"
#include "cobalt/media/base/video_resolution.h"
#include "nb/allocator.h"
#include "nb/memory_scope.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {

namespace {

const bool kEnableAllocationLog = false;

const std::size_t kAllocationRecordGranularity = 512 * 1024;
// Used to determine if the memory allocated is large. The underlying logic can
// be different.
const std::size_t kSmallAllocationThreshold = 512;

bool IsLargeAllocation(std::size_t size) {
  return size > kSmallAllocationThreshold;
}

}  // namespace

DecoderBufferAllocator::DecoderBufferAllocator()
    : using_memory_pool_(SbMediaIsBufferUsingMemoryPool()),
      is_memory_pool_allocated_on_demand_(
          SbMediaIsBufferPoolAllocateOnDemand()),
      initial_capacity_(SbMediaGetInitialBufferCapacity()),
      allocation_unit_(SbMediaGetBufferAllocationUnit()) {
  if (!using_memory_pool_) {
    DLOG(INFO) << "Allocated media buffer memory using SbMemory* functions.";
    return;
  }

  if (is_memory_pool_allocated_on_demand_) {
    DLOG(INFO) << "Allocated media buffer pool on demand.";
    return;
  }

  TRACK_MEMORY_SCOPE("Media");

  // We cannot call SbMediaGetMaxBufferCapacity because |video_codec_| is not
  // set yet. Use 0 (unbounded) until |video_codec_| is updated in
  // UpdateVideoConfig().
  int max_capacity = 0;
  reuse_allocator_.reset(new ReuseAllocator(
      &fallback_allocator_, initial_capacity_, allocation_unit_, max_capacity));
  DLOG(INFO) << "Allocated " << initial_capacity_
             << " bytes for media buffer pool as its initial buffer, with max"
             << " capacity set to " << max_capacity;
}

DecoderBufferAllocator::~DecoderBufferAllocator() {
  if (!using_memory_pool_) {
    return;
  }

  TRACK_MEMORY_SCOPE("Media");

  starboard::ScopedLock scoped_lock(mutex_);

  if (reuse_allocator_) {
    DCHECK_EQ(reuse_allocator_->GetAllocated(), 0);
    reuse_allocator_.reset();
  }
}

DecoderBuffer::Allocator::Allocations DecoderBufferAllocator::Allocate(
    size_t size, size_t alignment, intptr_t context) {
  TRACK_MEMORY_SCOPE("Media");

  if (!using_memory_pool_) {
    sbmemory_bytes_used_.fetch_add(size);
    return Allocations(SbMemoryAllocateAligned(alignment, size), size);
  }

  starboard::ScopedLock scoped_lock(mutex_);

  if (!reuse_allocator_) {
    DCHECK(is_memory_pool_allocated_on_demand_);

    int max_capacity = 0;
    if (video_codec_ != kSbMediaVideoCodecNone) {
      DCHECK_GT(resolution_width_, 0);
      DCHECK_GT(resolution_height_, 0);

      max_capacity = SbMediaGetMaxBufferCapacity(
          video_codec_, resolution_width_, resolution_height_, bits_per_pixel_);
    }
    reuse_allocator_.reset(new ReuseAllocator(&fallback_allocator_,
                                              initial_capacity_,
                                              allocation_unit_, max_capacity));
    DLOG(INFO) << "Allocated " << initial_capacity_
               << " bytes for media buffer pool, with max capacity set to "
               << max_capacity;
  }

  void* p = reuse_allocator_->Allocate(size, alignment);
  if (!p) {
    return Allocations();
  }
  LOG_IF(INFO, kEnableAllocationLog)
      << "Media Allocation Log " << p << " " << size << " " << alignment << " "
      << context;
  if (!UpdateAllocationRecord()) {
    // UpdateAllocationRecord may fail with non-NULL p when capacity is
    // exceeded.
    reuse_allocator_->Free(p);
    return Allocations();
  }
  return Allocations(p, size);
}

void DecoderBufferAllocator::Free(Allocations allocations) {
  TRACK_MEMORY_SCOPE("Media");

  if (!using_memory_pool_) {
    for (int i = 0; i < allocations.number_of_buffers(); ++i) {
      sbmemory_bytes_used_.fetch_sub(allocations.buffer_sizes()[i]);
      SbMemoryDeallocateAligned(allocations.buffers()[i]);
    }
    return;
  }

  starboard::ScopedLock scoped_lock(mutex_);

  DCHECK(reuse_allocator_);

  if (kEnableAllocationLog) {
    DCHECK_EQ(allocations.number_of_buffers(), 1);
    LOG(INFO) << "Media Allocation Log " << allocations.buffers()[0];
  }

  for (int i = 0; i < allocations.number_of_buffers(); ++i) {
    reuse_allocator_->Free(allocations.buffers()[i]);
  }

  if (is_memory_pool_allocated_on_demand_) {
    if (reuse_allocator_->GetAllocated() == 0) {
      DLOG(INFO) << "Freed " << reuse_allocator_->GetCapacity()
                 << " bytes of media buffer pool `on demand`.";
      reuse_allocator_.reset();
    }
  }
}

void DecoderBufferAllocator::UpdateVideoConfig(
    const VideoDecoderConfig& config) {
  if (!using_memory_pool_) {
    return;
  }

  starboard::ScopedLock scoped_lock(mutex_);

  video_codec_ = MediaVideoCodecToSbMediaVideoCodec(config.codec());
  resolution_width_ = config.visible_rect().size().width();
  resolution_height_ = config.visible_rect().size().height();
  bits_per_pixel_ = config.webm_color_metadata().BitsPerChannel;

  if (!reuse_allocator_) {
    return;
  }

  reuse_allocator_->IncreaseMaxCapacityIfNecessary(SbMediaGetMaxBufferCapacity(
      video_codec_, resolution_width_, resolution_height_, bits_per_pixel_));
  DLOG(INFO) << "Max capacity of decoder buffer allocator after increasing is "
             << reuse_allocator_->GetCapacity();
}

DecoderBufferAllocator::ReuseAllocator::ReuseAllocator(
    Allocator* fallback_allocator, std::size_t initial_capacity,
    std::size_t allocation_increment, std::size_t max_capacity)
    : BidirectionalFitReuseAllocator(fallback_allocator, initial_capacity,
                                     kSmallAllocationThreshold,
                                     allocation_increment, max_capacity) {}

DecoderBufferAllocator::ReuseAllocator::FreeBlockSet::iterator
DecoderBufferAllocator::ReuseAllocator::FindBestFreeBlock(
    std::size_t size, std::size_t alignment, intptr_t context,
    FreeBlockSet::iterator begin, FreeBlockSet::iterator end,
    bool* allocate_from_front) {
  DCHECK(allocate_from_front);

  auto free_block_iter =
      FindFreeBlock(size, alignment, begin, end, allocate_from_front);
  if (free_block_iter != end) {
    return free_block_iter;
  }

  *allocate_from_front = size > kSmallAllocationThreshold;
  *allocate_from_front = context == 1;
  if (*allocate_from_front) {
    for (FreeBlockSet::iterator it = begin; it != end; ++it) {
      if (it->CanFulfill(1, alignment)) {
        return it;
      }
    }

    return end;
  }

  FreeBlockSet::reverse_iterator rbegin(end);
  FreeBlockSet::reverse_iterator rend(begin);
  for (FreeBlockSet::reverse_iterator it = rbegin; it != rend; ++it) {
    if (it->CanFulfill(1, alignment)) {
      return --it.base();
    }
  }
  return end;
}

std::size_t DecoderBufferAllocator::GetAllocatedMemory() const {
  if (!using_memory_pool_) {
    return sbmemory_bytes_used_.load();
  }
  starboard::ScopedLock scoped_lock(mutex_);
  return reuse_allocator_ ? reuse_allocator_->GetAllocated() : 0;
}

std::size_t DecoderBufferAllocator::GetCurrentMemoryCapacity() const {
  if (!using_memory_pool_) {
    return sbmemory_bytes_used_.load();
  }
  starboard::ScopedLock scoped_lock(mutex_);
  return reuse_allocator_ ? reuse_allocator_->GetCapacity() : 0;
}

std::size_t DecoderBufferAllocator::GetMaximumMemoryCapacity() const {
  starboard::ScopedLock scoped_lock(mutex_);
  if (video_codec_ == kSbMediaVideoCodecNone) {
    return 0;
  }
  if (!using_memory_pool_) {
    return SbMediaGetMaxBufferCapacity(video_codec_, resolution_width_,
                                       resolution_height_, bits_per_pixel_);
  }
  return reuse_allocator_
             ? reuse_allocator_->max_capacity()
             : SbMediaGetMaxBufferCapacity(video_codec_, resolution_width_,
                                           resolution_height_, bits_per_pixel_);
}

bool DecoderBufferAllocator::UpdateAllocationRecord() const {
#if !defined(COBALT_BUILD_TYPE_GOLD)
  // This code is not quite multi-thread safe but is safe enough for tracking
  // purposes.
  static std::size_t max_allocated = initial_capacity_ / 2;
  static std::size_t max_capacity = initial_capacity_;

  bool new_max_reached = false;
  if (reuse_allocator_->GetAllocated() >
      max_allocated + kAllocationRecordGranularity) {
    max_allocated = reuse_allocator_->GetAllocated();
    new_max_reached = true;
  }
  if (reuse_allocator_->GetCapacity() >
      max_capacity + kAllocationRecordGranularity) {
    max_capacity = reuse_allocator_->GetCapacity();
    new_max_reached = true;
  }
  if (new_max_reached) {
    SB_LOG(INFO) << "New Media Buffer Allocation Record: "
                 << "Max Allocated: " << max_allocated
                 << "  Max Capacity: " << max_capacity;
    // TODO: Enable the following line once PrintAllocations() accepts max line
    // as a parameter.
    // reuse_allocator_->PrintAllocations();
  }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

  if (reuse_allocator_->CapacityExceeded()) {
    SB_LOG(WARNING) << "Cobalt media buffer capacity "
                    << reuse_allocator_->GetCapacity()
                    << " exceeded max capacity "
                    << reuse_allocator_->max_capacity();
    return false;
  }
  return true;
}

}  // namespace media
}  // namespace cobalt

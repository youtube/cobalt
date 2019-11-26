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
const std::size_t kSmallAllocationThreshold = 512;

bool IsLargeAllocation(std::size_t size) {
  return size > kSmallAllocationThreshold;
}

bool IsMemoryPoolEnabled() {
#if SB_API_VERSION >= 10
  return SbMediaIsBufferUsingMemoryPool();
#elif COBALT_MEDIA_BUFFER_INITIAL_CAPACITY > 0 || \
      COBALT_MEDIA_BUFFER_ALLOCATION_UNIT > 0
  return true;
#endif  // COBALT_MEDIA_BUFFER_INITIAL_CAPACITY == 0 &&
        // COBALT_MEDIA_BUFFER_ALLOCATION_UNIT == 0
  return false;
}

bool IsMemoryPoolAllocatedOnDemand() {
#if SB_API_VERSION >= 10
  return SbMediaIsBufferPoolAllocateOnDemand();
#else  // SB_API_VERSION >= 10
  return COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND;
#endif  // SB_API_VERSION >= 10
}

int GetInitialBufferCapacity() {
#if SB_API_VERSION >= 10
  return SbMediaGetInitialBufferCapacity();
#else   // SB_API_VERSION >= 10
  return COBALT_MEDIA_BUFFER_INITIAL_CAPACITY;
#endif  // SB_API_VERSION >= 10
}

int GetBufferAllocationUnit() {
#if SB_API_VERSION >= 10
  return SbMediaGetBufferAllocationUnit();
#else   // SB_API_VERSION >= 10
  return COBALT_MEDIA_BUFFER_ALLOCATION_UNIT;
#endif  // SB_API_VERSION >= 10
}

}  // namespace

DecoderBufferAllocator::DecoderBufferAllocator()
    : using_memory_pool_(IsMemoryPoolEnabled()),
      is_memory_pool_allocated_on_demand_(IsMemoryPoolAllocatedOnDemand()),
      initial_capacity_(GetInitialBufferCapacity()),
      allocation_unit_(GetBufferAllocationUnit()) {
  if (!using_memory_pool_) {
    DLOG(INFO) << "Allocated media buffer memory using SbMemory* functions.";
    return;
  }

  if (is_memory_pool_allocated_on_demand_) {
    DLOG(INFO) << "Allocated media buffer pool on demand.";
    return;
  }

  TRACK_MEMORY_SCOPE("Media");

#if SB_API_VERSION >= 10
  // We cannot call SbMediaGetMaxBufferCapacity because |video_codec_| is not
  // set yet. Use 0 (unbounded) until |video_codec_| is updated in
  // UpdateVideoConfig().
  int max_capacity = 0;
#else   // SB_API_VERSION >= 10
  int max_capacity = COBALT_MEDIA_BUFFER_MAX_CAPACITY_1080P;
#endif  // SB_API_VERSION >= 10
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
    return Allocations(SbMemoryAllocateAligned(alignment, size), size);
  }

  starboard::ScopedLock scoped_lock(mutex_);

  if (!reuse_allocator_) {
    DCHECK(is_memory_pool_allocated_on_demand_);

#if SB_API_VERSION >= 10
    int max_capacity = 0;
    if (video_codec_ != kSbMediaVideoCodecNone) {
      DCHECK_GT(resolution_width_, 0);
      DCHECK_GT(resolution_height_, 0);

      max_capacity = SbMediaGetMaxBufferCapacity(
          video_codec_, resolution_width_, resolution_height_, bits_per_pixel_);
    }
#else   // SB_API_VERSION >= 10
    VideoResolution resolution =
        GetVideoResolution(math::Size(resolution_width_, resolution_height_));
    int max_capacity = resolution <= kVideoResolution1080p
                           ? COBALT_MEDIA_BUFFER_MAX_CAPACITY_1080P
                           : COBALT_MEDIA_BUFFER_MAX_CAPACITY_4K;
#endif  // SB_API_VERSION >= 10
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

#if SB_API_VERSION >= 10
  reuse_allocator_->IncreaseMaxCapacityIfNecessary(SbMediaGetMaxBufferCapacity(
      video_codec_, resolution_width_, resolution_height_, bits_per_pixel_));
#else   // SB_API_VERSION >= 10
  VideoResolution resolution = GetVideoResolution(config.visible_rect().size());
  if (reuse_allocator_->max_capacity() && resolution > kVideoResolution1080p) {
    reuse_allocator_->IncreaseMaxCapacityIfNecessary(
        COBALT_MEDIA_BUFFER_MAX_CAPACITY_4K);
  }
#endif  // SB_API_VERSION >= 10
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
    SB_LOG(ERROR) << "New Media Buffer Allocation Record: "
                  << "Max Allocated: " << max_allocated
                  << "  Max Capacity: " << max_capacity;
    // TODO: Enable the following line once PrintAllocations() accepts max line
    // as a parameter.
    // reuse_allocator_->PrintAllocations();
  }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

  if (reuse_allocator_->CapacityExceeded()) {
    SB_LOG(ERROR) << "Cobalt media buffer capacity "
                  << reuse_allocator_->GetCapacity()
                  << " exceeded max capacity "
                  << reuse_allocator_->max_capacity();
    return false;
  }
  return true;
}

}  // namespace media
}  // namespace cobalt

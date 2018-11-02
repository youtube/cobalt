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

#include "cobalt/media/base/starboard_utils.h"
#include "cobalt/media/base/video_resolution.h"
#include "nb/allocator.h"
#include "nb/memory_scope.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
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
#if SB_API_VERSION >= 10
  using_memory_pool_ = SbMediaIsBufferUsingMemoryPool();
  bool pool_allocate_on_demand = SbMediaIsBufferPoolAllocateOnDemand();
#elif COBALT_MEDIA_BUFFER_USING_MEMORY_POOL
  using_memory_pool_ = true;
  bool pool_allocate_on_demand = COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND;
#endif  // SB_API_VERSION >= 10

#if COBALT_MEDIA_BUFFER_USING_MEMORY_POOL || SB_API_VERSION >= 10
  if (using_memory_pool_) {
    if (pool_allocate_on_demand) {
      DLOG(INFO) << "Allocated media buffer pool on demand.";
    } else {
      TRACK_MEMORY_SCOPE("Media");
#if SB_API_VERSION >= 10
      int initial_capacity = SbMediaGetInitialBufferCapacity();
      // We cannot call SbMediaGetMaxBufferCapacity because |video_codec_| is
      // not set yet. Use 0 (unbounded) until |video_codec_| is updated in
      // UpdateVideoConfig.
      int max_capacity = 0;
      int allocation_unit = SbMediaGetBufferAllocationUnit();
#else   // SB_API_VERSION >= 10
      int initial_capacity = COBALT_MEDIA_BUFFER_INITIAL_CAPACITY;
      int max_capacity = COBALT_MEDIA_BUFFER_MAX_CAPACITY_1080P;
      int allocation_unit = COBALT_MEDIA_BUFFER_ALLOCATION_UNIT;
#endif  // SB_API_VERSION >= 10
      reuse_allocator_.reset(new ReuseAllocator(&fallback_allocator_,
                                                initial_capacity,
                                                allocation_unit, max_capacity));
      DLOG(INFO) << "Allocated " << initial_capacity
                 << " bytes for media buffer pool as its initial buffer.";
    }
    return;
  }
#endif  // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL || SB_API_VERSION >= 10
  DLOG(INFO) << "Allocated media buffer memory using SbMemory* functions.";
}

DecoderBufferAllocator::~DecoderBufferAllocator() {
#if COBALT_MEDIA_BUFFER_USING_MEMORY_POOL || SB_API_VERSION >= 10
  if (using_memory_pool_) {
    TRACK_MEMORY_SCOPE("Media");

    starboard::ScopedLock scoped_lock(mutex_);

    if (reuse_allocator_) {
      DCHECK_EQ(reuse_allocator_->GetAllocated(), 0);
      reuse_allocator_.reset();
    }
  }
#endif  // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL ||
        // SB_API_VERSION >= 10
}

DecoderBuffer::Allocator::Allocations DecoderBufferAllocator::Allocate(
    size_t size, size_t alignment, intptr_t context) {
  TRACK_MEMORY_SCOPE("Media");

#if COBALT_MEDIA_BUFFER_USING_MEMORY_POOL || SB_API_VERSION >= 10
  if (using_memory_pool_) {
    starboard::ScopedLock scoped_lock(mutex_);

    if (!reuse_allocator_) {
#if SB_API_VERSION >= 10
      int initial_capacity = SbMediaGetInitialBufferCapacity();
      int max_capacity = SbMediaGetMaxBufferCapacity(
          video_codec_, resolution_width_, resolution_height_, bits_per_pixel_);
      int allocation_unit = SbMediaGetBufferAllocationUnit();
#else   // SB_API_VERSION >= 10
      int initial_capacity = COBALT_MEDIA_BUFFER_INITIAL_CAPACITY;
      int max_capacity = COBALT_MEDIA_BUFFER_MAX_CAPACITY_1080P;
      int allocation_unit = COBALT_MEDIA_BUFFER_ALLOCATION_UNIT;
#endif  // SB_API_VERSION >= 10
      reuse_allocator_.reset(new ReuseAllocator(&fallback_allocator_,
                                                initial_capacity,
                                                allocation_unit, max_capacity));
      DLOG(INFO) << "Returned " << initial_capacity
                 << " bytes from media buffer pool to system.";
    }

    if (!kEnableMultiblockAllocate || kEnableAllocationLog) {
      void* p = reuse_allocator_->Allocate(size, alignment);
      if (!p) {
        return Allocations();
      }
      LOG_IF(INFO, kEnableAllocationLog)
          << "Media Allocation Log " << p << " " << size << " "
          << alignment << " " << context;
      if (!UpdateAllocationRecord()) {
        // UpdateAllocationRecord may fail with non-NULL p when capacity is
        // exceeded.
        reuse_allocator_->Free(p);
        return Allocations();
      }
      return Allocations(p, size);
    }

    std::size_t allocated_size = size;
    void* p = reuse_allocator_->AllocateBestBlock(alignment, context,
                                                  &allocated_size);
    DCHECK_LE(allocated_size, size);
    if (!p) {
      return Allocations();
    }
    if (allocated_size == size) {
      if (!UpdateAllocationRecord()) {
        // UpdateAllocationRecord may fail with non-NULL p when capacity is
        // exceeded.
        reuse_allocator_->Free(p);
        return Allocations();
      }
      return Allocations(p, size);
    }

    std::vector<void*> buffers = {p};
    std::vector<int> buffer_sizes = {static_cast<int>(allocated_size)};
    size -= allocated_size;

    bool update_allocation_record_failed = false;
    while (size > 0) {
      allocated_size = size;
      void* p = reuse_allocator_->AllocateBestBlock(alignment, context,
                                                    &allocated_size);
      if (!p) {
        return Allocations();
      }
      if (!UpdateAllocationRecord()) {
        update_allocation_record_failed = true;
        reuse_allocator_->Free(p);
        break;
      }
      DCHECK_LE(allocated_size, size);
      buffers.push_back(p);
      buffer_sizes.push_back(allocated_size);

      size -= allocated_size;
    }
    if (update_allocation_record_failed) {
      for (auto& p : buffers) {
        reuse_allocator_->Free(p);
      }
      return Allocations();
    }
    return Allocations(static_cast<int>(buffers.size()), buffers.data(),
                       buffer_sizes.data());
  }
#endif  // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL ||
        // SB_API_VERSION >= 10
  return Allocations(SbMemoryAllocateAligned(alignment, size), size);
}

void DecoderBufferAllocator::Free(Allocations allocations) {
  TRACK_MEMORY_SCOPE("Media");
#if SB_API_VERSION >= 10
  bool pool_allocate_on_demand = SbMediaIsBufferPoolAllocateOnDemand();
#else   // SB_API_VERSION >= 10
  bool pool_allocate_on_demand = COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND;
#endif  // SB_API_VERSION >= 10

#if COBALT_MEDIA_BUFFER_USING_MEMORY_POOL || SB_API_VERSION >= 10
  if (using_memory_pool_) {
    starboard::ScopedLock scoped_lock(mutex_);

    DCHECK(reuse_allocator_);

    if (kEnableAllocationLog) {
      DCHECK_EQ(allocations.number_of_buffers(), 1);
      LOG(INFO) << "Media Allocation Log " << allocations.buffers()[0];
    }
    for (int i = 0; i < allocations.number_of_buffers(); ++i) {
      reuse_allocator_->Free(allocations.buffers()[i]);
    }
    if (pool_allocate_on_demand) {
      if (reuse_allocator_->GetAllocated() == 0) {
        DLOG(INFO) << "Freed " << reuse_allocator_->GetCapacity()
                   << " bytes of media buffer pool `on demand`.";
        reuse_allocator_.reset();
      }
    }
    return;
  }
#endif  // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL ||
        // SB_API_VERSION >= 10
  for (int i = 0; i < allocations.number_of_buffers(); ++i) {
    SbMemoryDeallocateAligned(allocations.buffers()[i]);
  }
}

void DecoderBufferAllocator::UpdateVideoConfig(
    const VideoDecoderConfig& config) {
#if COBALT_MEDIA_BUFFER_USING_MEMORY_POOL || SB_API_VERSION >= 10
  if (using_memory_pool_) {
#if SB_API_VERSION >= 10
    video_codec_ = MediaVideoCodecToSbMediaVideoCodec(config.codec());
    resolution_width_ = config.visible_rect().size().width();
    resolution_height_ = config.visible_rect().size().height();
    bits_per_pixel_ = config.webm_color_metadata().BitsPerChannel;
#endif  // SB_API_VERSION >= 10
    if (!reuse_allocator_) {
      return;
    }
#if SB_API_VERSION >= 10
    reuse_allocator_->set_max_capacity(SbMediaGetMaxBufferCapacity(
        video_codec_, resolution_width_, resolution_height_, bits_per_pixel_));
#else   // SB_API_VERSION >= 10
    VideoResolution resolution =
        GetVideoResolution(config.visible_rect().size());
    if (reuse_allocator_->max_capacity() &&
        resolution > kVideoResolution1080p) {
      reuse_allocator_->set_max_capacity(COBALT_MEDIA_BUFFER_MAX_CAPACITY_4K);
    }
#endif  // SB_API_VERSION >= 10
  }
#endif  // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL ||
        // SB_API_VERSION >= 10
}

#if COBALT_MEDIA_BUFFER_USING_MEMORY_POOL || SB_API_VERSION >= 10
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

bool DecoderBufferAllocator::UpdateAllocationRecord(
    std::size_t blocks /*= 1*/) const {
#if !defined(COBALT_BUILD_TYPE_GOLD)
  // This code is not quite multi-thread safe but is safe enough for tracking
  // purposes.
#if SB_API_VERSION >= 10
  int initial_capacity = SbMediaGetInitialBufferCapacity();
#else   // SB_API_VERSION >= 10
  int initial_capacity = COBALT_MEDIA_BUFFER_INITIAL_CAPACITY;
#endif  // SB_API_VERSION >= 10
  static std::size_t max_allocated = initial_capacity / 2;
  static std::size_t max_capacity = initial_capacity;
  static std::size_t max_blocks = 1;

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
  if (blocks > max_blocks) {
    max_blocks = blocks;
    new_max_reached = true;
  }
  if (new_max_reached) {
    SB_LOG(ERROR) << "New Media Buffer Allocation Record: "
                  << "Max Allocated: " << max_allocated
                  << "  Max Capacity: " << max_capacity
                  << "  Max Blocks: " << max_blocks;
    // TODO: Enable the following line once PrintAllocations() accepts max line
    // as a parameter.
    // reuse_allocator_->PrintAllocations();
  }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
#if COBALT_MEDIA_BUFFER_MAX_CAPACITY_1080P > 0 || \
    COBALT_MEDIA_BUFFER_MAX_CAPACITY_4K > 0 || SB_API_VERSION >= 10
  if (reuse_allocator_->CapacityExceeded()) {
    SB_LOG(ERROR) << "Cobalt media buffer capacity "
                  << reuse_allocator_->GetCapacity()
                  << " exceeded max capacity "
                  << reuse_allocator_->max_capacity();
    return false;
  }
#endif  // COBALT_MEDIA_BUFFER_MAX_CAPACITY_1080P > 0 ||
        // COBALT_MEDIA_BUFFER_MAX_CAPACITY_4K > 0 || SB_API_VERSION >= 10
  return true;
}
#endif  // COBALT_MEDIA_BUFFER_USING_MEMORY_POOL || SB_API_VERSION >= 10
}  // namespace media
}  // namespace cobalt

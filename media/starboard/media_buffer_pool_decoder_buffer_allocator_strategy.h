// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_STARBOARD_MEDIA_BUFFER_POOL_DECODER_BUFFER_ALLOCATOR_STRATEGY_H_
#define MEDIA_STARBOARD_MEDIA_BUFFER_POOL_DECODER_BUFFER_ALLOCATOR_STRATEGY_H_

#include <memory>

#include "media/starboard/decoder_buffer_allocator.h"
#include "media/starboard/starboard_memory_allocator.h"
#include "starboard/common/bidirectional_fit_reuse_allocator.h"
#include "starboard/common/experimental/media_buffer_pool.h"
#include "starboard/common/experimental/media_buffer_pool_bidirectional_reuse_allocator.h"
#include "starboard/common/in_place_reuse_allocator_base.h"
#include "starboard/common/reuse_allocator_base.h"
#include "starboard/media.h"

namespace media {

class MediaBufferPoolDecoderBufferAllocatorStrategy
    : public DecoderBufferAllocator::Strategy {
 public:
  MediaBufferPoolDecoderBufferAllocatorStrategy(
      starboard::common::experimental::MediaBufferPool* media_buffer_pool,
      size_t video_buffer_initial_capacity,
      size_t video_buffer_allocation_increment);

  void* Allocate(DemuxerStream::Type type,
                 size_t size,
                 size_t alignment) override;

  void Free(DemuxerStream::Type type, void* p) override;

  void Write(void* p, const void* data, size_t size) override;

  size_t GetCapacity() const override;

  size_t GetAllocated() const override;

 private:
  typedef starboard::common::experimental::
      MediaBufferPoolBidirectionalReuseAllocator
          MediaBufferPoolBidirectionalReuseAllocator;

  // We do not treat small allocations differently from large ones in this
  // strategy.
  static constexpr size_t kSmallAllocationThreshold = 0;
  // Use a smaller allocation increment for audio buffers as they are
  // significantly smaller than video buffers.
  static constexpr size_t kAudioAllocationIncrement = 1024 * 1024;

  starboard::common::experimental::MediaBufferPool* const media_buffer_pool_;
  const size_t video_buffer_initial_capacity_;
  const size_t video_buffer_allocation_increment_;

  StarboardMemoryAllocator audio_fallback_allocator_;
  starboard::common::BidirectionalFitReuseAllocator<
      starboard::common::InPlaceReuseAllocatorBase>
      audio_allocator_;

  // TODO(b/369245553): Consider using a small in-memory pool (e.g. 10 MBytes),
  // maybe combined with the in-memory audio pool, to speed up startup.
  std::unique_ptr<MediaBufferPoolBidirectionalReuseAllocator> video_allocator_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_MEDIA_BUFFER_POOL_DECODER_BUFFER_ALLOCATOR_STRATEGY_H_

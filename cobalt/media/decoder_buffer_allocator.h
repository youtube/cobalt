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

#ifndef COBALT_MEDIA_DECODER_BUFFER_ALLOCATOR_H_
#define COBALT_MEDIA_DECODER_BUFFER_ALLOCATOR_H_

#include <memory>

#include "base/compiler_specific.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/video_decoder_config.h"
#include "cobalt/media/base/video_resolution.h"
#include "cobalt/media/decoder_buffer_memory_info.h"
#include "nb/bidirectional_fit_reuse_allocator.h"
#include "nb/starboard_memory_allocator.h"
#include "starboard/atomic.h"
#include "starboard/common/mutex.h"
#include "starboard/media.h"

namespace cobalt {
namespace media {

class DecoderBufferAllocator : public DecoderBuffer::Allocator,
                               public DecoderBufferMemoryInfo {
 public:
  DecoderBufferAllocator();
  ~DecoderBufferAllocator() override;

  Allocations Allocate(size_t size, size_t alignment,
                       intptr_t context) override;
  void Free(Allocations allocations) override;
  void UpdateVideoConfig(const VideoDecoderConfig& video_config) override;
  size_t GetAllocatedMemory() const override;
  size_t GetCurrentMemoryCapacity() const override;
  size_t GetMaximumMemoryCapacity() const override;

 private:
  class ReuseAllocator : public nb::BidirectionalFitReuseAllocator {
   public:
    ReuseAllocator(Allocator* fallback_allocator, std::size_t initial_capacity,
                   std::size_t allocation_increment, std::size_t max_capacity);

    FreeBlockSet::iterator FindBestFreeBlock(
        std::size_t size, std::size_t alignment, intptr_t context,
        FreeBlockSet::iterator begin, FreeBlockSet::iterator end,
        bool* allocate_from_front) override;
  };

  // Update the Allocation record, and return false if allocation exceeds the
  // max buffer capacity, or true otherwise.
  bool UpdateAllocationRecord() const;

  const bool using_memory_pool_;
  const bool is_memory_pool_allocated_on_demand_;
  const int initial_capacity_;
  const int allocation_unit_;

  starboard::Mutex mutex_;
  nb::StarboardMemoryAllocator fallback_allocator_;
  std::unique_ptr<ReuseAllocator> reuse_allocator_;

  SbMediaVideoCodec video_codec_ = kSbMediaVideoCodecNone;
  int resolution_width_ = -1;
  int resolution_height_ = -1;
  int bits_per_pixel_ = -1;

  // Monitor memory allocation and use when |using_memory_pool_| is false
  starboard::atomic_int32_t sbmemory_bytes_used_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_DECODER_BUFFER_ALLOCATOR_H_

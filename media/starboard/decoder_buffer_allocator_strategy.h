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

#ifndef MEDIA_STARBOARD_DECODER_BUFFER_ALLOCATOR_STRATEGY_H_
#define MEDIA_STARBOARD_DECODER_BUFFER_ALLOCATOR_STRATEGY_H_

#include "media/starboard/bidirectional_fit_reuse_allocator.h"
#include "media/starboard/decoder_buffer_allocator.h"
#include "media/starboard/starboard_memory_allocator.h"
#include "starboard/configuration.h"

namespace media {

template <typename ReuseAllocatorBase>
class BidirectionalFitDecoderBufferAllocatorStrategy
    : public DecoderBufferAllocator::Strategy {
 public:
  BidirectionalFitDecoderBufferAllocatorStrategy(
      std::size_t initial_capacity,
      std::size_t allocation_increment)
      : birectional_fit_allocator_(&fallback_allocator_,
                                   initial_capacity,
                                   kSmallAllocationThreshold,
                                   allocation_increment) {
    LOG(INFO) << "Using BidirectionalFitDecoderBufferAllocatorStrategy ...";
  }

  void* Allocate(DemuxerStream::Type type,
                 size_t size,
                 size_t alignment) override {
    return birectional_fit_allocator_.Allocate(size, alignment);
  }
  void Free(DemuxerStream::Type type, void* p) override {
    birectional_fit_allocator_.Free(p);
  }

  size_t GetCapacity() const override {
    return birectional_fit_allocator_.GetCapacity();
  }

  size_t GetAllocated() const override {
    return birectional_fit_allocator_.GetAllocated();
  }

 private:
  // Used to determine if the memory allocated is large. The underlying logic
  // can be different.
  static constexpr size_t kSmallAllocationThreshold = 512;

  StarboardMemoryAllocator fallback_allocator_;
  BidirectionalFitReuseAllocator<ReuseAllocatorBase> birectional_fit_allocator_;
};

class StreamTypeBasedDecoderBufferAllocatorStrategy
    : public DecoderBufferAllocator::Strategy {
 public:
  StreamTypeBasedDecoderBufferAllocatorStrategy(
      std::size_t initial_capacity,
      std::size_t allocation_increment) : audio_allocator_(&fallback_allocator_,
                       kAudioPoolInitialCapacity,
                       0,
                       kAudioPoolAllocationIncrement),
      video_allocator_(&fallback_allocator_,
                       initial_capacity,
                       kSmallAllocationThreshold,
                       allocation_increment) {
  LOG(INFO) << "Using StreamTypeBasedDecoderBufferAllocatorStrategy ...";
}

  void* Allocate(DemuxerStream::Type type,
                 size_t size,
                 size_t alignment) override {
    if (type == DemuxerStream::AUDIO) {
      return audio_allocator_.Allocate(size, alignment);
    }
    // UNKNOWN may also fall here.
    return video_allocator_.Allocate(size, alignment);
  }
  void Free(DemuxerStream::Type type, void* p) override {
    if (type == DemuxerStream::AUDIO) {
      audio_allocator_.Free(p);
      return;
    }
    // UNKNOWN may also fall here.
    video_allocator_.Free(p);
  }

  size_t GetCapacity() const override {
    return audio_allocator_.GetCapacity() + video_allocator_.GetCapacity();
  }

  size_t GetAllocated() const override {
    return audio_allocator_.GetAllocated() + video_allocator_.GetAllocated();
  }

 private:
  // Used to determine if the memory allocated is large. The underlying logic
  // can be different.
  static constexpr size_t kSmallAllocationThreshold = 512;

  // Audio budgets are not configurable by the user of the class.
  static constexpr size_t kAudioPoolInitialCapacity = 4 * 1024 * 1024;
  static constexpr size_t kAudioPoolAllocationIncrement = 1 * 1024 * 1024;

  StarboardMemoryAllocator fallback_allocator_;
  BidirectionalFitReuseAllocator audio_allocator_;
  BidirectionalFitReuseAllocator video_allocator_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_DECODER_BUFFER_ALLOCATOR_STRATEGY_H_

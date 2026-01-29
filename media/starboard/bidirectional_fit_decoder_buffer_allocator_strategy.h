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

#ifndef MEDIA_STARBOARD_BIDIRECTIONAL_FIT_DECODER_BUFFER_ALLOCATOR_STRATEGY_H_
#define MEDIA_STARBOARD_BIDIRECTIONAL_FIT_DECODER_BUFFER_ALLOCATOR_STRATEGY_H_

#include "media/starboard/decoder_buffer_allocator.h"
#include "media/starboard/starboard_memory_allocator.h"
#include "starboard/common/bidirectional_fit_reuse_allocator.h"
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
                                   allocation_increment) {}

  void* Allocate(DemuxerStream::Type type,
                 size_t size,
                 size_t alignment) override {
    return birectional_fit_allocator_.Allocate(size, alignment);
  }
  void Free(DemuxerStream::Type type, void* p) override {
    birectional_fit_allocator_.Free(p);
  }
  void Write(void* p, const void* data, size_t size) override {
    memcpy(p, data, size);
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
  starboard::common::BidirectionalFitReuseAllocator<ReuseAllocatorBase>
      birectional_fit_allocator_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_BIDIRECTIONAL_FIT_DECODER_BUFFER_ALLOCATOR_STRATEGY_H_

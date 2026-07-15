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

#include <cstddef>

#include "media/starboard/decoder_buffer_allocator.h"
#include "media/starboard/starboard_memory_allocator.h"
#include "starboard/common/bidirectional_fit_reuse_allocator.h"
#include "starboard/configuration.h"

namespace media {

template <typename ReuseAllocatorBase>
class BidirectionalFitDecoderBufferAllocatorStrategy
    : public DecoderBufferAllocator::Strategy {
 public:
  BidirectionalFitDecoderBufferAllocatorStrategy(size_t initial_capacity,
                                                 size_t allocation_increment)
      : fallback_allocator_(/*enable_decommit_on_idle=*/false),
        bidirectional_fit_allocator_(&fallback_allocator_,
                                     initial_capacity,
                                     kSmallAllocationThreshold,
                                     allocation_increment) {}

  // Constructs a strategy with explicit decommit configurations.
  // |enable_decommit_on_idle|: Whether to perform any decommits when idle.
  // |retain_blocks|: Number of blocks to keep fully committed when idle.
  // |conservative_decommit_blocks|: Number of blocks beyond retain blocks to
  // lazily decommit (e.g. using MADV_FREE if supported). Any blocks beyond
  // these are aggressively decommitted (e.g. using MADV_DONTNEED).
  // |aggressive_decommit_on_suspend|: Whether to aggressively decommit all idle
  // blocks when app is suspended.
  BidirectionalFitDecoderBufferAllocatorStrategy(
      size_t initial_capacity,
      size_t allocation_increment,
      bool enable_decommit_on_idle,
      size_t retain_blocks,
      size_t conservative_decommit_blocks,
      bool aggressive_decommit_on_suspend = false)
      : fallback_allocator_(enable_decommit_on_idle),
        bidirectional_fit_allocator_(&fallback_allocator_,
                                     initial_capacity,
                                     kSmallAllocationThreshold,
                                     allocation_increment,
                                     enable_decommit_on_idle,
                                     retain_blocks,
                                     conservative_decommit_blocks,
                                     aggressive_decommit_on_suspend) {}

  void* Allocate(DemuxerStream::Type type, size_t size) override {
    return bidirectional_fit_allocator_.Allocate(size);
  }
  void Free(DemuxerStream::Type type, void* p) override {
    bidirectional_fit_allocator_.Free(p);
  }
  void Write(void* p, const void* data, size_t size) override {
    memcpy(p, data, size);
  }

  size_t GetCapacity() const override {
    return bidirectional_fit_allocator_.GetCapacity();
  }

  size_t GetAllocated() const override {
    return bidirectional_fit_allocator_.GetAllocated();
  }

  void DecommitAllDecommitableBlocks() override {
    bidirectional_fit_allocator_.DecommitAllDecommitableBlocks();
  }

 private:
  // Used to determine if the memory allocated is large. The underlying logic
  // can be different.
  static constexpr size_t kSmallAllocationThreshold = 512;

  StarboardMemoryAllocator fallback_allocator_;
  starboard::BidirectionalFitReuseAllocator<ReuseAllocatorBase>
      bidirectional_fit_allocator_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_BIDIRECTIONAL_FIT_DECODER_BUFFER_ALLOCATOR_STRATEGY_H_

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

#include <algorithm>
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
      : fallback_allocator_(/*enable_decommit_on_idle=*/false,
                            /*enable_page_alignment=*/false),
        bidirectional_fit_allocator_(&fallback_allocator_,
                                     initial_capacity,
                                     kSmallAllocationThreshold,
                                     allocation_increment) {}

  // Constructs a strategy with explicit decommit configurations.
  explicit BidirectionalFitDecoderBufferAllocatorStrategy(
      const DecoderBufferAllocator::Strategy::ExperimentConfig& config)
      : fallback_allocator_(config.enable_decommit_on_idle ||
                                config.enable_decommit_only_on_memory_pressure,
                            config.allocate_with_page_alignment),
        bidirectional_fit_allocator_(
            &fallback_allocator_,
            config.initial_capacity,
            kSmallAllocationThreshold,
            config.allocation_increment,
            config.enable_decommit_on_idle,
            config.retain_blocks,
            config.conservative_decommit_blocks,
            config.aggressive_decommit_on_suspend,
            config.memset_on_reclaim,
            config.mark_as_cold_on_reclaim,
            config.enable_decommit_only_on_memory_pressure) {}

  void* Allocate(DemuxerStream::Type type, size_t size) override {
    return bidirectional_fit_allocator_.Allocate(size);
  }
  void Free(DemuxerStream::Type type, void* p) override {
    bidirectional_fit_allocator_.Free(p);
  }

  size_t GetCapacity() const override {
    return bidirectional_fit_allocator_.GetCapacity();
  }

  size_t GetAllocated() const override {
    return bidirectional_fit_allocator_.GetAllocated();
  }

  void SetSuspended(bool is_suspended) override {
    bidirectional_fit_allocator_.SetSuspended(is_suspended);
  }

  void DecommitAllDecommitableBlocks(bool include_retained_blocks) override {
    bidirectional_fit_allocator_.DecommitAllDecommitableBlocks(
        include_retained_blocks);
  }

  void TryToDecommitOneBlock() override {
    bidirectional_fit_allocator_.TryToDecommitOneBlock();
  }

 private:
  // Used to determine if the memory allocated is large. The underlying logic
  // can be different.
  static constexpr size_t kSmallAllocationThreshold = 512;

  StarboardMemoryAllocator fallback_allocator_;
  starboard::BidirectionalFitReuseAllocator<ReuseAllocatorBase>
      bidirectional_fit_allocator_;
};

// Isolated audio/video reuse allocator strategy used when
// CobaltMseMemoryOptimization is enabled: separates small long-lived audio
// frames from large video GOP slabs and releases empty slabs beyond the retain
// window back to the OS during active playback.
template <typename ReuseAllocatorBase>
class SplitBidirectionalFitDecoderBufferAllocatorStrategy
    : public DecoderBufferAllocator::Strategy {
 public:
  static constexpr size_t kAudioAllocationIncrement = 256 * 1024;

  SplitBidirectionalFitDecoderBufferAllocatorStrategy(
      size_t initial_capacity,
      size_t allocation_increment)
      : fallback_allocator_(/*enable_decommit=*/true,
                            /*enable_page_alignment=*/true),
        audio_allocator_(
            &fallback_allocator_,
            /*initial_capacity=*/0,
            kSmallAllocationThreshold,
            std::min(allocation_increment > 0 ? allocation_increment
                                              : kAudioAllocationIncrement,
                     kAudioAllocationIncrement),
            /*enable_decommit_on_idle=*/true,
            /*retain_blocks=*/1,
            /*conservative_decommit_blocks=*/0,
            /*aggressive_decommit_on_suspend=*/true,
            /*memset_on_reclaim=*/false,
            /*mark_as_cold_on_reclaim=*/false,
            /*enable_decommit_only_on_memory_pressure=*/false),
        video_allocator_(&fallback_allocator_,
                         initial_capacity,
                         kSmallAllocationThreshold,
                         allocation_increment,
                         /*enable_decommit_on_idle=*/true,
                         /*retain_blocks=*/2,
                         /*conservative_decommit_blocks=*/0,
                         /*aggressive_decommit_on_suspend=*/true,
                         /*memset_on_reclaim=*/false,
                         /*mark_as_cold_on_reclaim=*/false,
                         /*enable_decommit_only_on_memory_pressure=*/false) {
    audio_allocator_.set_release_empty_fallback_blocks(true);
    video_allocator_.set_release_empty_fallback_blocks(true);
  }

  void* Allocate(DemuxerStream::Type type, size_t size) override {
    if (type == DemuxerStream::AUDIO) {
      return audio_allocator_.Allocate(size);
    }
    return video_allocator_.Allocate(size);
  }

  void Free(DemuxerStream::Type type, void* p) override {
    if (type == DemuxerStream::AUDIO) {
      audio_allocator_.Free(p);
      return;
    }
    video_allocator_.Free(p);
  }

  size_t GetCapacity() const override {
    return audio_allocator_.GetCapacity() + video_allocator_.GetCapacity();
  }

  size_t GetAllocated() const override {
    return audio_allocator_.GetAllocated() + video_allocator_.GetAllocated();
  }

  void SetSuspended(bool is_suspended) override {
    audio_allocator_.SetSuspended(is_suspended);
    video_allocator_.SetSuspended(is_suspended);
  }

  void DecommitAllDecommitableBlocks(bool include_retained_blocks) override {
    audio_allocator_.DecommitAllDecommitableBlocks(include_retained_blocks);
    video_allocator_.DecommitAllDecommitableBlocks(include_retained_blocks);
  }

  void TryToDecommitOneBlock() override {
    audio_allocator_.TryToDecommitOneBlock();
    video_allocator_.TryToDecommitOneBlock();
  }

 private:
  static constexpr size_t kSmallAllocationThreshold = 512;

  StarboardMemoryAllocator fallback_allocator_;
  starboard::BidirectionalFitReuseAllocator<ReuseAllocatorBase>
      audio_allocator_;
  starboard::BidirectionalFitReuseAllocator<ReuseAllocatorBase>
      video_allocator_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_BIDIRECTIONAL_FIT_DECODER_BUFFER_ALLOCATOR_STRATEGY_H_

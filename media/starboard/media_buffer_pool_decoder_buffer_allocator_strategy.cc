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

#include "media/starboard/media_buffer_pool_decoder_buffer_allocator_strategy.h"

#include "base/logging.h"
#include "media/base/demuxer_stream.h"

namespace media {

using ::starboard::common::experimental::IsPointerAnnotated;

MediaBufferPoolDecoderBufferAllocatorStrategy::
    MediaBufferPoolDecoderBufferAllocatorStrategy(
        starboard::common::experimental::MediaBufferPool* media_buffer_pool,
        size_t video_buffer_initial_capacity,
        size_t video_buffer_allocation_increment)
    : media_buffer_pool_(media_buffer_pool),
      video_buffer_initial_capacity_(video_buffer_initial_capacity),
      video_buffer_allocation_increment_(video_buffer_allocation_increment),
      audio_allocator_(
          &audio_fallback_allocator_,
          // Pre-allocate sufficient capacity for audio buffers to
          // avoid expansions in most scenarios.
          SbMediaGetAudioBufferBudget() + kAudioAllocationIncrement,
          kSmallAllocationThreshold,
          kAudioAllocationIncrement),
      video_allocator_(new MediaBufferPoolBidirectionalReuseAllocator(
          media_buffer_pool_,
          video_buffer_initial_capacity,
          kSmallAllocationThreshold,
          video_buffer_allocation_increment)) {
  CHECK(media_buffer_pool_);
}

void* MediaBufferPoolDecoderBufferAllocatorStrategy::Allocate(
    DemuxerStream::Type type,
    size_t size,
    size_t alignment) {
  if (type == DemuxerStream::AUDIO) {
    return audio_allocator_.Allocate(size, alignment);
  }

#if !defined(OFFICIAL_BUILD)
  CHECK_EQ(type, DemuxerStream::VIDEO);
#endif  // !defined(OFFICIAL_BUILD)

  // The MediaBufferPoolMemoryAllocator handles pool expansion.
  return video_allocator_->Allocate(size, alignment);
}

void MediaBufferPoolDecoderBufferAllocatorStrategy::Free(
    DemuxerStream::Type type,
    void* p) {
  if (type == DemuxerStream::AUDIO) {
    audio_allocator_.Free(p);
  } else {
    video_allocator_->Free(p);
    if (video_allocator_->GetAllocated() == 0) {
      LOG(INFO)
          << "Allocated video buffers reached 0, re-creating the allocator ...";

      // We have to reset first so all underlying resources are reclaimed before
      // the new instance is created.
      video_allocator_.reset();
      video_allocator_.reset(new MediaBufferPoolBidirectionalReuseAllocator(
          media_buffer_pool_, video_buffer_initial_capacity_,
          kSmallAllocationThreshold, video_buffer_allocation_increment_));
    }
  }
}

void MediaBufferPoolDecoderBufferAllocatorStrategy::Write(void* p,
                                                          const void* data,
                                                          size_t size) {
  if (IsPointerAnnotated(p)) {
    // We send the annotated pointer directly to Write() as it expects an
    // annotated pointer.
    media_buffer_pool_->Write(reinterpret_cast<intptr_t>(p), data, size);
  } else {
    // Audio buffers are allocated from system memory.
    memcpy(p, data, size);
  }
}

size_t MediaBufferPoolDecoderBufferAllocatorStrategy::GetCapacity() const {
  return audio_allocator_.GetCapacity() + video_allocator_->GetCapacity();
}

size_t MediaBufferPoolDecoderBufferAllocatorStrategy::GetAllocated() const {
  return audio_allocator_.GetAllocated() + video_allocator_->GetAllocated();
}

}  // namespace media

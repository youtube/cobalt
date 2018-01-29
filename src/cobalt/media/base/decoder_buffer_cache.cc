// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/media/base/decoder_buffer_cache.h"

namespace cobalt {
namespace media {

DecoderBufferCache::DecoderBufferCache()
    : audio_buffer_index_(0), video_buffer_index_(0) {}

void DecoderBufferCache::AddBuffer(DemuxerStream::Type type,
                                   const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (type == DemuxerStream::AUDIO) {
    audio_buffers_.push_back(buffer);
    if (!buffer->end_of_stream() && buffer->is_key_frame()) {
      audio_key_frame_timestamps_.push_back(buffer->timestamp());
    }
  } else {
    DCHECK_EQ(type, DemuxerStream::VIDEO);
    video_buffers_.push_back(buffer);
    if (!buffer->end_of_stream() && buffer->is_key_frame()) {
      video_key_frame_timestamps_.push_back(buffer->timestamp());
    }
  }
}

void DecoderBufferCache::ClearSegmentsBeforeMediaTime(
    base::TimeDelta media_time) {
  DCHECK(thread_checker_.CalledOnValidThread());

  audio_buffer_index_ -= ClearSegmentsBeforeMediaTime(
      media_time, &audio_buffers_, &audio_key_frame_timestamps_);
  video_buffer_index_ -= ClearSegmentsBeforeMediaTime(
      media_time, &video_buffers_, &video_key_frame_timestamps_);
}

void DecoderBufferCache::ClearAll() {
  DCHECK(thread_checker_.CalledOnValidThread());

  audio_buffers_.clear();
  audio_key_frame_timestamps_.clear();
  video_buffers_.clear();
  video_key_frame_timestamps_.clear();
  audio_buffer_index_ = 0;
  video_buffer_index_ = 0;
}

void DecoderBufferCache::StartResuming() {
  DCHECK(thread_checker_.CalledOnValidThread());

  audio_buffer_index_ = 0;
  video_buffer_index_ = 0;
}

scoped_refptr<DecoderBuffer> DecoderBufferCache::GetBuffer(
    DemuxerStream::Type type) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (type == DemuxerStream::AUDIO) {
    if (audio_buffer_index_ < audio_buffers_.size()) {
      return audio_buffers_[audio_buffer_index_];
    }
    return NULL;
  }

  DCHECK_EQ(type, DemuxerStream::VIDEO);
  if (video_buffer_index_ < video_buffers_.size()) {
    return video_buffers_[video_buffer_index_];
  }
  return NULL;
}

void DecoderBufferCache::AdvanceToNextBuffer(DemuxerStream::Type type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (type == DemuxerStream::AUDIO) {
    ++audio_buffer_index_;
  } else {
    DCHECK_EQ(type, DemuxerStream::VIDEO);
    ++video_buffer_index_;
  }
}

// static
size_t DecoderBufferCache::ClearSegmentsBeforeMediaTime(
    base::TimeDelta media_time, Buffers* buffers,
    KeyFrameTimestamps* key_frame_timestamps) {
  // Use K to denote a key frame and N for non-key frame.  If the cache contains
  // K N N N N N N N N K N N N N N N N N K N N N N N N N N
  //                     |
  //                 media_time
  // Then we should remove everything before the key frame before the
  // |media_time| and turn the cache into:
  //                   K N N N N N N N N K N N N N N N N N
  //                     |
  //                 media_time
  // So we need at least two keyframes before we can remove any frames.
  while (key_frame_timestamps->size() > 1 &&
         key_frame_timestamps->at(1) <= media_time) {
    key_frame_timestamps->erase(key_frame_timestamps->begin());
  }
  if (key_frame_timestamps->empty()) {
    return 0;
  }

  size_t buffers_removed = 0;
  while (scoped_refptr<DecoderBuffer> buffer = buffers->front()) {
    if (buffer->is_key_frame() &&
        buffer->timestamp() == key_frame_timestamps->front()) {
      break;
    }
    buffers->pop_front();
    ++buffers_removed;
  }

  return buffers_removed;
}

}  // namespace media
}  // namespace cobalt

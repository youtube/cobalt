// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <algorithm>
#include <cstring>
#include <utility>

#include "base/logging.h"

namespace cobalt {
namespace media {
namespace {

bool SafeStrEqual(const char* ptr1, const char* ptr2) {
  if (!ptr1 || !ptr2) {
    return ptr1 == ptr2;
  }
  return strcmp(ptr1, ptr2) == 0;
}

}  // namespace

bool operator!=(const SbMediaAudioStreamInfo& left,
                const SbMediaAudioStreamInfo& right) {
  if (left.codec == kSbMediaAudioCodecNone) {
    return right.codec != kSbMediaAudioCodecNone;
  }
  return left.codec != right.codec || !SafeStrEqual(left.mime, right.mime) ||
         left.number_of_channels != right.number_of_channels ||
         left.samples_per_second != right.samples_per_second ||
         left.bits_per_sample != right.bits_per_sample ||
         left.audio_specific_config_size != right.audio_specific_config_size ||
         memcmp(left.audio_specific_config, right.audio_specific_config,
                left.audio_specific_config_size) != 0;
}

bool operator!=(const SbMediaVideoStreamInfo& left,
                const SbMediaVideoStreamInfo& right) {
  if (left.codec == kSbMediaVideoCodecNone) {
    return right.codec != kSbMediaVideoCodecNone;
  }
  return left.codec != right.codec || !SafeStrEqual(left.mime, right.mime) ||
         !SafeStrEqual(left.max_video_capabilities,
                       right.max_video_capabilities) ||
         left.frame_width != right.frame_width ||
         left.frame_height != right.frame_height ||
         memcmp(&left.color_metadata, &right.color_metadata,
                sizeof(SbMediaColorMetadata)) != 0;
}

DecoderBufferCache::BufferGroup::Segment::Segment(
    const SbMediaAudioStreamInfo& stream_info) {
  DCHECK_NE(stream_info.codec, kSbMediaAudioCodecNone);

  audio_stream_info = stream_info;
  if (stream_info.audio_specific_config_size > 0) {
    auto specific_config =
        static_cast<const uint8_t*>(stream_info.audio_specific_config);
    audio_specific_config.assign(
        specific_config,
        specific_config + stream_info.audio_specific_config_size);
    audio_stream_info.audio_specific_config = audio_specific_config.data();
  }
  if (stream_info.mime) {
    mime = stream_info.mime;
    audio_stream_info.mime = mime.c_str();
  }
}

DecoderBufferCache::BufferGroup::Segment::Segment(
    const SbMediaVideoStreamInfo& stream_info) {
  DCHECK_NE(stream_info.codec, kSbMediaVideoCodecNone);

  video_stream_info = stream_info;
  if (stream_info.mime) {
    mime = stream_info.mime;
    video_stream_info.mime = mime.c_str();
  }
  if (stream_info.max_video_capabilities) {
    max_video_capabilities = stream_info.max_video_capabilities;
    video_stream_info.max_video_capabilities = max_video_capabilities.c_str();
  }
}

DecoderBufferCache::BufferGroup::BufferGroup(DemuxerStream::Type type)
    : type_(type) {}

void DecoderBufferCache::BufferGroup::AddBuffers(
    const DecoderBuffers& buffers, const SbMediaAudioStreamInfo& stream_info) {
  DCHECK(type_ == DemuxerStream::Type::AUDIO);
  if (buffers.empty()) {
    return;
  }
  if (segments_.empty() || segments_.back().audio_stream_info != stream_info) {
    AddStreamInfo(stream_info);
  }
  AddBuffers(buffers);
}

void DecoderBufferCache::BufferGroup::AddBuffers(
    const DecoderBuffers& buffers, const SbMediaVideoStreamInfo& stream_info) {
  DCHECK(type_ == DemuxerStream::Type::VIDEO);
  if (buffers.empty()) {
    return;
  }
  if (segments_.empty() || segments_.back().video_stream_info != stream_info) {
    AddStreamInfo(stream_info);
  }
  AddBuffers(buffers);
}

void DecoderBufferCache::BufferGroup::ClearSegmentsBeforeMediaTime(
    const base::TimeDelta& media_time) {
  // Use K to denote a key frame and N for non-key frame.  If the cache contains
  // K N N N N N N N N K N N N N N N N N K N N N N N N N N
  //                     |
  //                 media_time
  // Then we should remove everything before the key frame before the
  // |media_time| and turn the cache into:
  //                   K N N N N N N N N K N N N N N N N N
  //                     |
  //                 media_time
  // So we need at least two keyframes before we can remove any buffers.
  while (key_frame_timestamps_.size() > 1 &&
         key_frame_timestamps_[1] <= media_time) {
    key_frame_timestamps_.pop_front();
  }

  if (key_frame_timestamps_.empty()) {
    return;
  }

  base::TimeDelta key_frame_timestamp = key_frame_timestamps_.front();
  // Remove all buffers and groups before the key frame timestamp.
  while (!segments_.empty()) {
    auto& first_segment_buffers = segments_.front().buffers;
    while (!first_segment_buffers.empty()) {
      auto& buffer = first_segment_buffers.front();
      if (buffer->is_key_frame() &&
          buffer->timestamp() == key_frame_timestamp) {
        return;
      }
      first_segment_buffers.pop_front();
      if (segment_index_ == 0) {
        if (buffer_index_ == 0) {
          LOG(ERROR) << "Trying to remove unwritten buffers.";
        } else {
          buffer_index_--;
        }
      }
    }

    if (first_segment_buffers.empty()) {
      DCHECK_GT(segments_.size(), 1);
      DCHECK_GT(segment_index_, 0);

      segments_.pop_front();
      segment_index_--;
    }
  }
  DCHECK(!segments_.empty() && segment_index_ < segments_.size());
  DCHECK(buffer_index_ < segments_[segment_index_].buffers.size());
}


void DecoderBufferCache::BufferGroup::ClearAll() {
  segments_.clear();
  key_frame_timestamps_.clear();
  segment_index_ = 0;
  buffer_index_ = 0;
}

bool DecoderBufferCache::BufferGroup::HasMoreBuffers() const {
  if (segments_.size() == 0) {
    return false;
  }
  DCHECK(segment_index_ < segments_.size());
  DCHECK(buffer_index_ <= segments_[segment_index_].buffers.size());
  if (buffer_index_ == segments_[segment_index_].buffers.size()) {
    DCHECK(segment_index_ == segments_.size() - 1);
    return false;
  }
  return true;
}

template <typename StreamInfo>
void DecoderBufferCache::BufferGroup::AddStreamInfo(
    const StreamInfo& stream_info) {
  segments_.emplace_back(stream_info);
  AdvanceGroupIndexIfNecessary();
}

void DecoderBufferCache::BufferGroup::AddBuffers(
    const DecoderBuffers& buffers) {
  DCHECK(!segments_.empty() && segment_index_ < segments_.size());
  DCHECK(!buffers.empty());
  auto& last_segment_buffers = segments_.back().buffers;
  for (auto buffer : buffers) {
    if (!buffer->end_of_stream() && buffer->is_key_frame()) {
      key_frame_timestamps_.push_back(buffer->timestamp());
    }
  }
  last_segment_buffers.insert(last_segment_buffers.end(), buffers.begin(),
                              buffers.end());
}

void DecoderBufferCache::BufferGroup::ReadBuffers(
    DecoderBuffers* buffers, size_t max_buffers_per_read,
    SbMediaAudioStreamInfo* stream_info) {
  DCHECK_EQ(type_, DemuxerStream::AUDIO);
  DCHECK_GT(max_buffers_per_read, 0);
  DCHECK(HasMoreBuffers());

  *stream_info = segments_[segment_index_].audio_stream_info;
  ReadBuffers(buffers, max_buffers_per_read);
}

void DecoderBufferCache::BufferGroup::ReadBuffers(
    DecoderBuffers* buffers, size_t max_buffers_per_read,
    SbMediaVideoStreamInfo* stream_info) {
  DCHECK_EQ(type_, DemuxerStream::VIDEO);
  DCHECK_GT(max_buffers_per_read, 0);
  DCHECK(HasMoreBuffers());

  *stream_info = segments_[segment_index_].video_stream_info;
  ReadBuffers(buffers, max_buffers_per_read);
}

void DecoderBufferCache::BufferGroup::ResetIndex() {
  segment_index_ = 0;
  buffer_index_ = 0;
}

void DecoderBufferCache::BufferGroup::AdvanceGroupIndexIfNecessary() {
  DCHECK(segment_index_ < segments_.size());
  DCHECK(buffer_index_ <= segments_[segment_index_].buffers.size());
  if (buffer_index_ == segments_[segment_index_].buffers.size() &&
      segment_index_ + 1 < segments_.size()) {
    segment_index_++;
    buffer_index_ = 0;
  }
}

void DecoderBufferCache::BufferGroup::ReadBuffers(DecoderBuffers* buffers,
                                                  size_t max_buffers_per_read) {
  DCHECK(segment_index_ < segments_.size());
  DCHECK(buffer_index_ < segments_[segment_index_].buffers.size());

  auto& current_segment_buffers = segments_[segment_index_].buffers;
  size_t buffers_to_read = std::min(
      max_buffers_per_read, current_segment_buffers.size() - buffer_index_);
  DCHECK_GT(buffers_to_read, 0);

  buffers->insert(
      buffers->end(), current_segment_buffers.begin() + buffer_index_,
      current_segment_buffers.begin() + buffer_index_ + buffers_to_read);
  buffer_index_ += buffers_to_read;

  AdvanceGroupIndexIfNecessary();
}

DecoderBufferCache::DecoderBufferCache()
    : audio_buffer_group_(DemuxerStream::Type::AUDIO),
      video_buffer_group_(DemuxerStream::Type::VIDEO) {}

void DecoderBufferCache::AddBuffers(const DecoderBuffers& buffers,
                                    const SbMediaAudioStreamInfo& stream_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_buffer_group_.AddBuffers(buffers, stream_info);
}

void DecoderBufferCache::AddBuffers(const DecoderBuffers& buffers,
                                    const SbMediaVideoStreamInfo& stream_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  video_buffer_group_.AddBuffers(buffers, stream_info);
}

void DecoderBufferCache::ClearSegmentsBeforeMediaTime(
    const base::TimeDelta& media_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_buffer_group_.ClearSegmentsBeforeMediaTime(media_time);
  video_buffer_group_.ClearSegmentsBeforeMediaTime(media_time);
}

void DecoderBufferCache::ClearAll() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_buffer_group_.ClearAll();
  video_buffer_group_.ClearAll();
}

void DecoderBufferCache::StartResuming() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_buffer_group_.ResetIndex();
  video_buffer_group_.ResetIndex();
}

bool DecoderBufferCache::HasMoreBuffers(DemuxerStream::Type type) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (type == DemuxerStream::Type::AUDIO) {
    return audio_buffer_group_.HasMoreBuffers();
  }
  DCHECK(type == DemuxerStream::Type::VIDEO);
  return video_buffer_group_.HasMoreBuffers();
}

void DecoderBufferCache::ReadBuffers(DecoderBuffers* buffers,
                                     size_t max_buffers_per_read,
                                     SbMediaAudioStreamInfo* stream_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(buffers);
  DCHECK(buffers->empty());
  DCHECK(stream_info);

  if (max_buffers_per_read == 0 || !audio_buffer_group_.HasMoreBuffers()) {
    return;
  }
  audio_buffer_group_.ReadBuffers(buffers, max_buffers_per_read, stream_info);
  DCHECK(!buffers->empty());
}

void DecoderBufferCache::ReadBuffers(DecoderBuffers* buffers,
                                     size_t max_buffers_per_read,
                                     SbMediaVideoStreamInfo* stream_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(buffers);
  DCHECK(buffers->empty());
  CHECK(stream_info);

  if (max_buffers_per_read == 0 || !video_buffer_group_.HasMoreBuffers()) {
    return;
  }
  video_buffer_group_.ReadBuffers(buffers, max_buffers_per_read, stream_info);
  DCHECK(!buffers->empty());
}

}  // namespace media
}  // namespace cobalt

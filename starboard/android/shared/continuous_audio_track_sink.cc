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

#include "starboard/android/shared/continuous_audio_track_sink.h"

#include <unistd.h>
#include <algorithm>
#include <string>
#include <vector>

#include <math.h>
#include "starboard/android/shared/audio_stream.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/shared/pthread/thread_create_priority.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/common.h"

namespace starboard::android::shared {
namespace {

using ::starboard::shared::starboard::media::GetBytesPerSample;

// The maximum number of frames that can be written to android audio track per
// write request. If we don't set this cap for writing frames to audio track,
// we will repeatedly allocate a large byte array which cannot be consumed by
// audio track completely.
const int kMaxFramesPerRequest = 64 * 1024;

constexpr int kSilenceFramesPerAppend = 1024;
static_assert(kSilenceFramesPerAppend <= kMaxFramesPerRequest);

void* IncrementPointerByBytes(void* pointer, size_t offset) {
  return static_cast<uint8_t*>(pointer) + offset;
}

#define LOG_ELAPSED(op)                                                 \
  do {                                                                  \
    int64_t start = CurrentMonotonicTime();                             \
    op;                                                                 \
    int64_t end = CurrentMonotonicTime();                               \
    SB_LOG(INFO) << #op << ": elapsed(msec)=" << (end - start) / 1'000; \
  } while (0)

}  // namespace

ContinuousAudioTrackSink::ContinuousAudioTrackSink(
    Type* type,
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType sample_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    int preferred_buffer_size_in_bytes,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    ConsumeFramesFunc consume_frames_func,
    SbAudioSinkPrivate::ErrorFunc error_func,
    int64_t start_time,
    bool is_web_audio,
    void* context)
    : type_(type),
      channels_(channels),
      sampling_frequency_hz_(sampling_frequency_hz),
      frame_bytes_(GetBytesPerSample(sample_type) * channels_),
      frame_buffer_(static_cast<uint8_t*>(frame_buffers[0])),
      frames_per_channel_(frames_per_channel),
      update_source_status_func_(update_source_status_func),
      consume_frames_func_(consume_frames_func),
      error_func_(error_func),
      start_time_(start_time),
      context_(context),
      stream_(AudioStream::Create(sample_type,
                                  channels,
                                  sampling_frequency_hz,
                                  frames_per_channel,
                                  [this](void* data, int num_frames) {
                                    return OnReadData(data, num_frames);
                                  })) {
  SB_DCHECK(stream_ != nullptr);
  SB_DCHECK(update_source_status_func_);
  SB_DCHECK(consume_frames_func_);
  SB_DCHECK(frame_buffer_);

  SB_LOG(INFO) << "Creating continuous audio sink: start_time=" << start_time_
               << ", source_buffer_size(frames)=" << frames_per_channel_
               << ", source_buffer_size(msec)="
               << (frames_per_channel_ * 1'000 / sampling_frequency_hz_)
               << ", frame_bytes=" << frame_bytes_
               << ", preferred_buffer_size_in_bytes="
               << preferred_buffer_size_in_bytes << ", preferred_frames="
               << (preferred_buffer_size_in_bytes /
                   GetBytesPerSample(sample_type) / channels_);
}

ContinuousAudioTrackSink::~ContinuousAudioTrackSink() = default;

void ContinuousAudioTrackSink::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(playback_rate >= 0.0);
  if (playback_rate != 0.0 && playback_rate != 1.0) {
    SB_NOTIMPLEMENTED() << "TODO: Only playback rates of 0.0 and 1.0 are "
                           "currently supported.";
    playback_rate = (playback_rate > 0.0) ? 1.0 : 0.0;
  }
  ScopedLock lock(mutex_);
  if (playback_rate > 0.5) {
    SB_LOG(INFO) << __func__
                 << ": Call play with playback_rate=" << playback_rate;
    stream_->Play();
    is_playing_ = true;
    last_playback_start_us_ = CurrentMonotonicTime();
  } else {
    SB_LOG(INFO) << __func__
                 << ": Call pause with playback_rate=" << playback_rate;
    stream_->Pause();
    is_playing_ = false;
  }
}

int GetPlayedSilenceFrames(std::vector<std::pair<int, int>>& silence_frames,
                           int played_frame_offset) {
  int played_silence_frames_sum = 0;

  int index = 0;
  while (!silence_frames.empty()) {
    auto& [offset, length] = silence_frames.front();
#if 0
    SB_LOG(INFO) << "index=" << index++
                 << ", played_frame_offset=" << played_frame_offset
                 << ", offset=" << offset << ", length=" << length;
#endif
    if (played_frame_offset < offset) {
      return played_silence_frames_sum;
    }
    if (played_frame_offset < offset + length) {
      const int played_silence_frames = played_frame_offset - offset;
      SB_DCHECK(played_silence_frames >= 0)
          << "played_silence_frames=" << played_silence_frames;
      played_silence_frames_sum += played_silence_frames;

      offset += played_silence_frames;
      length -= played_silence_frames;
      return played_silence_frames_sum;
    }

    played_silence_frames_sum += length;
    silence_frames.erase(silence_frames.begin());
  }
  return played_silence_frames_sum;
}

AudioStream::Timestamp GetEstimatedNowTimestamp(
    AudioStream::Timestamp timestamp,
    int sample_rate) {
  int64_t now_us = CurrentMonotonicTime();
  int64_t elapsed_us = now_us - timestamp.system_time_us;
  SB_DCHECK(elapsed_us >= 0);

  int64_t frame_position =
      timestamp.frame_position + elapsed_us * sample_rate / 1'000'000;
  return {
      .frame_position = frame_position,
      .system_time_us = now_us,
  };
}

void ContinuousAudioTrackSink::ReportPlayedFrames() {
  auto latest_timestamp = stream_->GetTimestamp();
  if (!latest_timestamp.has_value()) {
    return;
  }
  AudioStream::Timestamp timestamp = [&] {
    if (!is_playing_) {
      SB_LOG(INFO) << "not playing: use latest timestamp";
      return *latest_timestamp;
    }
    if (latest_timestamp->system_time_us < last_playback_start_us_) {
      SB_LOG(INFO) << "timestamp is before playback starts";
      return *latest_timestamp;
    }
    return GetEstimatedNowTimestamp(*latest_timestamp, sampling_frequency_hz_);
  }();

  int frames_consumed =
      timestamp.frame_position - last_timestamp_->frame_position;
  if (frames_consumed <= 0) {
    return;
  }
  SB_DCHECK(frames_consumed >= 0) << "new_timestamp: " << timestamp
                                  << ", last_timestamp: " << *last_timestamp_;
  last_timestamp_ = timestamp;

  const int played_silence_frames =
      GetPlayedSilenceFrames(silence_frames_, timestamp.frame_position);

  SB_DCHECK(frames_consumed >= played_silence_frames)
      << "frames_consumed=" << frames_consumed
      << ", played_silence_frames=" << played_silence_frames;
  frames_consumed -= played_silence_frames;

  SB_DCHECK(pending_frames_in_platform_buffer_ >= frames_consumed)
      << "pending_frames_in_platform_buffer_="
      << pending_frames_in_platform_buffer_
      << ", frames_consumed=" << frames_consumed;
  pending_frames_in_platform_buffer_ -= frames_consumed;

  consume_frames_func_(frames_consumed, timestamp.system_time_us, context_);
}

bool ContinuousAudioTrackSink::OnReadData(void* data, int num_frames_to_read) {
  ReportPlayedFrames();

  return ReadMoreFrames(data, num_frames_to_read);
}

bool ContinuousAudioTrackSink::ReadMoreFrames(void* data,
                                              int num_frames_to_read) {
  const int frame_buffer_size = frames_per_channel_;
  int source_available_frames;
  int frame_offset;
  bool is_playing;
  bool is_eos_reached;
  update_source_status_func_(&source_available_frames, &frame_offset,
                             &is_playing, &is_eos_reached, context_);
  SB_CHECK(source_available_frames >= pending_frames_in_platform_buffer_)
      << "source_availabe_frames=" << source_available_frames
      << ", pending_frames_in_platform_buffer="
      << pending_frames_in_platform_buffer_;

  frame_offset =
      (frame_offset + pending_frames_in_platform_buffer_) % frame_buffer_size;
  const int available_frames =
      std::max(0, source_available_frames - pending_frames_in_platform_buffer_);
  if (available_frames == 0) {
    SB_LOG(INFO) << "Feed silence frames: num_frames_to_read="
                 << num_frames_to_read
                 << ", available_frames=" << available_frames
                 << ", source_available_frames=" << source_available_frames
                 << ", pending_frames_in_platform_buffer="
                 << pending_frames_in_platform_buffer_;

    silence_frames_.push_back({frame_offset_, num_frames_to_read});
    frame_offset_ += num_frames_to_read;

    return is_eos_reached ? false : true;
  }

  int frames_read = 0;
  const int available_frames_to_read =
      std::min(num_frames_to_read, available_frames);
  const int first_chunk_frames =
      std::min(available_frames_to_read, frame_buffer_size - frame_offset);
  memcpy(data, frame_buffer_ + frame_offset * frame_bytes_,
         first_chunk_frames * frame_bytes_);
  frames_read += first_chunk_frames;

  if (frames_read < available_frames_to_read) {
    const int remaining_frames = available_frames_to_read - frames_read;
    memcpy(static_cast<uint8_t*>(data) + frames_read * frame_bytes_,
           frame_buffer_, remaining_frames * frame_bytes_);
  }
  pending_frames_in_platform_buffer_ += frames_read;
  SB_CHECK(pending_frames_in_platform_buffer_ <= frame_buffer_size)
      << "pending_frames_in_platform_buffer_="
      << pending_frames_in_platform_buffer_
      << ", frame_buffer_size=" << frame_buffer_size;

  SB_DCHECK(frames_read <= num_frames_to_read)
      << ": frames_read=" << frames_read
      << ", num_frames_to_read=" << num_frames_to_read;

  const int underrun_frames = num_frames_to_read - frames_read;
  if (underrun_frames > 0) {
    memset(static_cast<uint8_t*>(data) + frames_read * frame_bytes_, 0,
           underrun_frames * frame_bytes_);

    silence_frames_.push_back({frame_offset_ + frames_read, underrun_frames});
    SB_LOG(INFO) << "Underrun: underrun frames=" << underrun_frames
                 << ", frames_read=" << frames_read
                 << ", num_frames_to_read=" << num_frames_to_read
                 << ", available_frames=" << available_frames
                 << ", source_available_frames=" << source_available_frames
                 << ", pending_frames_in_platform_buffer_="
                 << pending_frames_in_platform_buffer_;
  }
  frame_offset_ += num_frames_to_read;

  return true;
}

void ContinuousAudioTrackSink::ReportError(bool capability_changed,
                                           const std::string& error_message) {
  SB_LOG(INFO) << "ContinuousAudioTrackSink error: " << error_message;
  if (error_func_) {
    error_func_(capability_changed, error_message, context_);
  }
}

int64_t ContinuousAudioTrackSink::GetFramesDurationUs(int frames) const {
  return frames * 1'000'000LL / sampling_frequency_hz_;
}

void ContinuousAudioTrackSink::SetVolume(double volume) {
  SB_LOG(WARNING) << __func__ << ": not implemented: volume=" << volume;
}

int ContinuousAudioTrackSink::GetUnderrunCount() {
  return stream_->GetUnderrunCount();
}

int ContinuousAudioTrackSink::GetStartThresholdInFrames() {
  return stream_->GetFramesPerBurst();
}

}  // namespace starboard::android::shared

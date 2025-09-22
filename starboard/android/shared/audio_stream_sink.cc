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

#include "starboard/android/shared/audio_stream_sink.h"

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

using ::starboard::shared::starboard::media::GetBytesPerSample;

AudioStreamSink::AudioStreamSink(
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
    // TODO: b/428008986 - Use is_web_audio when creating AudioStream.
    bool is_web_audio,
    void* context)
    : type_(type),
      channels_(channels),
      sample_rate_(sampling_frequency_hz),
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
               << (frames_per_channel_ * 1'000 / sample_rate_)
               << ", frame_bytes=" << frame_bytes_
               << ", preferred_buffer_size_in_bytes="
               << preferred_buffer_size_in_bytes << ", preferred_frames="
               << (preferred_buffer_size_in_bytes /
                   GetBytesPerSample(sample_type) / channels_);
}

AudioStreamSink::~AudioStreamSink() {
  SB_LOG(INFO) << "Destroying AudioStreamSink";
}

AudioStreamSink::SourceState AudioStreamSink::GetSourceState() {
  int available_frames;
  int frame_offset;
  bool is_playing;
  bool is_eos_reached;
  update_source_status_func_(&available_frames, &frame_offset, &is_playing,
                             &is_eos_reached, context_);
  return {
      .available_frames = available_frames,
      .frame_offset = frame_offset,
      .is_playing = is_playing,
      .is_eos_reached = is_eos_reached,
  };
}

void AudioStreamSink::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(playback_rate >= 0.0);
  if (playback_rate != 0.0 && playback_rate != 1.0) {
    SB_NOTIMPLEMENTED() << "TODO: Only playback rates of 0.0 and 1.0 are "
                           "currently supported.";
    playback_rate = (playback_rate > 0.0) ? 1.0 : 0.0;
  }
  if (playback_rate > 0.5) {
    SB_LOG(INFO) << __func__
                 << ": Call play with playback_rate=" << playback_rate;
    stream_->Play();
  } else {
    SB_LOG(INFO) << __func__
                 << ": Call pause with playback_rate=" << playback_rate;
    stream_->Pause();
  }
  SB_LOG(INFO) << __func__ << ": completed";
}

void AudioStreamSink::ReportPlayedFrames() {
  auto latest_timestamp = stream_->GetTimestamp();
  if (!latest_timestamp.has_value()) {
    return;
  }

  const AudioStream::Timestamp& timestamp = *latest_timestamp;

  auto SanitizePosition = [&](int64_t frame_position) {
    return std::max<int64_t>(0, frame_position - initial_silence_frames_);
  };

  int frames_consumed = SanitizePosition(timestamp.frame_position) -
                        SanitizePosition(last_timestamp_.frame_position);
  if (frames_consumed <= 0) {
    return;
  }

  SB_DCHECK(frames_consumed >= 0) << "new_timestamp: " << timestamp
                                  << ", last_timestamp: " << last_timestamp_;
  last_timestamp_ = timestamp;

  SB_DCHECK(pending_frames_in_platform_buffer_ >= frames_consumed)
      << "pending_frames_in_platform_buffer_="
      << pending_frames_in_platform_buffer_
      << ", frames_consumed=" << frames_consumed;
  pending_frames_in_platform_buffer_ -= frames_consumed;

  consume_frames_func_(frames_consumed, timestamp.system_time_us, context_);
}

bool AudioStreamSink::OnReadData(void* data, int num_frames_to_read) {
  ReportPlayedFrames();

  return ReadMoreFrames(data, num_frames_to_read);
}

bool AudioStreamSink::ReadMoreFrames(void* data, int num_frames_to_read) {
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
  if (available_frames == 0 && is_eos_reached) {
    return false;
  }

  if (!data_has_arrived_) {
    if (available_frames > 0) {
      SB_LOG(INFO) << "First batch of data arrived: frames="
                   << available_frames;
      data_has_arrived_ = true;
    } else {
      initial_silence_frames_ += num_frames_to_read;
    }
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
    frames_read += remaining_frames;
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
    if (data_has_arrived_) {
      SB_LOG(INFO) << "Underrun: underrun frames=" << underrun_frames
                   << ", frames_read=" << frames_read
                   << ", num_frames_to_read=" << num_frames_to_read
                   << ", available_frames=" << available_frames
                   << ", source_available_frames=" << source_available_frames
                   << ", pending_frames_in_platform_buffer_="
                   << pending_frames_in_platform_buffer_;
    }
  }

  return true;
}

void AudioStreamSink::ReportError(bool capability_changed,
                                  const std::string& error_message) {
  SB_LOG(INFO) << "AudioStreamSink error: " << error_message;
  if (error_func_) {
    error_func_(capability_changed, error_message, context_);
  }
}

void AudioStreamSink::SetVolume(double volume) {
  SB_LOG(WARNING) << __func__ << ": not implemented: volume=" << volume;
}

int AudioStreamSink::GetUnderrunCount() {
  return stream_->GetUnderrunCount();
}

int AudioStreamSink::GetStartThresholdInFrames() {
  return stream_->GetFramesPerBurst();
}

}  // namespace starboard::android::shared

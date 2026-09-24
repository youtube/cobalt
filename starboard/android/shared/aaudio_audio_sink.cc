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

#include "starboard/android/shared/aaudio_audio_sink.h"

#include <android/api-level.h>

#include <algorithm>
#include <cstring>
#include <utility>

#include "starboard/android/shared/aaudio_loader.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

namespace starboard {
namespace {

aaudio_data_callback_result_t AudioDataCallback(AAudioStream* /*stream*/,
                                                void* user_data,
                                                void* audio_data,
                                                int32_t num_frames) {
  auto* sink = static_cast<AaudioAudioSink*>(user_data);
  return sink->OnAudioData(audio_data, num_frames);
}

void AudioErrorCallback(AAudioStream* /*stream*/,
                        void* user_data,
                        aaudio_result_t error) {
  auto* sink = static_cast<AaudioAudioSink*>(user_data);
  sink->OnAudioError(error);
}

struct AAudioStreamBuilderDeleter {
  void operator()(AAudioStreamBuilder* builder) const {
    if (!builder) {
      return;
    }
    AAudio::StreamBuilder_Delete(builder);
  }
};

}  // namespace

void AaudioAudioSink::AAudioStreamDeleter::operator()(
    AAudioStream* stream) const {
  if (!stream) {
    return;
  }

  AAudio::Stream_RequestStop(stream);
  AAudio::Stream_Close(stream);
}

// static
bool AaudioAudioSink::IsSupported(SbMediaAudioSampleType sample_type) {
  return sample_type == kSbMediaAudioSampleTypeFloat32 &&
         android_get_device_api_level() >= 28 && AAudio::Load();
}

// static
std::unique_ptr<AaudioAudioSink> AaudioAudioSink::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType sample_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    Callbacks callbacks,
    bool is_web_audio,
    void* context) {
  if (sample_type != kSbMediaAudioSampleTypeFloat32) {
    SB_LOG(WARNING) << "AaudioAudioSink only supports Float32 sample type.";
    return nullptr;
  }
  if (!AAudio::Load()) {
    SB_LOG(WARNING) << "AAudio library is not available.";
    return nullptr;
  }

  AAudioStreamBuilder* raw_builder = nullptr;
  if (aaudio_result_t result = AAudio::CreateStreamBuilder(&raw_builder);
      result != AAUDIO_OK || !raw_builder) {
    SB_LOG(ERROR) << "Failed to create AAudioStreamBuilder: "
                  << AAudio::ConvertResultToText(result);
    return nullptr;
  }
  std::unique_ptr<AAudioStreamBuilder, AAudioStreamBuilderDeleter> builder(
      raw_builder);

  AAudio::StreamBuilder_SetDirection(builder.get(), AAUDIO_DIRECTION_OUTPUT);
  AAudio::StreamBuilder_SetSampleRate(builder.get(), sampling_frequency_hz);
  AAudio::StreamBuilder_SetChannelCount(builder.get(), channels);
  AAudio::StreamBuilder_SetFormat(builder.get(), AAUDIO_FORMAT_PCM_FLOAT);
  AAudio::StreamBuilder_SetSharingMode(builder.get(),
                                       AAUDIO_SHARING_MODE_SHARED);
  AAudio::StreamBuilder_SetPerformanceMode(builder.get(),
                                           AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudio::StreamBuilder_SetUsage(
      builder.get(), is_web_audio ? AAUDIO_USAGE_GAME : AAUDIO_USAGE_MEDIA);
  AAudio::StreamBuilder_SetContentType(builder.get(),
                                       AAUDIO_CONTENT_TYPE_MUSIC);

  auto sink = std::make_unique<AaudioAudioSink>(
      PassKey<AaudioAudioSink>(), channels, frame_buffers, frames_per_channel,
      callbacks, context);

  AAudio::StreamBuilder_SetDataCallback(builder.get(), AudioDataCallback,
                                        sink.get());
  if (AAudio::StreamBuilder_SetErrorCallback) {
    AAudio::StreamBuilder_SetErrorCallback(builder.get(), AudioErrorCallback,
                                           sink.get());
  }

  AAudioStream* raw_stream = nullptr;
  if (aaudio_result_t result =
          AAudio::StreamBuilder_OpenStream(builder.get(), &raw_stream);
      result != AAUDIO_OK || !raw_stream) {
    SB_LOG(ERROR) << "Failed to open AAudioStream: "
                  << AAudio::ConvertResultToText(result);
    return nullptr;
  }

  sink->stream_.reset(raw_stream);

  int32_t burst_frames = AAudio::Stream_GetFramesPerBurst(raw_stream);
  if (burst_frames > 0) {
    AAudio::Stream_SetBufferSizeInFrames(raw_stream, burst_frames * 2);
  }

  if (aaudio_result_t result = AAudio::Stream_RequestStart(raw_stream);
      result != AAUDIO_OK) {
    SB_LOG(ERROR) << "Failed to start AAudioStream: "
                  << AAudio::ConvertResultToText(result);
    return nullptr;
  }

  SB_LOG(INFO) << "AaudioAudioSink (Pull Mode) created: channels=" << channels
               << ", rate=" << sampling_frequency_hz
               << ", buffer_size=" << frames_per_channel
               << ", burst=" << burst_frames;
  return sink;
}

AaudioAudioSink::AaudioAudioSink(PassKey<AaudioAudioSink>,
                                 int channels,
                                 SbAudioSinkFrameBuffers frame_buffers,
                                 int frames_per_channel,
                                 Callbacks callbacks,
                                 void* context)
    : channels_(channels),
      frame_buffer_(frame_buffers[0]),
      frames_per_channel_(frames_per_channel),
      callbacks_(callbacks),
      context_(context) {
  SB_CHECK(callbacks_.update_source_status);
  SB_CHECK(callbacks_.consume_frames);
  SB_CHECK(frame_buffer_);
}

AaudioAudioSink::~AaudioAudioSink() {
  quit_.store(true, std::memory_order_release);
  if (stream_) {
    // Calling Stream_Close blocks until the AAudio callback thread has fully
    // terminated.
    stream_.reset();
  }
}

bool AaudioAudioSink::IsType(Type* type) {
  return type == SbAudioSinkImpl::GetPrimaryType();
}

void AaudioAudioSink::SetPlaybackRate(double playback_rate) {
  // AaudioAudioSink only supports 0.0 (pause) and 1.0 (play). Variable playback
  // rates (e.g. 0.5x, 1.25x, 2.0x) are time-stretched upstream by
  // AudioRendererPcm using Sonic since AllowDirectPlaybackRateSetting() returns
  // false.
  SB_CHECK(playback_rate == 0.0 || playback_rate == 1.0);
  playback_rate_.store(static_cast<float>(playback_rate),
                       std::memory_order_relaxed);
}

void AaudioAudioSink::SetVolume(double volume) {
  volume_.store(static_cast<float>(volume), std::memory_order_relaxed);
}

bool AaudioAudioSink::Flush() {
  // Acquire |flush_mutex_| to wait for any in-flight OnAudioData() callback.
  std::lock_guard lock(flush_mutex_);
  flush_requested_.store(true, std::memory_order_relaxed);
  return true;
}

void AaudioAudioSink::SetStartTime(int64_t /*start_time_us*/) {}

aaudio_data_callback_result_t AaudioAudioSink::OnAudioData(void* audio_data,
                                                           int32_t num_frames) {
  if (quit_.load(std::memory_order_acquire)) {
    return AAUDIO_CALLBACK_RESULT_STOP;
  }

  // If we don't acquire |flush_mutex_| (Flush() is being called) or a flush was
  // just requested, output silence and skip consume_frames() to avoid consuming
  // stale pre-seek frames.
  std::unique_lock lock(flush_mutex_, std::try_to_lock);
  if (!lock.owns_lock() ||
      flush_requested_.exchange(false, std::memory_order_relaxed)) {
    std::memset(audio_data, 0, num_frames * channels_ * sizeof(float));
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  int frames_in_buffer = 0;
  int offset_in_frames = 0;
  bool is_playing = false;
  bool is_eos_reached = false;

  callbacks_.update_source_status(&frames_in_buffer, &offset_in_frames,
                                  &is_playing, &is_eos_reached, context_);

  float rate = playback_rate_.load(std::memory_order_relaxed);
  if (rate == 0.0f) {
    is_playing = false;
  }

  if (!is_playing || frames_in_buffer <= 0) {
    std::memset(audio_data, 0, num_frames * channels_ * sizeof(float));
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  int frames_to_copy = std::min(static_cast<int>(num_frames), frames_in_buffer);
  float* dest = static_cast<float*>(audio_data);
  const float* src = static_cast<const float*>(frame_buffer_);
  float volume = volume_.load(std::memory_order_relaxed);

  int first_chunk_frames =
      std::min(frames_to_copy, frames_per_channel_ - offset_in_frames);
  int second_chunk_frames = frames_to_copy - first_chunk_frames;

  const float* src_ptr = src + offset_in_frames * channels_;
  int first_chunk_samples = first_chunk_frames * channels_;
  if (volume == 1.0f) {
    std::memcpy(dest, src_ptr, first_chunk_samples * sizeof(float));
  } else {
    for (int i = 0; i < first_chunk_samples; ++i) {
      dest[i] = src_ptr[i] * volume;
    }
  }

  if (second_chunk_frames > 0) {
    float* dest_second = dest + first_chunk_samples;
    int second_chunk_samples = second_chunk_frames * channels_;
    if (volume == 1.0f) {
      std::memcpy(dest_second, src, second_chunk_samples * sizeof(float));
    } else {
      for (int i = 0; i < second_chunk_samples; ++i) {
        dest_second[i] = src[i] * volume;
      }
    }
  }

  if (frames_to_copy < num_frames) {
    int silence_samples = (num_frames - frames_to_copy) * channels_;
    std::memset(dest + frames_to_copy * channels_, 0,
                silence_samples * sizeof(float));
  }

  callbacks_.consume_frames(frames_to_copy, CurrentMonotonicTime(), context_);
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AaudioAudioSink::OnAudioError(aaudio_result_t error) {
  SB_LOG(WARNING) << "AAudio pull stream error: "
                  << AAudio::ConvertResultToText(error);
  if (!callbacks_.error) {
    return;
  }

  const bool capability_changed = (error == AAUDIO_ERROR_DISCONNECTED);
  callbacks_.error(capability_changed, "AAudio stream error", context_);
}

}  // namespace starboard

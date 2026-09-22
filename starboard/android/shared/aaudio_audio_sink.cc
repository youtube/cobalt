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
#include <sys/system_properties.h>

#include <algorithm>
#include <cstring>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "starboard/android/shared/aaudio_loader.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

namespace starboard {

namespace {

std::atomic<bool> g_ndk_audio_pull_sink_enabled{false};

}  // namespace

// static
AaudioAudioSinkType* AaudioAudioSinkType::GetInstance() {
  static AaudioAudioSinkType instance;
  return &instance;
}

// static
bool AaudioAudioSinkType::IsSupported(SbMediaAudioSampleType sample_type) {
  return sample_type == kSbMediaAudioSampleTypeFloat32 &&
         android_get_device_api_level() >= 28 && AAudio::Load();
}

// static
bool AaudioAudioSinkType::IsEnabled() {
  if (g_ndk_audio_pull_sink_enabled.load(std::memory_order_relaxed)) {
    return true;
  }
  char prop_value[PROP_VALUE_MAX] = {0};
  if (__system_property_get("debug.cobalt.audio.pull_sink", prop_value) > 0) {
    return strcmp(prop_value, "1") == 0 || strcmp(prop_value, "true") == 0;
  }
  auto* app = Application::Get();
  if (app && app->GetCommandLine()) {
    const auto* cmd = app->GetCommandLine();
    if (cmd->HasSwitch("ndk_audio_pull_sink") ||
        cmd->HasSwitch("enable_ndk_audio_pull_sink")) {
      return true;
    }
  }
  return false;
}

// static
void AaudioAudioSinkType::SetEnabled(bool enabled) {
  g_ndk_audio_pull_sink_enabled.store(enabled, std::memory_order_relaxed);
}

SbAudioSink AaudioAudioSinkType::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
    SbAudioSinkPrivate::ErrorFunc error_func,
    void* context) {
  return Create(channels, sampling_frequency_hz, audio_sample_type,
                frame_buffers, frames_per_channel,
                {update_source_status_func, consume_frames_func, error_func},
                /*start_time_us=*/0, /*is_web_audio=*/false, context);
}

SbAudioSink AaudioAudioSinkType::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    Callbacks callbacks,
    int64_t start_time_us,
    bool is_web_audio,
    void* context) {
  auto audio_sink = AaudioAudioSink::Create(
      this, channels, sampling_frequency_hz, audio_sample_type, frame_buffers,
      frames_per_channel, callbacks, start_time_us, is_web_audio, context);
  return audio_sink.release();
}

void AaudioAudioSink::AAudioStreamDeleter::operator()(
    AAudioStream* stream) const {
  if (stream) {
    AAudio::Stream_Close(stream);
  }
}

// static
std::unique_ptr<AaudioAudioSink> AaudioAudioSink::Create(
    Type* type,
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType sample_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    AaudioAudioSinkType::Callbacks callbacks,
    int64_t start_media_time_us,
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

  AAudioStreamBuilder* builder = nullptr;
  aaudio_result_t result = AAudio::CreateStreamBuilder(&builder);
  if (result != AAUDIO_OK || !builder) {
    SB_LOG(ERROR) << "Failed to create AAudioStreamBuilder: "
                  << AAudio::ConvertResultToText(result);
    return nullptr;
  }

  AAudio::StreamBuilder_SetDirection(builder, AAUDIO_DIRECTION_OUTPUT);
  AAudio::StreamBuilder_SetSampleRate(builder, sampling_frequency_hz);
  AAudio::StreamBuilder_SetChannelCount(builder, channels);
  AAudio::StreamBuilder_SetFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
  AAudio::StreamBuilder_SetSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
  AAudio::StreamBuilder_SetPerformanceMode(builder,
                                           AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudio::StreamBuilder_SetUsage(
      builder, is_web_audio ? AAUDIO_USAGE_GAME : AAUDIO_USAGE_MEDIA);
  AAudio::StreamBuilder_SetContentType(builder, AAUDIO_CONTENT_TYPE_MUSIC);

  std::unique_ptr<AaudioAudioSink> sink = std::make_unique<AaudioAudioSink>(
      PassKey<AaudioAudioSink>(), type, channels, sampling_frequency_hz,
      sample_type, frame_buffers, frames_per_channel, callbacks,
      start_media_time_us, context);

  AAudio::StreamBuilder_SetDataCallback(builder, AudioDataCallback, sink.get());
  if (AAudio::StreamBuilder_SetErrorCallback) {
    AAudio::StreamBuilder_SetErrorCallback(builder, AudioErrorCallback,
                                           sink.get());
  }

  AAudioStream* raw_stream = nullptr;
  result = AAudio::StreamBuilder_OpenStream(builder, &raw_stream);
  AAudio::StreamBuilder_Delete(builder);

  if (result != AAUDIO_OK || !raw_stream) {
    SB_LOG(ERROR) << "Failed to open AAudioStream: "
                  << AAudio::ConvertResultToText(result);
    return nullptr;
  }

  sink->stream_.reset(raw_stream);

  int32_t burst_frames = AAudio::Stream_GetFramesPerBurst(raw_stream);
  if (burst_frames > 0) {
    AAudio::Stream_SetBufferSizeInFrames(raw_stream, burst_frames * 2);
  }

  result = AAudio::Stream_RequestStart(raw_stream);
  if (result != AAUDIO_OK) {
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
                                 Type* type,
                                 int channels,
                                 int sampling_frequency_hz,
                                 SbMediaAudioSampleType sample_type,
                                 SbAudioSinkFrameBuffers frame_buffers,
                                 int frames_per_channel,
                                 AaudioAudioSinkType::Callbacks callbacks,
                                 int64_t start_media_time_us,
                                 void* context)
    : type_(type),
      channels_(channels),
      frame_buffer_(frame_buffers[0]),
      frames_per_channel_(frames_per_channel),
      callbacks_(callbacks),
      context_(context),
      start_time_us_(start_media_time_us),
      created_at_(CurrentMonotonicTime()) {
  SB_DCHECK(callbacks_.update_source_status);
  SB_DCHECK(callbacks_.consume_frames);
  SB_DCHECK(frame_buffer_);
}

AaudioAudioSink::~AaudioAudioSink() {
  quit_.store(true, std::memory_order_release);
  int underruns = GetUnderrunCount();
  if (stream_) {
    // Calling Stream_Close blocks until the AAudio callback thread has fully
    // terminated, ensuring safe read access to the non-atomic statistics below.
    stream_.reset();
  }

  int64_t avg_cb_us =
      total_callbacks_ > 0 ? total_callback_duration_us_ / total_callbacks_ : 0;
  double avg_switch_ms =
      switch_count_ > 0
          ? (static_cast<double>(total_switch_latency_us_) / switch_count_) /
                1000.0
          : -1.0;
  SB_LOG(INFO) << "[AudioSinkPerf] Mode: PULL (AAudio) Summary: "
               << "total_audio_frames=" << total_audio_frames_
               << ", total_silence_frames=" << total_silence_frames_
               << ", xruns=" << underruns << ", callbacks=" << total_callbacks_
               << ", avg_callback_us=" << avg_cb_us << ", startup_latency_ms="
               << (first_frame_rendered_ ? startup_latency_us_ / 1000.0 : -1.0)
               << ", avg_switch_latency_ms=" << avg_switch_ms;
}

bool AaudioAudioSink::IsType(Type* type) {
  return type == type_ || type == AaudioAudioSinkType::GetInstance() ||
         type == SbAudioSinkImpl::GetPrimaryType();
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

int AaudioAudioSink::GetUnderrunCount() {
  if (!stream_) {
    return 0;
  }
  return AAudio::Stream_GetXRunCount(stream_.get());
}

int AaudioAudioSink::GetStartThresholdInFrames() {
  if (!stream_) {
    return 0;
  }
  return AAudio::Stream_GetFramesPerBurst(stream_.get());
}

bool AaudioAudioSink::Flush() {
  flush_requested_at_.store(CurrentMonotonicTime(), std::memory_order_release);
  flush_requested_.store(true, std::memory_order_release);
  return true;
}

void AaudioAudioSink::SetStartTime(int64_t start_time_us) {
  start_time_us_.store(start_time_us, std::memory_order_relaxed);
}

// static
aaudio_data_callback_result_t AaudioAudioSink::AudioDataCallback(
    AAudioStream* stream,
    void* user_data,
    void* audio_data,
    int32_t num_frames) {
  auto* sink = static_cast<AaudioAudioSink*>(user_data);
  return sink->OnAudioData(audio_data, num_frames);
}

// static
void AaudioAudioSink::AudioErrorCallback(AAudioStream* stream,
                                         void* user_data,
                                         aaudio_result_t error) {
  auto* sink = static_cast<AaudioAudioSink*>(user_data);
  sink->OnAudioError(error);
}

aaudio_data_callback_result_t AaudioAudioSink::OnAudioData(void* audio_data,
                                                           int32_t num_frames) {
  int64_t cb_start = CurrentMonotonicTime();
  ++total_callbacks_;

  if (quit_.load(std::memory_order_acquire)) {
    return AAUDIO_CALLBACK_RESULT_STOP;
  }

  if (flush_requested_.exchange(false, std::memory_order_acq_rel)) {
    std::memset(audio_data, 0, num_frames * channels_ * sizeof(float));
    total_silence_frames_ += num_frames;
    total_callback_duration_us_ += (CurrentMonotonicTime() - cb_start);
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
    total_silence_frames_ += num_frames;
    total_callback_duration_us_ += (CurrentMonotonicTime() - cb_start);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  int frames_to_copy = std::min(static_cast<int>(num_frames), frames_in_buffer);
  float* dest = static_cast<float*>(audio_data);
  const float* src = static_cast<const float*>(frame_buffer_.get());
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
    total_silence_frames_ += (num_frames - frames_to_copy);
  }

  total_audio_frames_ += frames_to_copy;

  int64_t now = CurrentMonotonicTime();
  callbacks_.consume_frames(frames_to_copy, now, context_);

  // Performance logging: Startup latency measurement
  if (!first_frame_rendered_ && frames_to_copy > 0) {
    first_frame_rendered_ = true;
    startup_latency_us_ = now - created_at_;
    SB_LOG(INFO) << "[AudioSinkPerf] Mode: PULL (AAudio) Startup latency: "
                 << (startup_latency_us_ / 1000.0)
                 << " ms (from sink create to first " << frames_to_copy
                 << " frames rendered)";
  }

  // Performance logging: Switch / Seek latency measurement
  if (flush_requested_at_.load(std::memory_order_relaxed) > 0) {
    int64_t flush_time =
        flush_requested_at_.exchange(-1, std::memory_order_acq_rel);
    if (flush_time > 0 && frames_to_copy > 0) {
      int64_t switch_latency_us = now - flush_time;
      total_switch_latency_us_ += switch_latency_us;
      ++switch_count_;
      SB_LOG(INFO)
          << "[AudioSinkPerf] Mode: PULL (AAudio) Switch/Seek latency: "
          << (switch_latency_us / 1000.0) << " ms";
    }
  }

  total_callback_duration_us_ += (CurrentMonotonicTime() - cb_start);

  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AaudioAudioSink::OnAudioError(aaudio_result_t error) {
  SB_LOG(WARNING) << "AAudio pull stream error: "
                  << AAudio::ConvertResultToText(error);
  if (error == AAUDIO_ERROR_DISCONNECTED) {
    if (callbacks_.error) {
      callbacks_.error(true, "Audio device disconnected", context_);
    }
  }
}

}  // namespace starboard

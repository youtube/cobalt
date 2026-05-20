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

#include <unistd.h>

#include <algorithm>
#include <cmath>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {

namespace {

void* IncrementPointerByBytes(void* pointer, size_t offset) {
  return static_cast<uint8_t*>(pointer) + offset;
}

}  // namespace

// static
AAudioStream* AAudioAudioSink::OpenStreamHelper(AAudioStreamBuilder* builder,
                                                AAudioAudioSink* sink) {
  AAudioLoader* aaudio = AAudioLoader::GetInstance();
  aaudio->streamBuilder_setDataCallback(
      builder, AAudioAudioSink::AAudioDataCallback, sink);
  AAudioStream* stream = nullptr;
  aaudio_result_t result = aaudio->streamBuilder_openStream(builder, &stream);
  if (result != AAUDIO_OK) {
    SB_LOG(ERROR) << "Failed to open AAudio stream in helper: "
                  << aaudio->convertResultToText(result);
    return nullptr;
  }
  return stream;
}

std::unique_ptr<AAudioAudioSink> AAudioAudioSink::Create(
    SbAudioSinkPrivate::Type* type,
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType sample_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    AudioTrackAudioSinkType::Callbacks callbacks,
    void* context) {
  AAudioLoader* aaudio = AAudioLoader::GetInstance();
  if (!aaudio) {
    return nullptr;
  }

  AAudioStreamBuilder* builder = nullptr;
  aaudio_result_t result = aaudio->createStreamBuilder(&builder);
  if (result != AAUDIO_OK) {
    SB_LOG(ERROR) << "Failed to create AAudio stream builder: "
                  << aaudio->convertResultToText(result);
    return nullptr;
  }

  aaudio->streamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
  aaudio->streamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
  // We default to AAUDIO_PERFORMANCE_MODE_LOW_LATENCY.
  // While low latency mode could previously trigger SoC driver freezes in
  // polling mode due to thread scheduling jitter/underflows, the new Callback
  // Mode runs on a system-scheduled real-time thread, which completely
  // prevents underflows and makes low-latency streams rock-stable.
  aaudio->streamBuilder_setPerformanceMode(builder,
                                           AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  aaudio->streamBuilder_setChannelCount(builder, channels);
  aaudio->streamBuilder_setSampleRate(builder, sampling_frequency_hz);

  aaudio_format_t format = AAUDIO_FORMAT_INVALID;
  if (sample_type == kSbMediaAudioSampleTypeFloat32) {
    format = AAUDIO_FORMAT_PCM_FLOAT;
  } else if (sample_type == kSbMediaAudioSampleTypeInt16Deprecated) {
    format = AAUDIO_FORMAT_PCM_I16;
  } else {
    SB_NOTREACHED() << "Unsupported sample type: " << sample_type;
  }
  aaudio->streamBuilder_setFormat(builder, format);

  // We pass the builder to the constructor, which will set the callback,
  // open the stream via OpenStreamHelper, and take ownership of the builder
  // lifecycle.
  auto sink = std::make_unique<AAudioAudioSink>(
      PassKey<AAudioAudioSink>(), type, channels, sampling_frequency_hz,
      sample_type, frame_buffers, frames_per_channel, callbacks, builder,
      context);

  // If stream opening failed inside the constructor, it will fail to initialize
  // and we return null.
  if (!sink->stream()) {
    SB_LOG(ERROR) << "Failed to initialize AAudio stream inside constructor.";
    return nullptr;
  }

  return sink;
}

AAudioAudioSink::AAudioAudioSink(PassKey<AAudioAudioSink>,
                                 SbAudioSinkPrivate::Type* type,
                                 int channels,
                                 int sampling_frequency_hz,
                                 SbMediaAudioSampleType sample_type,
                                 SbAudioSinkFrameBuffers frame_buffers,
                                 int frames_per_channel,
                                 AudioTrackAudioSinkType::Callbacks callbacks,
                                 AAudioStreamBuilder* builder,
                                 void* context)
    : type_(type),
      channels_(channels),
      sampling_frequency_hz_(sampling_frequency_hz),
      sample_type_(sample_type),
      frame_buffer_(frame_buffers[0]),
      frames_per_channel_(frames_per_channel),
      callbacks_(callbacks),
      context_(context),
      aaudio_(AAudioLoader::GetInstance()),
      stream_(OpenStreamHelper(builder, this)),
      job_thread_(JobThread::Create(
          "aaudio_progress",
          ThreadOptions().SetPriority(kSbThreadPriorityRealTime))) {
  SB_DCHECK(callbacks_.update_source_status);
  SB_DCHECK(callbacks_.consume_frames);
  SB_DCHECK(frame_buffer_);

  if (stream_) {
    int32_t buffer_size = aaudio_->stream_getBufferSizeInFrames(stream_);
    int32_t burst_size = aaudio_->stream_getFramesPerBurst(stream_);
    SB_LOG(INFO) << "AAudio stream opened successfully in constructor. "
                 << "Buffer Size: " << buffer_size
                 << ", Burst Size: " << burst_size;

    // Initialize the high-priority progress job thread
    // Schedule the first progress polling job
    progress_job_token_ =
        job_thread_->Schedule([this]() { PollProgress(); }, 10'000);
  } else {
    SB_LOG(ERROR) << "Failed to open AAudio stream inside constructor.";
  }

  // Clean up builder now that stream is opened
  aaudio_->streamBuilder_delete(builder);
}

AAudioAudioSink::~AAudioAudioSink() {
  // Clean up the job thread first to prevent any pending tasks from running
  // during object destruction.
  job_thread_->Stop();

  if (stream_) {
    aaudio_->stream_requestStop(stream_);
    aaudio_->stream_close(stream_);
  }
  SB_LOG(INFO) << "AAudioAudioSink destroyed.";
}

void AAudioAudioSink::SetPlaybackRate(double playback_rate) {
  SB_DCHECK_GE(playback_rate, 0.0);
  std::lock_guard<std::mutex> lock(mutex_);
  playback_rate_ = playback_rate;
  // Playback rate is handled by software resampling in Cobalt for non-tunnel
  // mode, so we don't need to pass this to AAudio.
}

void AAudioAudioSink::SetVolume(double volume) {
  std::lock_guard<std::mutex> lock(mutex_);
  volume_ = volume;
}

// static
aaudio_data_callback_result_t AAudioAudioSink::AAudioDataCallback(
    AAudioStream* stream,
    void* userData,
    void* audioData,
    int32_t numFrames) {
  AAudioAudioSink* sink = static_cast<AAudioAudioSink*>(userData);
  return sink->OnAudioCallback(audioData, numFrames);
}

aaudio_data_callback_result_t AAudioAudioSink::OnAudioCallback(
    void* audioData,
    int32_t numFrames) {
  int frames_in_buffer = 0;
  int offset_in_frames = 0;
  bool is_playing = false;
  bool is_eos_reached = false;

  // Retrieve buffer status from Cobalt
  callbacks_.update_source_status(&frames_in_buffer, &offset_in_frames,
                                  &is_playing, &is_eos_reached, context_);

  double current_volume = 1.0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    current_volume = volume_;
    if (playback_rate_ == 0.0) {
      is_playing = false;
    }
  }

  size_t bytes_per_sample = GetBytesPerSample(sample_type_);
  size_t bytes_per_frame = channels_ * bytes_per_sample;

  int frames_in_sink = frames_in_sink_.load();
  int available_frames = std::max(0, frames_in_buffer - frames_in_sink);

  // If stopped or empty, fill with silence and continue
  if (!is_playing || available_frames == 0) {
    memset(audioData, 0, numFrames * bytes_per_frame);

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  int frames_to_copy = std::min(numFrames, available_frames);
  int start_position =
      (offset_in_frames + frames_in_sink) % frames_per_channel_;

  if (frames_per_channel_ >= start_position + frames_to_copy) {
    // Contiguous copy
    void* src_ptr = IncrementPointerByBytes(frame_buffer_,
                                            start_position * bytes_per_frame);
    memcpy(audioData, src_ptr, frames_to_copy * bytes_per_frame);
  } else {
    // Circular wrap copy
    int first_part_frames = frames_per_channel_ - start_position;
    void* src_ptr1 = IncrementPointerByBytes(frame_buffer_,
                                             start_position * bytes_per_frame);
    memcpy(audioData, src_ptr1, first_part_frames * bytes_per_frame);

    int second_part_frames = frames_to_copy - first_part_frames;
    void* dst_ptr2 =
        IncrementPointerByBytes(audioData, first_part_frames * bytes_per_frame);
    memcpy(dst_ptr2, frame_buffer_, second_part_frames * bytes_per_frame);
  }

  // Fill underflow remaining space with silence
  if (frames_to_copy < numFrames) {
    int missing_frames = numFrames - frames_to_copy;
    void* dst_ptr_silence =
        IncrementPointerByBytes(audioData, frames_to_copy * bytes_per_frame);
    memset(dst_ptr_silence, 0, missing_frames * bytes_per_frame);
  }

  frames_in_sink_.fetch_add(frames_to_copy);

  // Apply volume directly to the hardware buffer
  if (current_volume != 1.0) {
    ApplyVolume(audioData, numFrames, current_volume);
  }

  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AAudioAudioSink::PollProgress() {
  aaudio_stream_state_t state = aaudio_->stream_getState(stream_);

  int frames_in_buffer = 0;
  int offset_in_frames = 0;
  bool is_playing = false;
  bool is_eos_reached = false;

  // Query Cobalt status to get play/pause state
  callbacks_.update_source_status(&frames_in_buffer, &offset_in_frames,
                                  &is_playing, &is_eos_reached, context_);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (playback_rate_ == 0.0) {
      is_playing = false;
    }
  }

  // Handle AAudio Stream lifecycle transitions
  bool is_currently_playing = (state == AAUDIO_STREAM_STATE_STARTED ||
                               state == AAUDIO_STREAM_STATE_STARTING);
  bool can_start = (state == AAUDIO_STREAM_STATE_OPEN ||
                    state == AAUDIO_STREAM_STATE_PAUSED ||
                    state == AAUDIO_STREAM_STATE_STOPPED ||
                    state == AAUDIO_STREAM_STATE_PAUSING ||
                    state == AAUDIO_STREAM_STATE_STOPPING);

  if (is_currently_playing && !is_playing) {
    SB_LOG(INFO) << "[AudioSink] Calling stream_requestPause";
    aaudio_result_t result = aaudio_->stream_requestPause(stream_);
    SB_LOG(INFO) << "[AudioSink] stream_requestPause returned: " << result;
  } else if (can_start && is_playing) {
    SB_LOG(INFO) << "[AudioSink] Calling stream_requestStart";
    aaudio_result_t result = aaudio_->stream_requestStart(stream_);
    SB_LOG(INFO) << "[AudioSink] stream_requestStart returned: " << result;
  }

  // Progress reporting (using super-smooth getFramesRead pipeline compensation)
  if (is_currently_playing || state == AAUDIO_STREAM_STATE_PAUSED ||
      state == AAUDIO_STREAM_STATE_PAUSING) {
    TrackAndConsumePlayhead();
  }

  // Control polling frequency dynamically to save CPU when paused
  int64_t delay_us = 10000;  // 10ms default
  if (state != AAUDIO_STREAM_STATE_STARTED) {
    delay_us = 200000;  // 200ms when paused/stopped
  }

  std::lock_guard<std::mutex> lock(mutex_);
  progress_job_token_ =
      job_thread_->Schedule([this]() { PollProgress(); }, delay_us);
}

void AAudioAudioSink::TrackAndConsumePlayhead() {
  int64_t frames_read = aaudio_->stream_getFramesRead(stream_);
  int32_t buffer_size = aaudio_->stream_getBufferSizeInFrames(stream_);

  // Compensate for pipeline latency (buffer size) to maintain perfect A/V sync
  int64_t frame_position = std::max(0LL, frames_read - buffer_size);
  int64_t time_nanoseconds = CurrentMonotonicTime() * 1000;

  if (frame_position >= last_playback_head_position_) {
    int frames_consumed = frame_position - last_playback_head_position_;
    if (frames_consumed > 0) {
      // Cap clock progress to 16ms (768 frames) per tick to prevent visual
      // jumpiness during stream startup or resume, while smoothly catching up.
      constexpr int kMaxFramesConsumedPerTick = 768;
      int frames_to_report =
          std::min(frames_consumed, kMaxFramesConsumedPerTick);

      // Cap by frames actually in sink to prevent over-consuming Cobalt buffer
      int frames_in_sink = frames_in_sink_.load();
      frames_to_report = std::min(frames_to_report, frames_in_sink);

      if (frames_to_report > 0) {
        int64_t frames_consumed_at_us = time_nanoseconds / 1000;
        callbacks_.consume_frames(frames_to_report, frames_consumed_at_us,
                                  context_);

        frames_in_sink_.fetch_sub(frames_to_report);
        last_playback_head_position_ += frames_to_report;
      }
    }
  } else {
    SB_LOG(WARNING) << "[AudioSink] Hardware playhead went backward! "
                    << "Previous: " << last_playback_head_position_
                    << ", Current: " << frame_position;
    last_playback_head_position_ = frame_position;
  }
}

void AAudioAudioSink::ApplyVolume(void* buffer, int frames, double volume) {
  int total_samples = frames * channels_;
  float vol_f = static_cast<float>(volume);

  if (sample_type_ == kSbMediaAudioSampleTypeFloat32) {
    float* samples = static_cast<float*>(buffer);
    for (int i = 0; i < total_samples; ++i) {
      samples[i] *= vol_f;
    }
  } else if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated) {
    int16_t* samples = static_cast<int16_t*>(buffer);
    for (int i = 0; i < total_samples; ++i) {
      float scaled = samples[i] * vol_f;
      // Clamp to int16 limits
      samples[i] =
          static_cast<int16_t>(std::max(-32768.0f, std::min(32767.0f, scaled)));
    }
  }
}

void AAudioAudioSink::ReportError(const std::string& error_message) {
  SB_LOG(ERROR) << "AAudioAudioSink error: " << error_message;
  if (callbacks_.error) {
    // AAudio errors are generally not transient device capability changes, so
    // we set capability_changed = false
    callbacks_.error(false, error_message, context_);
  }
}

int64_t AAudioAudioSink::GetFramesDurationUs(int frames) const {
  return frames * 1'000'000LL / sampling_frequency_hz_;
}

}  // namespace starboard

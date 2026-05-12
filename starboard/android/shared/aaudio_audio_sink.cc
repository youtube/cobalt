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

namespace starboard::android {

namespace {

// Capping write request size to avoid large allocations
constexpr int kMaxFramesPerRequest = 65'536;

// Timeout for AAudioStream_write. We use 0 for non-blocking.
constexpr int64_t kWriteTimeoutNanoseconds = 0;

void* IncrementPointerByBytes(void* pointer, size_t offset) {
  return static_cast<uint8_t*>(pointer) + offset;
}

}  // namespace

class AAudioAudioSink::AAudioOutThread : public Thread {
 public:
  explicit AAudioOutThread(AAudioAudioSink* sink)
      : Thread("aaudio_out",
               ThreadOptions().SetPriority(kSbThreadPriorityRealTime)),
        sink_(sink) {}

  void Run() override { sink_->AudioThreadFunc(); }

 private:
  const raw_ptr<AAudioAudioSink> sink_;
};

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
  if (!aaudio->IsSupported()) {
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
  aaudio->streamBuilder_setPerformanceMode(builder,
                                           AAUDIO_PERFORMANCE_MODE_NONE);
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

  AAudioStream* stream = nullptr;
  result = aaudio->streamBuilder_openStream(builder, &stream);
  aaudio->streamBuilder_delete(builder);  // clean up builder

  if (result != AAUDIO_OK) {
    SB_LOG(ERROR) << "Failed to open AAudio stream: "
                  << aaudio->convertResultToText(result);
    return nullptr;
  }

  SB_LOG(INFO) << "Successfully opened AAudio stream. Format: " << format
               << ", Channels: " << channels
               << ", Sample Rate: " << sampling_frequency_hz;

  auto sink = std::make_unique<AAudioAudioSink>(
      PassKey<AAudioAudioSink>(), type, channels, sampling_frequency_hz,
      sample_type, frame_buffers, frames_per_channel, callbacks, stream,
      context);

  sink->SpawnThread();
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
                                 AAudioStream* stream,
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
      stream_(stream) {
  SB_DCHECK(callbacks_.update_source_status);
  SB_DCHECK(callbacks_.consume_frames);
  SB_DCHECK(frame_buffer_);
  SB_CHECK(stream_);
}

void AAudioAudioSink::SpawnThread() {
  audio_out_thread_ = std::make_unique<AAudioOutThread>(this);
  audio_out_thread_->Start();
}

AAudioAudioSink::~AAudioAudioSink() {
  quit_ = true;
  if (audio_out_thread_) {
    audio_out_thread_->Join();
  }

  if (stream_) {
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

void AAudioAudioSink::AudioThreadFunc() {
  SB_LOG(INFO) << "AAudioAudioSink thread started.";

  int frames_in_audio_stream = 0;
  int64_t last_playback_head_position = 0;

  while (!quit_) {
    // 1. Handle Playback Head and Consume Frames
    SB_LOG(INFO) << "[AAudioTrace] Calling stream_getState";
    aaudio_stream_state_t state = aaudio_->stream_getState(stream_);
    SB_LOG(INFO) << "[AAudioTrace] stream_getState returned: " << state;
    bool is_currently_playing = (state == AAUDIO_STREAM_STATE_STARTED);

    if (is_currently_playing || state == AAUDIO_STREAM_STATE_PAUSED) {
      int64_t frame_position = 0;
      int64_t time_nanoseconds = 0;

      // Retrieve hardware timestamp
      SB_LOG(INFO) << "[AAudioTrace] Calling stream_getTimestamp";
      aaudio_result_t result = aaudio_->stream_getTimestamp(
          stream_, CLOCK_MONOTONIC, &frame_position, &time_nanoseconds);
      SB_LOG(INFO) << "[AAudioTrace] stream_getTimestamp returned: " << result;

      if (result == AAUDIO_OK) {
        SB_DCHECK_GE(frame_position, last_playback_head_position);
        int frames_consumed = frame_position - last_playback_head_position;
        frames_consumed = std::min(frames_consumed, frames_in_audio_stream);

        if (frames_consumed != 0) {
          int64_t frames_consumed_at_us = time_nanoseconds / 1000;
          callbacks_.consume_frames(frames_consumed, frames_consumed_at_us,
                                    context_);
          frames_in_audio_stream -= frames_consumed;
          last_playback_head_position = frame_position;
        }
      }
    }

    // 2. Update Source Status
    int frames_in_buffer;
    int offset_in_frames;
    bool is_playing;
    bool is_eos_reached;
    callbacks_.update_source_status(&frames_in_buffer, &offset_in_frames,
                                    &is_playing, &is_eos_reached, context_);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (playback_rate_ == 0.0) {
        is_playing = false;
      }
    }

    // 3. Handle Stream State Changes
    bool can_start = (state == AAUDIO_STREAM_STATE_OPEN ||
                      state == AAUDIO_STREAM_STATE_PAUSED ||
                      state == AAUDIO_STREAM_STATE_STOPPED ||
                      state == AAUDIO_STREAM_STATE_PAUSING ||
                      state == AAUDIO_STREAM_STATE_STOPPING);

    if (is_currently_playing && !is_playing) {
      SB_LOG(INFO) << "[AAudioTrace] Calling stream_requestPause";
      aaudio_->stream_requestPause(stream_);
      SB_LOG(INFO) << "[AAudioTrace] stream_requestPause returned";
    } else if (can_start && is_playing) {
      SB_LOG(INFO) << "[AAudioTrace] Calling stream_requestStart";
      aaudio_->stream_requestStart(stream_);
      SB_LOG(INFO) << "[AAudioTrace] stream_requestStart returned";
    }

    if (!is_playing || frames_in_buffer == 0) {
      int sleep_us = 10'000;
      if (state != AAUDIO_STREAM_STATE_STARTED) {
        sleep_us = 200'000;  // Slow down polling when not actively playing to
                             // prevent driver starvation
      }
      usleep(sleep_us);
      continue;
    }

    // 4. Calculate Write Parameters
    int start_position =
        (offset_in_frames + frames_in_audio_stream) % frames_per_channel_;
    int expected_written_frames = 0;
    if (frames_per_channel_ > offset_in_frames + frames_in_audio_stream) {
      expected_written_frames = std::min(
          frames_per_channel_ - (offset_in_frames + frames_in_audio_stream),
          frames_in_buffer - frames_in_audio_stream);
    } else {
      expected_written_frames = frames_in_buffer - frames_in_audio_stream;
    }

    expected_written_frames =
        std::min(expected_written_frames, kMaxFramesPerRequest);

    if (expected_written_frames == 0) {
      usleep(10'000);
      continue;
    }

    // 5. Write Data
    void* src_ptr = IncrementPointerByBytes(
        frame_buffer_,
        start_position * channels_ * GetBytesPerSample(sample_type_));

    int written_frames = WriteData(src_ptr, expected_written_frames);

    if (written_frames < 0) {
      ReportError(FormatString("AAudio write error: %s",
                               aaudio_->convertResultToText(written_frames)));
      break;
    }

    frames_in_audio_stream += written_frames;

    // Control sleep rate based on how full the stream buffer is
    int32_t buffer_size = aaudio_->stream_getBufferSizeInFrames(stream_);
    if (buffer_size > 0 && frames_in_audio_stream > buffer_size / 2) {
      usleep(20'000);
    } else if (written_frames < expected_written_frames) {
      usleep(10'000);
    }
  }

  aaudio_->stream_requestStop(stream_);
  aaudio_->stream_requestFlush(stream_);
}

int AAudioAudioSink::WriteData(const void* buffer,
                               int expected_written_frames) {
  double current_volume = 1.0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    current_volume = volume_;
  }

  const void* write_ptr = buffer;

  // Apply software volume control if necessary
  if (current_volume != 1.0) {
    size_t bytes_per_frame = channels_ * GetBytesPerSample(sample_type_);
    size_t required_bytes =
        static_cast<size_t>(expected_written_frames) * bytes_per_frame;
    if (volume_buffer_.size() < required_bytes) {
      volume_buffer_.resize(required_bytes);
    }
    memcpy(volume_buffer_.data(), buffer, required_bytes);
    ApplyVolume(volume_buffer_.data(), expected_written_frames, current_volume);
    write_ptr = volume_buffer_.data();
  }

  int64_t start = CurrentMonotonicTime();

  SB_LOG(INFO) << "[AAudioTrace] Calling stream_write with "
               << expected_written_frames << " frames";
  aaudio_result_t result = aaudio_->stream_write(
      stream_, write_ptr, expected_written_frames, kWriteTimeoutNanoseconds);
  SB_LOG(INFO) << "[AAudioTrace] stream_write returned: " << result;

  int64_t duration = CurrentMonotonicTime() - start;

  write_count_++;
  total_duration_ += duration;
  total_frames_ += expected_written_frames;

  if (write_count_ >= 500) {
    SB_LOG(INFO) << "[AudioSinkPerf] Write Type: NATIVE_AAUDIO, Writes: "
                 << write_count_
                 << ", Avg Frames: " << (total_frames_ / write_count_)
                 << ", Avg Time: " << (total_duration_ / write_count_) << "us";
    write_count_ = 0;
    total_duration_ = 0;
    total_frames_ = 0;
  }

  return result;  // AAudioStream_write returns number of frames written or
                  // negative error code
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

}  // namespace starboard::android

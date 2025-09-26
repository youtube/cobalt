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

#include "base/android/jni_android.h"
#include "starboard/common/check_op.h"
#include "starboard/common/scoped_timer.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/thread.h"

namespace starboard {
namespace {

using AudioTimestamp = AudioTrackBridge::AudioTimestamp;

// The maximum number of frames that can be written to android audio track per
// we will repeatedly allocate a large byte array which cannot be consumed by
// audio track completely.
constexpr int kMaxFramesPerRequest = 64 * 1024;

constexpr int kSilenceIntervalUs = 10'000;

constexpr int kSilenceFramesPerAppend = 1024;
static_assert(kSilenceFramesPerAppend <= kMaxFramesPerRequest);

void HandleNoFramesToWrite(JNIEnv* env,
                           const AudioSourceState& source_state,
                           int frame_bytes,
                           ContinuousAudioTrackSink* sink) {
  if (source_state.is_eos_reached) {
    std::vector<uint8_t> silence_buffer(frame_bytes * kSilenceFramesPerAppend);
    sink->WriteData(env, silence_buffer.data(), kSilenceFramesPerAppend);
  }
  usleep(10'000);
}

}  // namespace

ContinuousAudioTrackSink::ContinuousAudioTrackSink(
    Type* type,
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType sample_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int source_buffer_frames,
    int preferred_buffer_size_in_bytes,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    ConsumeFramesFunc consume_frames_func,
    SbAudioSinkPrivate::ErrorFunc error_func,
    bool is_web_audio,
    void* context)
    : type_(type),
      channels_(channels),
      sampling_frequency_hz_(sampling_frequency_hz),
      sample_type_(sample_type),
      frame_bytes_(channels_ * GetBytesPerSample(sample_type)),
      frame_buffer_(static_cast<uint8_t*>(frame_buffers[0])),
      source_buffer_frames_(source_buffer_frames),
      update_source_status_func_(update_source_status_func),
      consume_frames_func_(consume_frames_func),
      error_func_(error_func),
      context_(context),
      bridge_(kSbMediaAudioCodingTypePcm,
              sample_type,
              channels,
              sampling_frequency_hz,
              preferred_buffer_size_in_bytes,
              /*tunnel_mode_audio_session_id=*/-1,
              is_web_audio),
      initial_frames_(bridge_.GetStartThresholdInFrames()),
      silence_buffer_frames_(kSilenceIntervalUs * sampling_frequency_hz_ /
                             1'000'000),
      silence_buffer_(silence_buffer_frames_ * frame_bytes_) {
  SB_CHECK(update_source_status_func_);
  SB_CHECK(consume_frames_func_);
  SB_CHECK(frame_buffer_);

  SB_LOG(INFO) << "Creating continuous audio sink: initial_frames="
               << initial_frames_
               << ", initial_frames(msec)=" << GetFramesMs(initial_frames_);

  if (!bridge_.is_valid()) {
    // One of the cases that this may hit is when output happened to be switched
    // to a device that doesn't support tunnel mode.
    // TODO: Find a way to exclude the device from tunnel mode playback, to
    //       avoid infinite loop in creating the audio sink on a device
    //       claims to support tunnel mode but fails to create the audio sink.
    // TODO: Currently this will be reported as a general decode error,
    //       investigate if this can be reported as a capability changed error.
    return;
  }

  pthread_t thread;
  const int result = pthread_create(
      &thread, nullptr, &ContinuousAudioTrackSink::ThreadEntryPoint, this);
  SB_CHECK_EQ(result, 0);
  audio_out_thread_ = thread;
}

ContinuousAudioTrackSink::~ContinuousAudioTrackSink() {
  quit_ = true;

  if (audio_out_thread_) {
    SB_CHECK_EQ(pthread_join(*audio_out_thread_, nullptr), 0);
  }
}

void ContinuousAudioTrackSink::SetPlaybackRate(double playback_rate) {
  SB_DCHECK_GE(playback_rate, 0.0);
  if (playback_rate != 0.0 && playback_rate != 1.0) {
    SB_NOTIMPLEMENTED() << "TODO: Only playback rates of 0.0 and 1.0 are "
                           "currently supported.";
    playback_rate = (playback_rate > 0.0) ? 1.0 : 0.0;
  }
  std::lock_guard lock(mutex_);
  playback_rate_ = playback_rate;
}

// static
void* ContinuousAudioTrackSink::ThreadEntryPoint(void* context) {
  pthread_setname_np(pthread_self(), "continous_audio_track_sink");
  SbThreadSetPriority(kSbThreadPriorityRealTime);

  SB_CHECK(context);
  ContinuousAudioTrackSink* sink =
      reinterpret_cast<ContinuousAudioTrackSink*>(context);
  sink->AudioThreadFunc();

  return nullptr;
}

int64_t ContinuousAudioTrackSink::EstimateFramePosition(
    const std::optional<AudioTimestamp>& timestamp,
    int64_t time_us) const {
  if (!timestamp) {
    // head_frame of 0 means that playback didn't start yet.
    return 0;
  }
  int64_t elapsed_us = time_us - timestamp->rendered_at_us;
  return timestamp->frame_position + GetFrames(elapsed_us);
}

std::optional<AudioTimestamp>
ContinuousAudioTrackSink::ReportConsumedFramesToSource(
    JNIEnv* env,
    AudioSinkState* sink_state,
    const std::optional<AudioTimestamp> new_timestamp,
    const std::optional<AudioTimestamp> last_timestamp) {
  if (!last_timestamp && new_timestamp) {
    SB_CHECK_LE(new_timestamp->frame_position, initial_silence_frames_)
        << "Timestamp should start from 0";
  }
  if (!new_timestamp) {
    // In this iteration, we don't report any new timestamp.
    return last_timestamp;
  }

  int64_t last_frame_position =
      last_timestamp ? last_timestamp->frame_position : 0;
  int64_t new_frame_position = new_timestamp->frame_position;

  last_frame_position =
      std::max(0LL, last_frame_position - initial_silence_frames_);
  new_frame_position =
      std::max(0LL, new_frame_position - initial_silence_frames_);

  SB_CHECK_GE(new_frame_position, last_frame_position);
  const int frames_consumed = new_frame_position - last_frame_position;

  sink_state->frames_in_audio_track -= frames_consumed;
  consume_frames_func_(frames_consumed, new_timestamp->rendered_at_us,
                       context_);
  return new_timestamp;
}

AudioSourceState ContinuousAudioTrackSink::GetAudioSourceState() {
  int frames_in_buffer;
  int offset_in_frames;
  bool is_playing;
  bool is_eos_reached;

  update_source_status_func_(&frames_in_buffer, &offset_in_frames, &is_playing,
                             &is_eos_reached, context_);
  {
    std::lock_guard lock(mutex_);
    if (playback_rate_ == 0.0) {
      is_playing = false;
    }
  }

  return AudioSourceState{
      frames_in_buffer,
      offset_in_frames,
      is_playing,
      is_eos_reached,
  };
}

void ContinuousAudioTrackSink::UpdatePlaybackState(
    JNIEnv* env,
    const AudioSourceState& source_state,
    AudioSinkState* sink_state) {
  SB_CHECK(sink_state);
  if (source_state.is_playing == sink_state->is_playing) {
    return;
  }

  if (source_state.is_playing) {
    bridge_.Play();
  } else {
    bridge_.Pause();
  }
  sink_state->is_playing = source_state.is_playing;
}

std::optional<int64_t> ContinuousAudioTrackSink::WriteAudioData(
    JNIEnv* env,
    const AudioSourceState& source_state,
    AudioSinkState* sink_state) {
  SB_CHECK(sink_state);

  const int total_frames_to_write =
      source_state.frames_in_buffer - sink_state->frames_in_audio_track;
  int total_frames_written = 0;

  while (total_frames_written < total_frames_to_write) {
    const int offset =
        (source_state.offset_in_frames + sink_state->frames_in_audio_track) %
        source_buffer_frames_;
    const int frames_to_write =
        std::min(source_buffer_frames_ - offset,
                 std::min(total_frames_to_write - total_frames_written,
                          kMaxFramesPerRequest));

    SB_CHECK_GT(frames_to_write, 0);
    SB_CHECK_LE(offset + frames_to_write, source_buffer_frames_)
        << "frames_in_buffer: " << source_state.frames_in_buffer
        << ", frames_in_audio_track: " << sink_state->frames_in_audio_track
        << ", offset_in_frames: " << source_state.offset_in_frames;

    const int frames_written = WriteData(
        env, frame_buffer_ + (offset * frame_bytes_), frames_to_write);
    if (frames_written == AudioTrackBridge::kAudioTrackErrorDeadObject) {
      ReportError(/*capability_changed=*/true,
                  "Failed to write data and received dead object error.");
      return std::nullopt;
    } else if (frames_written < 0) {
      ReportError(/*capability_changed=*/false,
                  FormatString("Failed to write data and received %d.",
                               frames_written));
      return std::nullopt;
    }

    sink_state->frames_in_audio_track += frames_written;
    total_frames_written += frames_written;
    if (frames_written < frames_to_write) {
      // WriteSample only partially wrote given frames, which means that buffer
      // is full.
      return 10'000;
    }
  }

  return 0;
}

void ContinuousAudioTrackSink::PrimeSilenceBuffer(JNIEnv* env) {
  while (initial_silence_frames_ < initial_frames_) {
    int frames_to_feed = std::min(silence_buffer_frames_,
                                  initial_frames_ - initial_silence_frames_);
    int written = WriteData(env, silence_buffer_.data(), frames_to_feed);
    initial_silence_frames_ += written;
  }
  SB_LOG(INFO) << __func__ << " > initial_silence(msec)="
               << GetFramesMs(initial_silence_frames_);
}

void ContinuousAudioTrackSink::AppendSilence(
    JNIEnv* env,
    std::optional<AudioTimestamp> last_timestamp) {
  const int64_t silence_frame_head =
      EstimateFramePosition(last_timestamp, CurrentMonotonicTime());
  const int64_t current_silence_us = GetFramesUs(silence_frame_head);

  const int64_t initial_silence_us = GetFramesUs(initial_silence_frames_);

  const int64_t silence_to_play_us = initial_silence_us - current_silence_us;
  if (silence_to_play_us > 10'000) {
    return;
  }

  int written_frames =
      WriteData(env, silence_buffer_.data(), silence_buffer_frames_);
  initial_silence_frames_ += written_frames;

  SB_LOG(INFO) << "Feeds silences: silence_to_play(msec)="
               << silence_to_play_us / 1'000
               << ", new silences(msec)=" << GetFramesMs(written_frames)
               << ", current_media(msec)=" << current_silence_us / 1'000
               << ", current silences fed(msec)="
               << GetFramesMs(initial_silence_frames_);
}

void ContinuousAudioTrackSink::AudioThreadFunc() {
  JNIEnv* env = base::android::AttachCurrentThread();

  SB_LOG(INFO) << "ContinuousAudioTrackSink thread started.";

  AudioSourceState source_state;
  AudioSinkState sink_state;
  std::optional<AudioTimestamp> last_reported_timestamp;

  PrimeSilenceBuffer(env);
  {
    ScopedTimer timer("Initial Play");
    bridge_.Play();
  }
  sink_state.is_playing = true;
  sink_state.is_playing_silence = true;

  bool is_first_audio_written = false;
  while (!quit_) {
    if (bridge_.GetAndResetHasAudioDeviceChanged(env)) {
      SB_LOG(INFO) << "Audio device changed, raising a capability changed "
                      "error to restart playback.";
      ReportError(/*capability_changed=*/true,
                  "Audio device capability changed");
      break;
    }

    std::optional<AudioTimestamp> timestamp = bridge_.GetRawAudioTimestamp(env);
    if (timestamp) {
      if (!playback_started_ &&
          playback_frame_head_ != timestamp->frame_position) {
        SB_LOG(INFO) << "Playback started";
        playback_started_ = true;
      }
      playback_frame_head_ = timestamp->frame_position;

      if (source_state.is_playing) {
        last_reported_timestamp = ReportConsumedFramesToSource(
            env, &sink_state, timestamp, last_reported_timestamp);
      }
    }
    source_state = GetAudioSourceState();

    if (sink_state.is_playing_silence && source_state.is_playing) {
      SB_LOG(INFO) << "Source started";
      sink_state.is_playing_silence = false;
    }
    if (sink_state.is_playing_silence) {
      AppendSilence(env, timestamp);
      usleep(10'000);
      continue;
    }

    UpdatePlaybackState(env, source_state, &sink_state);
    if (!source_state.is_playing || source_state.frames_in_buffer == 0) {
      usleep(10'000);
      continue;
    }

    if (!is_first_audio_written) {
      const int64_t frame_head =
          EstimateFramePosition(timestamp, CurrentMonotonicTime());

      SB_LOG(INFO) << "Now writing real audio: frame_head(msec)= "
                   << GetFramesMs(frame_head) << ", silences fed(msec)="
                   << GetFramesMs(initial_silence_frames_) << ", gap(msec)="
                   << GetFramesMs(initial_silence_frames_ - frame_head);
      is_first_audio_written = true;
    }
    std::optional<int64_t> continue_latency =
        WriteAudioData(env, source_state, &sink_state);
    if (!continue_latency) {
      SB_LOG(INFO) << "WriteAudioData failed";
      break;
    }
    if (*continue_latency > 0) {
      usleep(*continue_latency);
    }
  }

  bridge_.PauseAndFlush();
}

int ContinuousAudioTrackSink::WriteData(JNIEnv* env,
                                        const void* buffer,
                                        int frames_to_write) {
  const int samples_to_write = frames_to_write * channels_;
  int samples_written = 0;
  if (sample_type_ == kSbMediaAudioSampleTypeFloat32) {
    samples_written = bridge_.WriteSample(static_cast<const float*>(buffer),
                                          samples_to_write, env);
  } else if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated) {
    samples_written =
        bridge_.WriteSample(static_cast<const uint16_t*>(buffer),
                            samples_to_write, /*sync_time=*/0, env);
  } else {
    SB_NOTREACHED();
  }
  if (samples_written < 0) {
    // Error code returned as negative value, like kAudioTrackErrorDeadObject.
    SB_LOG(ERROR) << "WriteSample failed: ret=" << samples_written;
    return samples_written;
  }
  SB_CHECK_EQ(samples_written % channels_, 0);
  return samples_written / channels_;
}

void ContinuousAudioTrackSink::ReportError(bool capability_changed,
                                           const std::string& error_message) {
  SB_LOG(INFO) << "ContinuousAudioTrackSink error: " << error_message;
  if (error_func_) {
    error_func_(capability_changed, error_message, context_);
  }
}

int64_t ContinuousAudioTrackSink::GetFramesUs(int64_t frames) const {
  return frames * 1'000'000LL / sampling_frequency_hz_;
}

int64_t ContinuousAudioTrackSink::GetFrames(int64_t duration_us) const {
  return duration_us * sampling_frequency_hz_ / 1'000'000;
}

void ContinuousAudioTrackSink::SetVolume(double volume) {
  bridge_.SetVolume(volume);
}

int ContinuousAudioTrackSink::GetUnderrunCount() {
  return bridge_.GetUnderrunCount();
}

}  // namespace starboard

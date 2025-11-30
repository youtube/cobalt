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

#include "starboard/android/shared/jni_state.h"
#include "starboard/common/check_op.h"
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
}  // namespace

class ContinuousAudioTrackSink::AudioOutThread : public Thread {
 public:
  explicit AudioOutThread(ContinuousAudioTrackSink* sink)
      : Thread("cont_audio_sink"), sink_(sink) {}

  void Run() override {
    SbThreadSetPriority(kSbThreadPriorityRealTime);
    sink_->AudioThreadFunc();
  }

 private:
  ContinuousAudioTrackSink* sink_;
};

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
      sample_type_(sample_type),
      frame_buffer_(frame_buffers[0]),
      frames_per_channel_(frames_per_channel),
      update_source_status_func_(update_source_status_func),
      consume_frames_func_(consume_frames_func),
      error_func_(error_func),
      start_time_(start_time),
      context_(context),
      bridge_(kSbMediaAudioCodingTypePcm,
              sample_type,
              channels,
              sampling_frequency_hz,
              preferred_buffer_size_in_bytes,
              /*tunnel_mode_audio_session_id=*/-1,
              is_web_audio) {
  SB_DCHECK(update_source_status_func_);
  SB_DCHECK(consume_frames_func_);
  SB_DCHECK(frame_buffer_);

  SB_LOG(INFO) << "Creating continuous audio sink starts at " << start_time_;

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

  audio_out_thread_ = std::make_unique<AudioOutThread>(this);
  audio_out_thread_->Start();
}

ContinuousAudioTrackSink::~ContinuousAudioTrackSink() {
  quit_ = true;

  if (audio_out_thread_) {
    audio_out_thread_->Join();
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

// TODO: Break down the function into manageable pieces.
void ContinuousAudioTrackSink::AudioThreadFunc() {
  JniEnvExt* env = JniEnvExt::Get();
  bool was_playing = false;
  int frames_in_audio_track = 0;

  SB_LOG(INFO) << "ContinuousAudioTrackSink thread started.";

  int64_t last_playback_head_event_at = -1;  // microseconds
  int last_playback_head_position = 0;

  while (!quit_) {
    int playback_head_position = 0;
    int64_t frames_consumed_at = 0;
    if (bridge_.GetAndResetHasAudioDeviceChanged(env)) {
      SB_LOG(INFO) << "Audio device changed, raising a capability changed "
                      "error to restart playback.";
      ReportError(true, "Audio device capability changed");
      break;
    }

    if (was_playing) {
      playback_head_position =
          bridge_.GetAudioTimestamp(&frames_consumed_at, env);
      SB_DCHECK_GE(playback_head_position, last_playback_head_position);

      int frames_consumed =
          playback_head_position - last_playback_head_position;
      int64_t now = CurrentMonotonicTime();

      if (last_playback_head_event_at == -1) {
        last_playback_head_event_at = now;
      }
      if (last_playback_head_position == playback_head_position) {
        int64_t elapsed = now - last_playback_head_event_at;
        if (elapsed > 5'000'000LL) {
          last_playback_head_event_at = now;
          SB_LOG(INFO) << "last playback head position is "
                       << last_playback_head_position
                       << " and it hasn't been updated for " << elapsed
                       << " microseconds.";
        }
      } else {
        last_playback_head_event_at = now;
      }

      last_playback_head_position = playback_head_position;
      frames_consumed = std::min(frames_consumed, frames_in_audio_track);

      if (frames_consumed != 0) {
        SB_DCHECK_GE(frames_consumed, 0);
        consume_frames_func_(frames_consumed, frames_consumed_at, context_);
        frames_in_audio_track -= frames_consumed;
      }
    }

    int frames_in_buffer;
    int offset_in_frames;
    bool is_playing;
    bool is_eos_reached;
    update_source_status_func_(&frames_in_buffer, &offset_in_frames,
                               &is_playing, &is_eos_reached, context_);
    {
      std::lock_guard lock(mutex_);
      if (playback_rate_ == 0.0) {
        is_playing = false;
      }
    }

    if (was_playing && !is_playing) {
      was_playing = false;
      bridge_.Pause();
    } else if (!was_playing && is_playing) {
      was_playing = true;
      last_playback_head_event_at = -1;
      bridge_.Play();
    }

    if (!is_playing || frames_in_buffer == 0) {
      usleep(10'000);
      continue;
    }

    int start_position =
        (offset_in_frames + frames_in_audio_track) % frames_per_channel_;
    int expected_written_frames = 0;
    if (frames_per_channel_ > offset_in_frames + frames_in_audio_track) {
      expected_written_frames = std::min(
          frames_per_channel_ - (offset_in_frames + frames_in_audio_track),
          frames_in_buffer - frames_in_audio_track);
    } else {
      expected_written_frames = frames_in_buffer - frames_in_audio_track;
    }

    expected_written_frames =
        std::min(expected_written_frames, kMaxFramesPerRequest);

    if (expected_written_frames == 0) {
      // It is possible that all the frames in buffer are written to the
      // soundcard, but those are not being consumed. If eos is reached,
      // write silence to make sure audio track start working and avoid
      // underflow. Android audio track would not start working before
      // its buffer is fully filled once.
      if (is_eos_reached) {
        // Currently AudioDevice and AudioRenderer will write tail silence.
        // It should be reached only in tests. It's not ideal to allocate
        // a new silence buffer every time.
        std::vector<uint8_t> silence_buffer(channels_ *
                                            GetBytesPerSample(sample_type_) *
                                            kSilenceFramesPerAppend);
        // Not necessary to handle error of WriteData(), as the audio has
        // reached the end of stream.
        WriteData(env, silence_buffer.data(), kSilenceFramesPerAppend);
      }

      usleep(10'000);
      continue;
    }
    SB_DCHECK_GT(expected_written_frames, 0);
    SB_DCHECK(start_position + expected_written_frames <= frames_per_channel_)
        << "start_position: " << start_position
        << ", expected_written_frames: " << expected_written_frames
        << ", frames_per_channel_: " << frames_per_channel_
        << ", frames_in_buffer: " << frames_in_buffer
        << ", frames_in_audio_track: " << frames_in_audio_track
        << ", offset_in_frames: " << offset_in_frames;

    int written_frames =
        WriteData(env,
                  IncrementPointerByBytes(frame_buffer_,
                                          start_position * channels_ *
                                              GetBytesPerSample(sample_type_)),
                  expected_written_frames);
    int64_t now = CurrentMonotonicTime();

    if (written_frames < 0) {
      if (written_frames == AudioTrackBridge::kAudioTrackErrorDeadObject) {
        // There might be an audio device change, try to recreate the player.
        ReportError(true,
                    "Failed to write data and received dead object error.");
      } else {
        ReportError(false, FormatString("Failed to write data and received %d.",
                                        written_frames));
      }
      break;
    }
    frames_in_audio_track += written_frames;

    bool written_fully = (written_frames == expected_written_frames);
    auto unplayed_frames_in_time =
        GetFramesDurationUs(frames_in_audio_track) - (now - frames_consumed_at);
    // As long as there is enough data in the buffer, run the loop in lower
    // frequency to avoid taking too much CPU.  Note that the threshold should
    // be big enough to account for the unstable playback head reported at the
    // beginning of the playback and during underrun.
    if (playback_head_position > 0 && unplayed_frames_in_time > 500'000) {
      usleep(40'000);
    } else if (!written_fully) {
      // Only sleep if the buffer is nearly full and the last write is partial.
      usleep(10'000);
    }
  }

  bridge_.PauseAndFlush();
}

int ContinuousAudioTrackSink::WriteData(JniEnvExt* env,
                                        const void* buffer,
                                        int expected_written_frames) {
  const int samples_to_write = expected_written_frames * channels_;
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
    return samples_written;
  }
  SB_DCHECK_EQ(samples_written % channels_, 0);
  return samples_written / channels_;
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
  bridge_.SetVolume(volume);
}

int ContinuousAudioTrackSink::GetUnderrunCount() {
  return bridge_.GetUnderrunCount();
}

int ContinuousAudioTrackSink::GetStartThresholdInFrames() {
  return bridge_.GetStartThresholdInFrames();
}

}  // namespace starboard::android::shared

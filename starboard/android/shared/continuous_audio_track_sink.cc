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
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/thread.h"

namespace starboard {
namespace {

std::ostream& operator<<(std::ostream& os,
                         const ContinuousAudioTrackSink::Timestamp& timestamp) {
  os << "{frame_position=" << timestamp.frame_position
     << ",rendered_at_us=" << timestamp.rendered_at_us << "}";
  return os;
}

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
              is_web_audio),
      initial_frames_(bridge_.GetStartThresholdInFrames()) {
  SB_DCHECK(update_source_status_func_);
  SB_DCHECK(consume_frames_func_);
  SB_DCHECK(frame_buffer_);

  SB_LOG(INFO) << "Creating continuous audio sink starts at " << start_time_
               << ", initial_frames=" << initial_frames_
               << ", initial_frames(msec)="
               << GetFramesDurationUs(initial_frames_) / 1'000;

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
  SB_DCHECK(context);
  SbThreadSetPriority(kSbThreadPriorityRealTime);

  ContinuousAudioTrackSink* sink =
      reinterpret_cast<ContinuousAudioTrackSink*>(context);
  sink->AudioThreadFunc();

  return NULL;
}

std::optional<ContinuousAudioTrackSink::Timestamp>
ContinuousAudioTrackSink::GetTimestamp(JNIEnv* env) {
  int64_t rendered_at_us;
  int64_t frame_position = bridge_.GetAudioTimestamp(&rendered_at_us, env);
  return frame_position == 0 ? std::optional<Timestamp>(std::nullopt)
                             : Timestamp{frame_position, rendered_at_us};
}

int64_t ContinuousAudioTrackSink::EstimateFramePosition(
    const std::optional<Timestamp>& timestamp) const {
  if (!timestamp) {
    // head_frame of 0 means that playback didn't start yet.
    return 0;
  }
  int64_t elapsed_us = CurrentMonotonicTime() - timestamp->rendered_at_us;
  return timestamp->frame_position + GetFrames(elapsed_us);
}

// TODO: Break down the function into manageable pieces.
void ContinuousAudioTrackSink::AudioThreadFunc() {
  JNIEnv* env = base::android::AttachCurrentThread();
  bool was_playing = false;
  int frames_in_audio_track = 0;

  SB_LOG(INFO) << "ContinuousAudioTrackSink thread started.";

  int64_t last_playback_head_event_at = -1;  // microseconds
  int last_playback_head_position = 0;

  bool is_initial_silence_feeding = true;
  int initial_silence_frames = 0;
  int dropped_frames = 0;
  int reported_dropped_frames = 0;
  int64_t silence_fed_at_us = 0;
  const int64_t start_us = CurrentMonotonicTime();
  bridge_.Play();
  const int64_t elapsed_us = CurrentMonotonicTime() - start_us;
  SB_LOG(INFO) << "Initial Play complete time(msec)=" << elapsed_us / 1'000;

  constexpr int kSilenceIntervalUs = 10'000;
  const int kSilenceIntervalFrames =
      kSilenceIntervalUs * sampling_frequency_hz_ / 1'000'000;
  std::optional<Timestamp> last_timestamp;
  const std::vector<uint8_t> silence_frames(
      channels_ * GetBytesPerSample(sample_type_) * kSilenceIntervalFrames);

  int64_t last_real_frame_head = 0;
  while (!quit_) {
    int64_t playback_head_position = 0;
    int64_t frames_consumed_at = 0;
    if (bridge_.GetAndResetHasAudioDeviceChanged(env)) {
      SB_LOG(INFO) << "Audio device changed, raising a capability changed "
                      "error to restart playback.";
      ReportError(true, "Audio device capability changed");
      break;
    }

    int64_t ignore_us;
    int real_frame_head = bridge_.GetAudioTimestamp(&ignore_us, env);
    if (!playback_started_ && real_frame_head != last_real_frame_head) {
      // SB_CHECK_NE(silence_fed_at_us, 0);
      int64_t elapsed_ms =
          silence_fed_at_us > 0
              ? (CurrentMonotonicTime() - silence_fed_at_us) / 1'000
              : 0;
      SB_LOG(INFO) << "audio playback started: frames_consumed="
                   << FormatWithDigitSeparators(real_frame_head -
                                                last_real_frame_head)
                   << ", elased after silence feed(msec)="
                   << (elapsed_ms != 0 ? std::to_string(elapsed_ms) : "(null)");
      playback_started_ = true;
    }
    last_real_frame_head = real_frame_head;

    if (is_initial_silence_feeding) {
      last_timestamp = GetTimestamp(env);
      // SB_LOG(INFO) << "Silence feeding: last_timestamp=" << last_timestamp;
    } else if (was_playing) {
      playback_head_position =
          bridge_.GetAudioTimestamp(&frames_consumed_at, env);
      if (playback_head_position < initial_silence_frames) {
        // SB_LOG(INFO)
        //    << "Still playing initial_silence_frames:
        //    playback_head_positions="
        //    << playback_head_position
        //    << ", initial_silence_frames=" << initial_silence_frames;
        playback_head_position = 0;
      } else {
        playback_head_position -= initial_silence_frames;
      }

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

      SB_CHECK(started_us_);
      int to_report_dropped_frames = 0;
      if (reported_dropped_frames < dropped_frames) {
        const int max_dropped_frames =
            std::min<int>(GetFrames(now - *started_us_), dropped_frames);
        SB_CHECK_GE(max_dropped_frames, reported_dropped_frames);
        to_report_dropped_frames = max_dropped_frames - reported_dropped_frames;
        SB_LOG_IF(INFO, to_report_dropped_frames > 0)
            << "to_report_dropped_frames="
            << FormatWithDigitSeparators(to_report_dropped_frames)
            << ", dropped_frames=" << FormatWithDigitSeparators(dropped_frames)
            << ", reported_dropped_frames="
            << FormatWithDigitSeparators(reported_dropped_frames);
        reported_dropped_frames += to_report_dropped_frames;
      }

      const int effective_frames_consumed =
          frames_consumed + to_report_dropped_frames;

      if (effective_frames_consumed != 0) {
        SB_CHECK_GE(effective_frames_consumed, 0);
        consume_frames_func_(effective_frames_consumed, frames_consumed_at,
                             context_);
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
      if (is_initial_silence_feeding) {
        if (!started_us_) {
          started_us_ = CurrentMonotonicTime();
        }

        is_initial_silence_feeding = false;
        const int64_t initial_silence_us =
            GetFramesDurationUs(initial_silence_frames);
        const int64_t not_played_silence_frames =
            initial_silence_frames - EstimateFramePosition(last_timestamp);
        SB_CHECK_GE(not_played_silence_frames, 0);

        dropped_frames = not_played_silence_frames;
        offset_in_frames += dropped_frames;

        SB_LOG(INFO) << "Switch to real audio: initial_silence(msec)="
                     << initial_silence_us / 1'000
                     << ", dropped_frames=" << dropped_frames
                     << ", dropped_frames(msec)="
                     << GetFramesDurationUs(dropped_frames) / 1'000;
      } else {
        bridge_.Play();
      }
    }

    if (!is_playing && is_initial_silence_feeding) {
      if (!last_timestamp) {
        if (initial_silence_frames >= initial_frames_) {
          // SB_LOG(INFO) << "Reached max initial silence(msec)="
          //             << GetFramesDurationUs(initial_silence_frames) / 1'000;
        } else {
          while (initial_silence_frames < initial_frames_) {
            int frames_to_feed =
                std::min(kSilenceIntervalFrames,
                         initial_frames_ - initial_silence_frames);
            frames_to_feed = kSilenceIntervalFrames;
            int written = WriteData(env, silence_frames.data(), frames_to_feed);
            // SB_CHECK_EQ(written, frames_to_feed);
            initial_silence_frames += written;
          }
          SB_LOG(INFO) << "Feeds initial silences(msec)="
                       << GetFramesDurationUs(initial_silence_frames) / 1'000
                       << ", frames=" << initial_silence_frames;
          silence_fed_at_us = CurrentMonotonicTime();
        }
      } else {
        const int64_t silence_frame_head =
            EstimateFramePosition(last_timestamp);
        const int64_t initial_silence_us =
            GetFramesDurationUs(initial_silence_frames);
        const int64_t current_silence_us =
            GetFramesDurationUs(silence_frame_head);
        const int64_t silence_to_play_us =
            initial_silence_us - current_silence_us;
        if (silence_to_play_us > 10'000) {
          // SB_LOG(INFO) << "Reached max running silence(msec)="
          //             << silence_to_play_us / 1'000;
        } else {
          initial_silence_frames += kSilenceIntervalFrames;
          WriteData(env, silence_frames.data(), kSilenceIntervalFrames);
          SB_LOG(INFO) << "Feeds silences interval(msec)="
                       << GetFramesDurationUs(kSilenceIntervalFrames) / 1'000
                       << ", frames=" << initial_silence_frames;
        }
      }
      usleep(10'000);
      continue;
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

int ContinuousAudioTrackSink::WriteData(JNIEnv* env,
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

int64_t ContinuousAudioTrackSink::GetFramesDurationUs(int64_t frames) const {
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

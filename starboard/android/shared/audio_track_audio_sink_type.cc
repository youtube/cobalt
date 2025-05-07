// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/audio_track_audio_sink_type.h"

#include <unistd.h>
#include <algorithm>
#include <string>
#include <vector>

#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/shared/pthread/thread_create_priority.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/common.h"

namespace {
starboard::android::shared::AudioTrackAudioSinkType*
    audio_track_audio_sink_type_;
}

namespace starboard {
namespace android {
namespace shared {
namespace {

using ::starboard::shared::starboard::media::GetBytesPerSample;

// The maximum number of frames that can be written to android audio track per
// write request. If we don't set this cap for writing frames to audio track,
// we will repeatedly allocate a large byte array which cannot be consumed by
// audio track completely.
const int kMaxFramesPerRequest = 65536;

// Most Android audio HAL updates audio time for A/V synchronization on audio
// sync frames. For example, audio HAL may try to render when it gets an entire
// sync frame and then update audio time. Shorter duration of sync frame
// improves the accuracy of audio time, especially at the beginning of a
// playback, as otherwise the audio time during the initial update may be too
// large (non zero) and results in dropped video frames.
const int64_t kMaxDurationPerRequestInTunnelMode = 16'000;  // 16ms

const size_t kSilenceFramesPerAppend = 1024;

const int kMaxRequiredFramesLocal = 16 * 1024;
const int kMaxRequiredFramesRemote = 32 * 1024;
const int kMaxRequiredFrames = kMaxRequiredFramesRemote;
const int kRequiredFramesIncrement = 4 * 1024;
const int kMinStablePlayedFrames = 12 * 1024;

const int kSampleFrequency22050 = 22050;
const int kSampleFrequency48000 = 48000;

void* IncrementPointerByBytes(void* pointer, size_t offset) {
  return static_cast<uint8_t*>(pointer) + offset;
}

int GetMaxFramesPerRequestForTunnelMode(int sampling_frequency_hz) {
  auto max_frames =
      kMaxDurationPerRequestInTunnelMode * sampling_frequency_hz / 1'000'000LL;
  return (max_frames + 15) / 16 * 16;  // align to 16
}

bool HasRemoteAudioOutput() {
  // SbPlayerBridge::GetAudioConfigurations() reads up to 32 configurations. The
  // limit here is to avoid infinite loop and also match
  // SbPlayerBridge::GetAudioConfigurations().
  const int kMaxAudioConfigurations = 32;
  SbMediaAudioConfiguration configuration;
  int index = 0;
  while (index < kMaxAudioConfigurations &&
         MediaCapabilitiesCache::GetInstance()->GetAudioConfiguration(
             index, &configuration)) {
    switch (configuration.connector) {
      case kSbMediaAudioConnectorUnknown:
      case kSbMediaAudioConnectorAnalog:
      case kSbMediaAudioConnectorBuiltIn:
      case kSbMediaAudioConnectorHdmi:
      case kSbMediaAudioConnectorSpdif:
      case kSbMediaAudioConnectorUsb:
        break;
      case kSbMediaAudioConnectorBluetooth:
      case kSbMediaAudioConnectorRemoteWired:
      case kSbMediaAudioConnectorRemoteWireless:
      case kSbMediaAudioConnectorRemoteOther:
        return true;
    }
    index++;
  }
  return false;
}

}  // namespace

AudioTrackAudioSink::AudioTrackAudioSink(
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
    int tunnel_mode_audio_session_id,
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
      max_frames_per_request_(
          tunnel_mode_audio_session_id == -1
              ? kMaxFramesPerRequest
              : GetMaxFramesPerRequestForTunnelMode(sampling_frequency_hz_)),
      context_(context),
      bridge_(kSbMediaAudioCodingTypePcm,
              sample_type,
              channels,
              sampling_frequency_hz,
              preferred_buffer_size_in_bytes,
              tunnel_mode_audio_session_id,
              is_web_audio) {
  SB_DCHECK(update_source_status_func_);
  SB_DCHECK(consume_frames_func_);
  SB_DCHECK(frame_buffer_);

  SB_LOG(INFO) << "Creating audio sink starts at " << start_time_;

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

  pthread_create(&audio_out_thread_, nullptr,
                 &AudioTrackAudioSink::ThreadEntryPoint, this);
  SB_DCHECK(audio_out_thread_ != 0);
}

AudioTrackAudioSink::~AudioTrackAudioSink() {
  quit_ = true;

  if (audio_out_thread_ != 0) {
    pthread_join(audio_out_thread_, NULL);
  }
}

void AudioTrackAudioSink::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(playback_rate >= 0.0);
  if (playback_rate != 0.0 && playback_rate != 1.0) {
    SB_NOTIMPLEMENTED() << "TODO: Only playback rates of 0.0 and 1.0 are "
                           "currently supported.";
    playback_rate = (playback_rate > 0.0) ? 1.0 : 0.0;
  }
  ScopedLock lock(mutex_);
  playback_rate_ = playback_rate;
}

// static
void* AudioTrackAudioSink::ThreadEntryPoint(void* context) {
  pthread_setname_np(pthread_self(), "audio_track_out");
  SB_DCHECK(context);
  ::starboard::shared::pthread::ThreadSetPriority(kSbThreadPriorityRealTime);

  AudioTrackAudioSink* sink = reinterpret_cast<AudioTrackAudioSink*>(context);
  sink->AudioThreadFunc();

  return NULL;
}

int64_t MeasureElapsedMs(std::function<void()> method) {
  int64_t start_us = CurrentMonotonicTime();
  method();
  int64_t end_us = CurrentMonotonicTime();
  return (end_us - start_us) / 1'000;
}

#define LOG_ELAPSED(name, method) \
  { SB_LOG(INFO) << name << " took " << MeasureElapsedMs(method) << " msec."; }

// TODO: Break down the function into manageable pieces.
void AudioTrackAudioSink::AudioThreadFunc() {
  JniEnvExt* env = JniEnvExt::Get();
  bool was_playing = false;
  int frames_in_audio_track = 0;

  SB_LOG(INFO) << "AudioTrackAudioSink thread started.";

  int accumulated_written_frames = 0;
  int64_t last_playback_head_event_at = -1;  // microseconds

  int last_playback_head_position = 0;

  const int silence_frames =
      std::min<int>(kSilenceFramesPerAppend, max_frames_per_request_);
  const int minimum_startup_frames = bridge_.GetStartThresholdInFrames();
  SB_LOG(INFO) << "minimum startup_frames=" << minimum_startup_frames
               << ", mininum_startup_frames(msec)="
               << GetFramesDurationUs(minimum_startup_frames) / 1'000
               << ", silice_frames=" << silence_frames
               << ", silence_frames(msec)="
               << GetFramesDurationUs(silence_frames) / 1'000;

  std::vector<uint8_t> silence_bytes(
      channels_ * GetBytesPerSample(sample_type_) * silence_frames);
  std::function<void()> WriteFrames = [&] {
    WriteData(env, silence_bytes.data(), silence_frames, 0);
  };
  std::function<void()> Cleanup = [&] {
    bridge_.PauseAndFlush();
    usleep(1'000'000);
  };
  std::function<void()> WriteMinimumStartupFrames = [&] {
    for (int offset = 0; offset < minimum_startup_frames;
         offset += silence_frames) {
      int frames_to_write =
          std::min(silence_frames, minimum_startup_frames - offset);
      int frames_written =
          WriteData(env, silence_bytes.data(), frames_to_write, 0);
      if (frames_written < 0) {
        SB_LOG(ERROR) << "Write failed: err=" << frames_written;
        return;
      }
      offset += frames_written;
      if (frames_to_write != frames_written) {
        SB_LOG(INFO) << "Partilly wrote data: frames_to_write="
                     << frames_to_write
                     << ", frames_written=" << frames_written;
        usleep(1'000);
      }
    }
  };

  std::function<int()> GetPlaybackHead = [&] {
    int64_t _;
    return bridge_.GetAudioTimestamp(&_, env);
  };

  std::function<void()> WaitUntilStartAndLog = [&] {
    int last_head = GetPlaybackHead();
    int64_t start_us = CurrentMonotonicTime();
    int elapsed_ms = 0;
    do {
      usleep(10'000);
      elapsed_ms = (CurrentMonotonicTime() - start_us) / 1'000;
      if (elapsed_ms > 1'000) {
        SB_LOG(INFO) << "playback head doesn't move: elapsed_ms=" << elapsed_ms;
        return;
      }
    } while (GetPlaybackHead() == last_head);

    int media_time_progress_ms =
        GetFramesDurationUs(GetPlaybackHead() - last_head) / 1'000;
    SB_LOG(INFO) << "Audio startup latency(msec)="
                 << (elapsed_ms - media_time_progress_ms)
                 << ", elapsed_ms=" << elapsed_ms
                 << ", media_time_progress_ms=" << media_time_progress_ms;
  };

  // #define TEST_1
  // #define TEST_2

#if defined(TEST_1)
  SB_LOG(INFO) << "Test #1.0: Initial play/pause";
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });

  SB_LOG(INFO) << "Test #1.1: Call play after 200 msec";
  LOG_ELAPSED("Wait(200 msec)", [&] { usleep(200'000); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });

  SB_LOG(INFO) << "Test #1.2: Call play after 400 msec";
  LOG_ELAPSED("Wait(400 msec)", [&] { usleep(400'000); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });

  SB_LOG(INFO) << "Test #1.3: Call play after 600 msec";
  LOG_ELAPSED("Wait(600 msec)", [&] { usleep(400'000); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });

  SB_LOG(INFO) << "Test #1.4: Call play after 800 msec";
  LOG_ELAPSED("Wait(800 msec)", [&] { usleep(800'000); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });

  SB_LOG(INFO) << "Test #1.5: Call play after 1'000 msec";
  LOG_ELAPSED("Wait(1,000 msec)", [&] { usleep(1'000'000); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });

  Cleanup();
#endif

#define TEST_1_1
#if defined(TEST_1_1)
  SB_LOG(INFO) << "Test #1.1: Prewarm/play/pause/delay/play";
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });

  Cleanup();
  SB_LOG(INFO) << "Test #1.1: Call play after 200 msec";
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });
  LOG_ELAPSED("Wait(200 msec)", [&] { usleep(200'000); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });

  Cleanup();
  SB_LOG(INFO) << "Test #1.1: Call play after 400 msec";
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });
  LOG_ELAPSED("Wait(400 msec)", [&] { usleep(400'000); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });

  Cleanup();
  SB_LOG(INFO) << "Test #1.1: Call play after 600 msec";
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });
  LOG_ELAPSED("Wait(600 msec)", [&] { usleep(600'000); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });

  Cleanup();
  SB_LOG(INFO) << "Test #1.1: Call play after 800 msec";
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });
  LOG_ELAPSED("Wait(800 msec)", [&] { usleep(800'000); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });

  Cleanup();
  SB_LOG(INFO) << "Test #1.1: Call play after 1,000 msec";
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });
  LOG_ELAPSED("Wait(1,000 msec)", [&] { usleep(1'000'000); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });

#endif

#if defined(TEST_2)
  SB_LOG(INFO) << "Test #2.1: WriteFrames immediately after Play()";
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  Cleanup();

  SB_LOG(INFO) << "Test #2.2: WriteFrames 200 msec after Play()";
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Wait(200 msec)", [&] { usleep(200'000); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  Cleanup();

  SB_LOG(INFO) << "Test #2.3: WriteFrames 400 msec after Play()";
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Wait(400 msec)", [&] { usleep(400'000); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  Cleanup();

  SB_LOG(INFO) << "Test #2.4: WriteFrames 600 msec after Play()";
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Wait(600 msec)", [&] { usleep(600'000); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  Cleanup();

  SB_LOG(INFO) << "Test #2.5: WriteFrames 800 msec after Play()";
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Wait(800 msec)", [&] { usleep(800'000); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  Cleanup();

  SB_LOG(INFO) << "Test #2.6: WriteFrames 1,000 msec after Play()";
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Wait(1,000 msec)", [&] { usleep(1'000'000); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  Cleanup();
#endif

#if 0
  SB_LOG(INFO) << "Test #3.1: WriteFrame latency after Flush()";
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Flush", [&] { bridge_.Flush(); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  Cleanup();

  SB_LOG(INFO) << "Test #4.1: Prewarm before calling Play()";
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  Cleanup();

  SB_LOG(INFO) << "Test #4.2: Prewarm before calling Play()";
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Play(); });
  LOG_ELAPSED("Wait(1,000msec)", [&] { usleep(1'000'000); });
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  Cleanup();

  SB_LOG(INFO) << "Test #5.1: Play -> Write -> Gap -> Write";
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  LOG_ELAPSED("Wait(400msec)", [&] { usleep(400'000); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  LOG_ELAPSED("Wait(600msec)", [&] { usleep(600'000); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  LOG_ELAPSED("Wait(800msec)", [&] { usleep(800'000); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  Cleanup();

  SB_LOG(INFO) << "Test #5.2: Play -> Write -> Pause -> Gap -> Write -> Play";
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  LOG_ELAPSED("Wait(400msec)", [&] { usleep(400'000); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });

  LOG_ELAPSED("Wait(600msec)", [&] { usleep(600'000); });
  LOG_ELAPSED("Write", [&] { WriteFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  Cleanup();
#endif

#if 0
  SB_LOG(INFO) << "Test #6 Flush results";
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  LOG_ELAPSED("Wait(100msec)", [&] { usleep(100'000); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  LOG_ELAPSED("Wait(50msec)", [&] { usleep(50'000); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  LOG_ELAPSED("Wait(50msec)", [&] { usleep(50'000); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Wait(50msec)", [&] { usleep(50'000); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  LOG_ELAPSED("Flush", [&] { bridge_.Flush(); });
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Wait(50msec)", [&] { usleep(50'000); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  Cleanup();

  SB_LOG(INFO) << "Test #6.1 Flush works synchronously";
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  LOG_ELAPSED("Wait(100msec)", [&] { usleep(100'000); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  LOG_ELAPSED("Wait(50msec)", [&] { usleep(50'000); });
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Wait(50msec)", [&] { usleep(50'000); });
  LOG_ELAPSED("Flush", [&] { bridge_.Flush(); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  LOG_ELAPSED("Wait(10msec)", [&] { usleep(10'000); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  LOG_ELAPSED("Wait(10msec)", [&] { usleep(10'000); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  LOG_ELAPSED("Wait(20msec)", [&] { usleep(20'000); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
#endif

#if 0
  Cleanup();


  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  WaitUntilStartAndLog();

  LOG_ELAPSED("Flush", [&] { bridge_.Flush(); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  LOG_ELAPSED("Write data again", [&] { WriteMinimumStartupFrames(); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  WaitUntilStartAndLog();
#endif

#if defined(TEST_7)
  SB_LOG(INFO) << "Test #7 AudioTimestamp after Flush doesn't move";
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  WaitUntilStartAndLog();

  LOG_ELAPSED("Pause", [&] { bridge_.Pause(); });
  SB_LOG(INFO) << "Playback head: frames=" << GetPlaybackHead()
               << ", msec=" << GetFramesDurationUs(GetPlaybackHead()) / 1'000;
  LOG_ELAPSED("Flush", [&] { bridge_.Flush(); });
  LOG_ELAPSED("Wait(10msec)", [&] { usleep(10'000); });
  LOG_ELAPSED("Prewarm", [&] { WriteMinimumStartupFrames(); });
  LOG_ELAPSED("Play", [&] { bridge_.Play(); });
  WaitUntilStartAndLog();
#endif

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
      SB_DCHECK(playback_head_position >= last_playback_head_position);

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
        SB_DCHECK(frames_consumed >= 0);
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
      ScopedLock lock(mutex_);
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
        std::min(expected_written_frames, max_frames_per_request_);

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
        const int silence_frames_per_append =
            std::min<int>(kSilenceFramesPerAppend, max_frames_per_request_);
        std::vector<uint8_t> silence_buffer(channels_ *
                                            GetBytesPerSample(sample_type_) *
                                            silence_frames_per_append);
        int64_t sync_time =
            start_time_ + GetFramesDurationUs(accumulated_written_frames);
        // Not necessary to handle error of WriteData(), as the audio has
        // reached the end of stream.
        WriteData(env, silence_buffer.data(), silence_frames_per_append,
                  sync_time);
      }

      usleep(10'000);
      continue;
    }
    SB_DCHECK(expected_written_frames > 0);
    int64_t sync_time =
        start_time_ + GetFramesDurationUs(accumulated_written_frames);
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
                  expected_written_frames, sync_time);
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
    accumulated_written_frames += written_frames;

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

int AudioTrackAudioSink::WriteData(JniEnvExt* env,
                                   const void* buffer,
                                   int expected_written_frames,
                                   int64_t sync_time) {
  int samples_written = 0;
  if (sample_type_ == kSbMediaAudioSampleTypeFloat32) {
    samples_written =
        bridge_.WriteSample(static_cast<const float*>(buffer),
                            expected_written_frames * channels_, env);
  } else if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated) {
    samples_written = bridge_.WriteSample(static_cast<const uint16_t*>(buffer),
                                          expected_written_frames * channels_,
                                          sync_time, env);
  } else {
    SB_NOTREACHED();
  }
  if (samples_written < 0) {
    // Error code returned as negative value, like kAudioTrackErrorDeadObject.
    return samples_written;
  }
  SB_DCHECK(samples_written % channels_ == 0);
  return samples_written / channels_;
}

void AudioTrackAudioSink::ReportError(bool capability_changed,
                                      const std::string& error_message) {
  SB_LOG(INFO) << "AudioTrackAudioSink error: " << error_message;
  if (error_func_) {
    error_func_(capability_changed, error_message, context_);
  }
}

int64_t AudioTrackAudioSink::GetFramesDurationUs(int frames) const {
  return frames * 1'000'000LL / sampling_frequency_hz_;
}

void AudioTrackAudioSink::SetVolume(double volume) {
  bridge_.SetVolume(volume);
}

int AudioTrackAudioSink::GetUnderrunCount() {
  return bridge_.GetUnderrunCount();
}

int AudioTrackAudioSink::GetStartThresholdInFrames() {
  return bridge_.GetStartThresholdInFrames();
}

// static
int AudioTrackAudioSinkType::GetMinBufferSizeInFrames(
    int channels,
    SbMediaAudioSampleType sample_type,
    int sampling_frequency_hz) {
  SB_DCHECK(audio_track_audio_sink_type_);

  return std::max(
      AudioTrackBridge::GetMinBufferSizeInFrames(sample_type, channels,
                                                 sampling_frequency_hz),
      audio_track_audio_sink_type_->GetMinBufferSizeInFramesInternal(
          channels, sample_type, sampling_frequency_hz));
}

AudioTrackAudioSinkType::AudioTrackAudioSinkType()
    : min_required_frames_tester_(kMaxRequiredFrames,
                                  kRequiredFramesIncrement,
                                  kMinStablePlayedFrames) {}

SbAudioSink AudioTrackAudioSinkType::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
    SbAudioSinkPrivate::ErrorFunc error_func,
    void* context) {
  const int64_t kStartTime = 0;
  // Disable tunnel mode.
  const int kTunnelModeAudioSessionId = -1;
  const bool kIsWebAudio = true;
  return Create(channels, sampling_frequency_hz, audio_sample_type,
                audio_frame_storage_type, frame_buffers, frames_per_channel,
                update_source_status_func, consume_frames_func, error_func,
                kStartTime, kTunnelModeAudioSessionId, kIsWebAudio, context);
}

SbAudioSink AudioTrackAudioSinkType::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
    SbAudioSinkPrivate::ErrorFunc error_func,
    int64_t start_media_time,
    int tunnel_mode_audio_session_id,
    bool is_web_audio,
    void* context) {
  int min_required_frames = SbAudioSinkGetMinBufferSizeInFrames(
      channels, audio_sample_type, sampling_frequency_hz);
  SB_DCHECK(frames_per_channel >= min_required_frames);
  int preferred_buffer_size_in_bytes =
      min_required_frames * channels * GetBytesPerSample(audio_sample_type);
  AudioTrackAudioSink* audio_sink = new AudioTrackAudioSink(
      this, channels, sampling_frequency_hz, audio_sample_type, frame_buffers,
      frames_per_channel, preferred_buffer_size_in_bytes,
      update_source_status_func, consume_frames_func, error_func,
      start_media_time, tunnel_mode_audio_session_id, is_web_audio, context);
  if (!audio_sink->IsAudioTrackValid()) {
    SB_DLOG(ERROR)
        << "AudioTrackAudioSinkType::Create failed to create audio track";
    Destroy(audio_sink);
    return kSbAudioSinkInvalid;
  }
  return audio_sink;
}

void AudioTrackAudioSinkType::TestMinRequiredFrames() {
  auto onMinRequiredFramesForWebAudioReceived =
      [&](int number_of_channels, SbMediaAudioSampleType sample_type,
          int sample_rate, int min_required_frames) {
        bool has_remote_audio_output = HasRemoteAudioOutput();
        SB_LOG(INFO) << "Received min required frames " << min_required_frames
                     << " for " << number_of_channels << " channels, "
                     << sample_rate << "hz, with "
                     << (has_remote_audio_output ? "remote" : "local")
                     << " audio output device.";
        ScopedLock lock(min_required_frames_map_mutex_);
        has_remote_audio_output_ = has_remote_audio_output;
        min_required_frames_map_[sample_rate] =
            std::min(min_required_frames, has_remote_audio_output_
                                              ? kMaxRequiredFramesRemote
                                              : kMaxRequiredFramesLocal);
      };

  SbMediaAudioSampleType sample_type = kSbMediaAudioSampleTypeFloat32;
  if (!SbAudioSinkIsAudioSampleTypeSupported(sample_type)) {
    sample_type = kSbMediaAudioSampleTypeInt16Deprecated;
    SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(sample_type));
  }
  min_required_frames_tester_.AddTest(
      2, sample_type, kSampleFrequency48000,
      onMinRequiredFramesForWebAudioReceived,
      std::max(8 * 1024, AudioTrackBridge::GetMinBufferSizeInFrames(
                             sample_type, 2, kSampleFrequency48000)));
  min_required_frames_tester_.AddTest(
      2, sample_type, kSampleFrequency22050,
      onMinRequiredFramesForWebAudioReceived,
      std::max(4 * 1024, AudioTrackBridge::GetMinBufferSizeInFrames(
                             sample_type, 2, kSampleFrequency22050)));
  min_required_frames_tester_.Start();
}

int AudioTrackAudioSinkType::GetMinBufferSizeInFramesInternal(
    int channels,
    SbMediaAudioSampleType sample_type,
    int sampling_frequency_hz) {
  bool has_remote_audio_output = HasRemoteAudioOutput();
  ScopedLock lock(min_required_frames_map_mutex_);
  if (has_remote_audio_output == has_remote_audio_output_) {
    // There's no audio output type change, we can use the numbers we got from
    // the tests at app launch.
    if (sampling_frequency_hz <= kSampleFrequency22050) {
      if (min_required_frames_map_.find(kSampleFrequency22050) !=
          min_required_frames_map_.end()) {
        return min_required_frames_map_[kSampleFrequency22050];
      }
    } else if (sampling_frequency_hz <= kSampleFrequency48000) {
      if (min_required_frames_map_.find(kSampleFrequency48000) !=
          min_required_frames_map_.end()) {
        return min_required_frames_map_[kSampleFrequency48000];
      }
    }
  }
  // We cannot find a matched result from our tests, or the audio output type
  // has changed. We use the default max required frames to avoid underruns.
  return has_remote_audio_output ? kMaxRequiredFramesRemote
                                 : kMaxRequiredFramesLocal;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

namespace starboard {
namespace shared {
namespace starboard {
namespace audio_sink {

// static
void SbAudioSinkImpl::PlatformInitialize() {
  SB_DCHECK(!audio_track_audio_sink_type_);
  audio_track_audio_sink_type_ =
      new ::starboard::android::shared::AudioTrackAudioSinkType;
  SetPrimaryType(audio_track_audio_sink_type_);
  EnableFallbackToStub();
  audio_track_audio_sink_type_->TestMinRequiredFrames();
}

// static
void SbAudioSinkImpl::PlatformTearDown() {
  SB_DCHECK(audio_track_audio_sink_type_ == GetPrimaryType());
  SetPrimaryType(NULL);
  delete audio_track_audio_sink_type_;
  audio_track_audio_sink_type_ = NULL;
}

}  // namespace audio_sink
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

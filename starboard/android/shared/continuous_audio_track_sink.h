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

#ifndef STARBOARD_ANDROID_SHARED_CONTINUOUS_AUDIO_TRACK_SINK_H_
#define STARBOARD_ANDROID_SHARED_CONTINUOUS_AUDIO_TRACK_SINK_H_

#include <pthread.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "starboard/android/shared/audio_track_bridge.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

namespace starboard {

struct AudioSinkState {
  int frames_in_audio_track = 0;
  bool is_playing = false;
  bool is_playing_silence = false;
};

struct AudioSourceState {
  int frames_in_buffer = 0;
  int offset_in_frames = 0;
  bool is_playing = false;
  bool is_eos_reached = false;
};

class ContinuousAudioTrackSink : public SbAudioSinkImpl {
 public:
  ContinuousAudioTrackSink(
      Type* type,
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType sample_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frames_per_channel,
      int preferred_buffer_size,
      SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      ConsumeFramesFunc consume_frames_func,
      SbAudioSinkPrivate::ErrorFunc error_func,
      bool is_web_audio,
      void* context);
  ~ContinuousAudioTrackSink() override;

  bool IsAudioTrackValid() const { return bridge_.is_valid(); }
  bool IsType(Type* type) override { return type_ == type; }
  void SetPlaybackRate(double playback_rate) override;

  void SetVolume(double volume) override;
  int GetUnderrunCount();
  int GetStartThresholdInFrames();

  int WriteData(JNIEnv* env, const void* buffer, int size);

 private:
  static void* ThreadEntryPoint(void* context);
  void AudioThreadFunc();

  void PrimeSilenceBuffer(JNIEnv* env);
  void AppendSilence(
      JNIEnv* env,
      std::optional<AudioTrackBridge::AudioTimestamp> last_timestamp);

  std::optional<AudioTrackBridge::AudioTimestamp> ReportConsumedFramesToSource(
      JNIEnv* env,
      AudioSinkState* state,
      const std::optional<AudioTrackBridge::AudioTimestamp> new_timestamp,
      const std::optional<AudioTrackBridge::AudioTimestamp> last_timestamp);

  AudioSourceState GetAudioSourceState();

  void UpdatePlaybackState(JNIEnv* env,
                           const AudioSourceState& source_state,
                           AudioSinkState* sink_state);
  void HandleInitialSilenceFeeding(
      JNIEnv* env,
      int* initial_silence_frames,
      int64_t* silence_fed_at_us,
      const std::optional<AudioTrackBridge::AudioTimestamp>& last_timestamp,
      const std::vector<uint8_t>& silence_frames);
  std::optional<int64_t> WriteAudioData(JNIEnv* env,
                                        const AudioSourceState& source_state,
                                        AudioSinkState* sink_state);
  void CheckForPlaybackStart(JNIEnv* env,
                             int64_t* last_real_frame_head,
                             int64_t silence_fed_at_us);

  void ReportError(bool capability_changed, const std::string& error_message);

  int64_t EstimateFramePosition(
      const std::optional<AudioTrackBridge::AudioTimestamp>& timestamp,
      int64_t time_us) const;

  int64_t GetFramesUs(int64_t frames) const;
  int64_t GetFramesMs(int64_t frames) const {
    return GetFramesUs(frames) / 1'000;
  }
  int64_t GetFrames(int64_t duration_us) const;

  Type* const type_;
  const int channels_;
  const int sampling_frequency_hz_;
  const SbMediaAudioSampleType sample_type_;
  const int frame_bytes_;
  uint8_t* frame_buffer_;
  const int source_buffer_frames_;
  const SbAudioSinkUpdateSourceStatusFunc update_source_status_func_;
  const ConsumeFramesFunc consume_frames_func_;
  const SbAudioSinkPrivate::ErrorFunc error_func_;
  void* const context_;

  AudioTrackBridge bridge_;
  const int initial_frames_;

  bool quit_ = false;
  std::optional<pthread_t> audio_out_thread_;

  std::mutex mutex_;
  double playback_rate_ = 1.0;  // Guared by |mutex_|.

  int initial_silence_frames_ = 0;
  const int silence_buffer_frames_;
  const std::vector<uint8_t> silence_buffer_;

  bool playback_started_ = false;
  int playback_frame_head_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_CONTINUOUS_AUDIO_TRACK_SINK_H_

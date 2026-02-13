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

#ifndef STARBOARD_ANDROID_SHARED_AUDIO_TRACK_AUDIO_SINK_TYPE_H_
#define STARBOARD_ANDROID_SHARED_AUDIO_TRACK_AUDIO_SINK_TYPE_H_

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "starboard/android/shared/audio_sink_min_required_frames_tester.h"
#include "starboard/android/shared/audio_track_bridge.h"
#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/common/thread.h"
#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

namespace starboard {

class AudioTrackAudioSinkType : public SbAudioSinkPrivate::Type {
 public:
  static int GetMinBufferSizeInFrames(int channels,
                                      SbMediaAudioSampleType sample_type,
                                      int sampling_frequency_hz);

  AudioTrackAudioSinkType();

  SbAudioSink Create(
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType audio_sample_type,
      SbMediaAudioFrameStorageType audio_frame_storage_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frames_per_channel,
      SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
      SbAudioSinkPrivate::ErrorFunc error_func,
      void* context) override;
  SbAudioSink Create(
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType audio_sample_type,
      SbMediaAudioFrameStorageType audio_frame_storage_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frames_per_channel,
      SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
      SbAudioSinkPrivate::ErrorFunc error_func,
      int64_t start_time,
      int tunnel_mode_audio_session_id,
      bool is_web_audio,
      void* context);

  bool IsValid(SbAudioSink audio_sink) override {
    return audio_sink != kSbAudioSinkInvalid && audio_sink->IsType(this);
  }

  void Destroy(SbAudioSink audio_sink) override {
    // TODO(b/330793785): Use audio_sink.flush() instead of re-creating a new
    // audio_sink.
    if (audio_sink != kSbAudioSinkInvalid && !IsValid(audio_sink)) {
      SB_LOG(WARNING) << "audio_sink is invalid.";
      return;
    }
    delete audio_sink;
  }

  void TestMinRequiredFrames();

 private:
  int GetMinBufferSizeInFramesInternal(int channels,
                                       SbMediaAudioSampleType sample_type,
                                       int sampling_frequency_hz);

  std::mutex min_required_frames_map_mutex_;
  // The minimum frames required to avoid underruns of different frequencies.
  std::map<int, int> min_required_frames_map_;
  MinRequiredFramesTester min_required_frames_tester_;
  bool has_remote_audio_output_ = false;
};

class AudioTrackAudioSink : public SbAudioSinkImpl {
 public:
  static std::unique_ptr<AudioTrackAudioSink> Create(
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
      int64_t start_media_time,
      int tunnel_mode_audio_session_id,
      bool is_web_audio,
      void* context);

  AudioTrackAudioSink(
      PassKey<AudioTrackAudioSink>,
      std::unique_ptr<AudioTrackBridge> bridge,
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
      int64_t start_media_time,
      int tunnel_mode_audio_session_id,
      bool is_web_audio,
      void* context);
  ~AudioTrackAudioSink() override;

  bool IsType(Type* type) override { return type_ == type; }
  void SetPlaybackRate(double playback_rate) override;

  void SetVolume(double volume) override;
  int GetUnderrunCount();
  int GetStartThresholdInFrames();

 private:
  class AudioTrackOutThread;

  void AudioThreadFunc();

  int WriteData(JNIEnv* env, const void* buffer, int size, int64_t sync_time);

  void ReportError(bool capability_changed, const std::string& error_message);

  int64_t GetFramesDurationUs(int frames) const;

  Type* const type_;
  const int channels_;
  const int sampling_frequency_hz_;
  const SbMediaAudioSampleType sample_type_;
  void* frame_buffer_;
  const int frames_per_channel_;
  const SbAudioSinkUpdateSourceStatusFunc update_source_status_func_;
  const ConsumeFramesFunc consume_frames_func_;
  const SbAudioSinkPrivate::ErrorFunc error_func_;
  const int64_t start_time_;  // microseconds
  const int max_frames_per_request_;
  void* const context_;

  const std::unique_ptr<AudioTrackBridge> bridge_;

  volatile bool quit_ = false;
  const std::unique_ptr<Thread> audio_out_thread_;

  std::mutex mutex_;
  double playback_rate_ = 1.0;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AUDIO_TRACK_AUDIO_SINK_TYPE_H_

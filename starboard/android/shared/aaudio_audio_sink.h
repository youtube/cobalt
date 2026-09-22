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

#ifndef STARBOARD_ANDROID_SHARED_AAUDIO_AUDIO_SINK_H_
#define STARBOARD_ANDROID_SHARED_AAUDIO_AUDIO_SINK_H_

#include <aaudio/AAudio.h>

#include <atomic>
#include <cstdint>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "starboard/android/shared/android_audio_sink.h"
#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/common/pass_key.h"
#include "starboard/media.h"

// Suppress availability warnings for API 26+ AAudio symbols.
#pragma clang diagnostic ignored "-Wunguarded-availability"

namespace starboard {

// Factory and type descriptor for pull-based AAudio sinks.
//
// Expected lifetime / ownership:
// Singleton instance accessed via AaudioAudioSinkType::GetInstance().
//
// Threading model:
// Methods are expected to be called on the player worker thread.
class AaudioAudioSinkType : public SbAudioSinkPrivate::Type {
 public:
  static AaudioAudioSinkType* GetInstance();
  static bool IsSupported(SbMediaAudioSampleType sample_type);
  static bool IsEnabled();
  static void SetEnabled(bool enabled);

  AaudioAudioSinkType() = default;

  struct Callbacks {
    SbAudioSinkUpdateSourceStatusFunc update_source_status;
    SbAudioSinkPrivate::ConsumeFramesFunc consume_frames;
    SbAudioSinkPrivate::ErrorFunc error;
  };

  SbAudioSink Create(
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType audio_sample_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frames_per_channel,
      SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
      SbAudioSinkPrivate::ErrorFunc error_func,
      void* context) override;

  SbAudioSink Create(int channels,
                     int sampling_frequency_hz,
                     SbMediaAudioSampleType audio_sample_type,
                     SbAudioSinkFrameBuffers frame_buffers,
                     int frames_per_channel,
                     Callbacks callbacks,
                     int64_t start_time,
                     bool is_web_audio,
                     void* context);

  bool IsValid(SbAudioSink audio_sink) override {
    return audio_sink != kSbAudioSinkInvalid && audio_sink->IsType(this);
  }

  void Destroy(SbAudioSink audio_sink) override {
    if (audio_sink != kSbAudioSinkInvalid && !IsValid(audio_sink)) {
      SB_LOG(WARNING) << "audio_sink is invalid.";
      return;
    }
    delete audio_sink;
  }
};

// AaudioAudioSink is a pull-based audio sink implementation using the
// Android NDK AAudio data callback API.
//
// Audio frames are pulled directly by AAudio on its real-time OS audio thread
// whenever the audio hardware is ready for new data, minimizing startup
// and switch latency.
//
// Expected lifetime / ownership:
// Instances are created via AaudioAudioSinkType::Create() and owned by
// AudioRendererSinkAndroid on the player worker thread.
//
// Threading model:
// - Control methods (SetVolume, SetPlaybackRate, SetStartTime, Flush, etc.)
//   are called on the Cobalt player worker thread.
// - Real-time audio data requests (AudioDataCallback, OnAudioData) execute on
//   the dedicated AAudio OS callback thread. Atomic primitives are used to
//   synchronize state without blocking the audio thread.
class AaudioAudioSink final : public AndroidAudioSink {
 public:
  static std::unique_ptr<AaudioAudioSink> Create(
      Type* type,
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType sample_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frames_per_channel,
      AaudioAudioSinkType::Callbacks callbacks,
      int64_t start_media_time,
      bool is_web_audio,
      void* context);

  AaudioAudioSink(PassKey<AaudioAudioSink>,
                  Type* type,
                  int channels,
                  int sampling_frequency_hz,
                  SbMediaAudioSampleType sample_type,
                  SbAudioSinkFrameBuffers frame_buffers,
                  int frames_per_channel,
                  AaudioAudioSinkType::Callbacks callbacks,
                  int64_t start_media_time,
                  void* context);
  ~AaudioAudioSink() override;

  bool IsType(Type* type) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(double volume) override;
  int GetUnderrunCount() override;
  int GetStartThresholdInFrames() override;
  bool Flush() override;
  void SetStartTime(int64_t start_time) override;

 private:
  struct AAudioStreamDeleter {
    void operator()(AAudioStream* stream) const;
  };

  static aaudio_data_callback_result_t AudioDataCallback(AAudioStream* stream,
                                                         void* user_data,
                                                         void* audio_data,
                                                         int32_t num_frames);

  static void AudioErrorCallback(AAudioStream* stream,
                                 void* user_data,
                                 aaudio_result_t error);

  aaudio_data_callback_result_t OnAudioData(void* audio_data,
                                            int32_t num_frames);
  void OnAudioError(aaudio_result_t error);

  const raw_ptr<Type> type_;
  std::unique_ptr<AAudioStream, AAudioStreamDeleter> stream_;
  const int channels_;
  const raw_ptr<void> frame_buffer_;
  const int frames_per_channel_;
  const AaudioAudioSinkType::Callbacks callbacks_;
  const raw_ptr<void> context_;

  std::atomic<int64_t> start_time_{0};
  std::atomic<float> volume_{1.0f};
  std::atomic<float> playback_rate_{1.0f};
  std::atomic_bool flush_requested_{false};
  std::atomic_bool quit_{false};

  const int64_t created_at_;
  std::atomic<int64_t> flush_requested_at_{-1};
  bool first_frame_rendered_ = false;
  int64_t startup_latency_us_ = -1;
  int64_t total_switch_latency_us_ = 0;
  int switch_count_ = 0;
  int64_t total_callbacks_ = 0;
  int64_t total_callback_duration_us_ = 0;
  int64_t total_silence_frames_ = 0;
  int64_t total_audio_frames_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AAUDIO_AUDIO_SINK_H_

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
#include <mutex>

#include "starboard/android/shared/audio_sink_android.h"
#include "starboard/audio_sink.h"
#include "starboard/common/pass_key.h"
#include "starboard/media.h"

namespace starboard {

// AaudioAudioSink is a pull-based audio sink implementation using the
// Android NDK AAudio data callback API.
//
// Audio frames are pulled directly by AAudio on its real-time OS audio thread
// whenever the audio hardware is ready for new data, minimizing startup
// and switch latency.
//
// Expected lifetime / ownership:
// Instances are created via AaudioAudioSink::Create() and owned by
// AudioRendererSinkAndroid on the player worker thread.
//
// Threading model:
// - Control methods (SetVolume, SetPlaybackRate, SetStartTime, Flush, etc.)
//   are called on the Cobalt player worker thread.
// - Real-time audio data requests execute on the dedicated AAudio OS callback
//   thread. Atomic primitives are used to synchronize state without blocking
//   the audio thread.
class AaudioAudioSink final : public AudioSinkAndroid {
 public:
  struct Callbacks {
    SbAudioSinkUpdateSourceStatusFunc update_source_status;
    SbAudioSinkPrivate::ConsumeFramesFunc consume_frames;
    SbAudioSinkPrivate::ErrorFunc error;
  };

  static bool IsSupported(SbMediaAudioSampleType sample_type);

  static std::unique_ptr<AaudioAudioSink> Create(
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType sample_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frames_per_channel,
      Callbacks callbacks,
      bool is_web_audio,
      void* context);

  AaudioAudioSink(PassKey<AaudioAudioSink>,
                  int channels,
                  SbAudioSinkFrameBuffers frame_buffers,
                  int frames_per_channel,
                  Callbacks callbacks,
                  void* context);
  ~AaudioAudioSink() override;

  bool IsType(Type* type) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(double volume) override;
  bool Flush() override;
  void SetStartTime(int64_t start_time_us) override;

  aaudio_data_callback_result_t OnAudioData(void* audio_data,
                                            int32_t num_frames);
  void OnAudioError(aaudio_result_t error);

 private:
  struct AAudioStreamDeleter {
    void operator()(AAudioStream* stream) const;
  };

  std::unique_ptr<AAudioStream, AAudioStreamDeleter> stream_;
  const int channels_;
  void* const frame_buffer_;
  const int frames_per_channel_;
  const Callbacks callbacks_;
  void* const context_;

  std::mutex flush_mutex_;
  std::atomic<float> volume_ = 1.0f;
  std::atomic<float> playback_rate_ = 1.0f;
  std::atomic_bool flush_requested_ = false;
  std::atomic_bool quit_ = false;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AAUDIO_AUDIO_SINK_H_

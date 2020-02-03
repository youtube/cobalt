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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_SINK_ANDROID_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_SINK_ANDROID_H_

#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class AudioRendererSinkAndroid : public AudioRendererSink {
 public:
  AudioRendererSinkAndroid();
  ~AudioRendererSinkAndroid() override;

 private:
  // AudioRendererSink methods
  bool IsAudioSampleTypeSupported(
      SbMediaAudioSampleType audio_sample_type) const override;
  bool IsAudioFrameStorageTypeSupported(
      SbMediaAudioFrameStorageType audio_frame_storage_type) const override;
  int GetNearestSupportedSampleFrequency(int sampling_frequency_hz) const
      override;

  bool HasStarted() const override;
  void Start(int channels,
             int sampling_frequency_hz,
             SbMediaAudioSampleType audio_sample_type,
             SbMediaAudioFrameStorageType audio_frame_storage_type,
             SbAudioSinkFrameBuffers frame_buffers,
             int frames_per_channel,
             RenderCallback* render_callback) override;
  void Stop() override;

  void SetVolume(double volume) override;
  void SetPlaybackRate(double playback_rate) override;

  void UpdatePlaybackStatusIfPossible();

  // SbAudioSink callbacks
  static void UpdateSourceStatusFunc(int* frames_in_buffer,
                                     int* offset_in_frames,
                                     bool* is_playing,
                                     bool* is_eos_reached,
                                     void* context);
  static void ConsumeFramesFunc(int frames_consumed,
#if SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
                                SbTime frames_consumed_at,
#endif  // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
                                void* context);

  ThreadChecker thread_checker_;
  SbAudioSinkPrivate* audio_sink_;
  RenderCallback* render_callback_;
  double playback_rate_;
  double volume_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  //  STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_SINK_ANDROID_H_

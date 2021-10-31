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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MOCK_AUDIO_RENDERER_SINK_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MOCK_AUDIO_RENDERER_SINK_H_

#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"

#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class MockAudioRendererSink : public AudioRendererSink {
 public:
  MOCK_CONST_METHOD1(IsAudioSampleTypeSupported,
                     bool(SbMediaAudioSampleType audio_sample_type));
  MOCK_CONST_METHOD1(
      IsAudioFrameStorageTypeSupported,
      bool(SbMediaAudioFrameStorageType audio_frame_storage_type));
  MOCK_CONST_METHOD1(GetNearestSupportedSampleFrequency,
                     int(int sampling_frequency_hz));
  MOCK_CONST_METHOD0(HasStarted, bool());
  MOCK_METHOD8(Start,
               void(SbTime media_start_time,
                    int channels,
                    int sampling_frequency_hz,
                    SbMediaAudioSampleType audio_sample_type,
                    SbMediaAudioFrameStorageType audio_frame_storage_type,
                    SbAudioSinkFrameBuffers frame_buffers,
                    int frames_per_channel,
                    RenderCallback* render_callback));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(SetPlaybackRate, void(double playback_rate));

  void SetHasStarted(bool started) { has_started_ = started; }
  const bool* HasStartedPointer() { return &has_started_; }

 private:
  bool has_started_ = false;
  SbMediaAudioSampleType audio_sample_type_ = kSbMediaAudioSampleTypeFloat32;
  SbMediaAudioFrameStorageType audio_frame_storage_type_ =
      kSbMediaAudioFrameStorageTypeInterleaved;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MOCK_AUDIO_RENDERER_SINK_H_

// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_SINK_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_SINK_H_

#include "starboard/audio_sink.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// The interface used by AudioRendererImpl to output audio samples.
class AudioRendererSink {
 public:
  class RenderCallback {
   public:
    virtual void GetSourceStatus(int* frames_in_buffer,
                                 int* offset_in_frames,
                                 bool* is_playing,
                                 bool* is_eos_reached) = 0;
#if SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
    virtual void ConsumeFrames(int frames_consumed,
                               SbTime frames_consumed_at) = 0;
#else   //  SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
    virtual void ConsumeFrames(int frames_consumed) = 0;
#endif  // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)

   protected:
    ~RenderCallback() {}
  };

  virtual ~AudioRendererSink() {}

  virtual bool IsAudioSampleTypeSupported(
      SbMediaAudioSampleType audio_sample_type) const = 0;
  virtual bool IsAudioFrameStorageTypeSupported(
      SbMediaAudioFrameStorageType audio_frame_storage_type) const = 0;
  virtual int GetNearestSupportedSampleFrequency(
      int sampling_frequency_hz) const = 0;

  virtual bool HasStarted() const = 0;
  virtual void Start(int channels,
                     int sampling_frequency_hz,
                     SbMediaAudioSampleType audio_sample_type,
                     SbMediaAudioFrameStorageType audio_frame_storage_type,
                     SbAudioSinkFrameBuffers frame_buffers,
                     int frames_per_channel,
                     RenderCallback* render_callback) = 0;
  virtual void Stop() = 0;

  virtual void SetVolume(double volume) = 0;
  virtual void SetPlaybackRate(double playback_rate) = 0;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_SINK_H_

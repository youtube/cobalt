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

#ifndef STARBOARD_ANDROID_SHARED_AUDIO_RENDERER_SINK_ANDROID_H_
#define STARBOARD_ANDROID_SHARED_AUDIO_RENDERER_SINK_ANDROID_H_

#include <optional>

#include "starboard/android/shared/audio_sink_android.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"

namespace starboard {

// AudioRendererSinkAndroid is an implementation of AudioRendererSink for
// Android. It manages the lifecycle of the underlying AudioSinkAndroid,
// allowing it to be flushed and reused during seek operations to avoid
// recreating the track.
//
// Lifetime and Ownership:
// It is typically owned by the AudioRenderer and its lifetime is bound to the
// playback session.
//
// Threading Model:
// This class is not thread-safe and is expected to be called from the player
// thread.
class AudioRendererSinkAndroid final : public AudioRendererSinkImpl {
 public:
  AudioRendererSinkAndroid(
      const AudioStreamInfo& audio_stream_info,
      std::optional<int> tunnel_mode_audio_session_id,
      bool allow_audio_writing_on_pause,
      bool enable_video_renderer_vsp_adjustment,
      bool allow_flush_during_seek,
      bool pause_using_audio_track_state,
      CreateAudioSinkFunc create_audio_sink_func = CreateAudioSinkFunc());

  bool AllowOverflowAudioSamples() const override;
  bool AllowDirectPlaybackRateSetting() const override;
  bool HasStarted() const override;

  void GetAudioRendererParams(const AudioStreamInfo& audio_stream_info,
                              int* max_cached_frames,
                              int* min_frames_per_append) const override;

  int GetOutputNumberOfChannels(int number_of_channels) const override;

  void Start(int64_t media_start_time,
             int channels,
             int sampling_frequency_hz,
             SbMediaAudioSampleType audio_sample_type,
             SbAudioSinkFrameBuffers frame_buffers,
             int frames_per_channel,
             RenderCallback* render_callback) override;

  void Reset() override;
  void Stop() override;

 private:
  struct AudioFormat {
    int channels = -1;
    SbMediaAudioSampleType sample_type = kSbMediaAudioSampleTypeInt16Deprecated;
    int sampling_frequency_hz = -1;

    // Not defaulted, as defaulted comparison operators require C++20, while
    // some platforms (e.g. AOSP) build Starboard with C++17.
    bool operator==(const AudioFormat& other) const {
      return channels == other.channels && sample_type == other.sample_type &&
             sampling_frequency_hz == other.sampling_frequency_hz;
    }
  };

  // Returns the output format required by the platform for
  // |audio_stream_info|.  Currently it's only called when tunnel mode is
  // enabled: in tunnel mode (FLAG_HW_AV_SYNC), Android's software mixer and
  // resampler are bypassed, so the audio has to be converted to a format that
  // the hardware supports.
  // Note that AC3 and E-AC3 audio is played through AudioRendererPassthrough
  // instead of AudioRendererSinkAndroid, so it is never passed in here.
  static AudioFormat GetPlatformRequiredFormat(
      const AudioStreamInfo& audio_stream_info);

  bool IsAudioSampleTypeSupported(
      SbMediaAudioSampleType audio_sample_type) const override;
  int GetNearestSupportedSampleFrequency(
      int sampling_frequency_hz) const override;

  const bool is_tunnel_mode_enabled_;
  const bool enable_video_renderer_vsp_adjustment_;
  const bool allow_flush_during_seek_;
  // The output format required by the platform. Currently it's only set when
  // tunnel mode is enabled. To avoid duplicated calculations, it's calculated
  // once in the ctor, and then returned by GetOutputNumberOfChannels(),
  // IsAudioSampleTypeSupported() and GetNearestSupportedSampleFrequency().
  const std::optional<AudioFormat> platform_required_format_;

  bool is_flushed_ = false;

  // The format of the currently started audio sink.
  AudioFormat current_audio_format_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AUDIO_RENDERER_SINK_ANDROID_H_

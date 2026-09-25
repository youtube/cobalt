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
  struct Options {
    std::optional<int> tunnel_mode_audio_session_id;
    bool allow_audio_writing_on_pause = false;
    bool enable_video_renderer_vsp_adjustment = false;
    bool allow_flush_during_seek = false;
    bool pause_using_audio_track_state = false;
    bool enable_ndk_audio_pull_sink = false;
  };

  explicit AudioRendererSinkAndroid(
      const Options& options,
      CreateAudioSinkFunc create_audio_sink_func = CreateAudioSinkFunc());

  bool AllowOverflowAudioSamples() const override;
  bool AllowDirectPlaybackRateSetting() const override;
  bool HasStarted() const override;

  void GetAudioRendererParams(const AudioStreamInfo& audio_stream_info,
                              int* max_cached_frames,
                              int* min_frames_per_append) const override;

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
  bool IsAudioSampleTypeSupported(
      SbMediaAudioSampleType audio_sample_type) const override;

  const bool is_tunnel_mode_enabled_;
  const bool enable_video_renderer_vsp_adjustment_;
  const bool allow_flush_during_seek_;

  bool is_flushed_ = false;

  int channels_ = -1;
  int sampling_frequency_hz_ = -1;
  SbMediaAudioSampleType audio_sample_type_ =
      kSbMediaAudioSampleTypeInt16Deprecated;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AUDIO_RENDERER_SINK_ANDROID_H_

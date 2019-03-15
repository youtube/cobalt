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

#include "starboard/log.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/punchout_video_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/stub_audio_decoder.h"
#include "starboard/shared/starboard/player/filter/stub_video_decoder.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class PlayerComponentsImpl : public PlayerComponents {
  void CreateAudioComponents(
      const AudioParameters& audio_parameters,
      scoped_ptr<AudioDecoder>* audio_decoder,
      scoped_ptr<AudioRendererSink>* audio_renderer_sink) override {
    SB_DCHECK(audio_decoder);
    SB_DCHECK(audio_renderer_sink);

    audio_decoder->reset(new StubAudioDecoder(audio_parameters.audio_header));
    audio_renderer_sink->reset(new AudioRendererSinkImpl);
  }

  void CreateVideoComponents(
      const VideoParameters& video_parameters,
      scoped_ptr<VideoDecoder>* video_decoder,
      scoped_ptr<VideoRenderAlgorithm>* video_render_algorithm,
      scoped_refptr<VideoRendererSink>* video_renderer_sink) override {
    const SbTime kVideoSinkRenderInterval = 10 * kSbTimeMillisecond;

    SB_DCHECK(video_decoder);
    SB_DCHECK(video_render_algorithm);
    SB_DCHECK(video_renderer_sink);

    video_decoder->reset(new StubVideoDecoder);
    video_render_algorithm->reset(new VideoRenderAlgorithmImpl);
    *video_renderer_sink = new PunchoutVideoRendererSink(
        video_parameters.player, kVideoSinkRenderInterval);
  }

  void GetAudioRendererParams(int* max_cached_frames,
                              int* max_frames_per_append) const override {
    SB_DCHECK(max_cached_frames);
    SB_DCHECK(max_frames_per_append);

    *max_cached_frames = 256 * 1024;
    *max_frames_per_append = 16384;
  }
};

// static
scoped_ptr<PlayerComponents> PlayerComponents::Create() {
  return make_scoped_ptr<PlayerComponents>(new PlayerComponentsImpl);
}

// static
bool VideoDecoder::OutputModeSupported(SbPlayerOutputMode output_mode,
                                       SbMediaVideoCodec codec,
                                       SbDrmSystem drm_system) {
  return output_mode == kSbPlayerOutputModePunchOut;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

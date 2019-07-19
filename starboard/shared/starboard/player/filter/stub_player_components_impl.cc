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

#include "starboard/common/log.h"
#include "starboard/shared/starboard/player/filter/player_components.h"

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
    CreateStubAudioComponents(audio_parameters, audio_decoder,
                              audio_renderer_sink);
  }

  void CreateVideoComponents(
      const VideoParameters& video_parameters,
      scoped_ptr<VideoDecoder>* video_decoder,
      scoped_ptr<VideoRenderAlgorithm>* video_render_algorithm,
      scoped_refptr<VideoRendererSink>* video_renderer_sink) override {
    CreateStubVideoComponents(video_parameters, video_decoder,
                              video_render_algorithm, video_renderer_sink);
  }

  void GetAudioRendererParams(int* max_cached_frames,
                              int* max_frames_per_append) const override {
    SB_DCHECK(max_cached_frames);
    SB_DCHECK(max_frames_per_append);

    *max_cached_frames = 128 * 1024;
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

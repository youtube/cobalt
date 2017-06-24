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

#include "starboard/shared/starboard/player/filter/player_components.h"

#include "starboard/android/shared/audio_decoder.h"
#include "starboard/android/shared/video_decoder.h"
#include "starboard/android/shared/video_renderer.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_impl_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// static
scoped_ptr<PlayerComponents> PlayerComponents::Create(
    const AudioParameters& audio_parameters,
    const VideoParameters& video_parameters) {
  using AudioDecoderImpl = ::starboard::android::shared::AudioDecoder;
  using VideoDecoderImpl = ::starboard::android::shared::VideoDecoder;
  using VideoRendererImpl = ::starboard::android::shared::VideoRenderer;

  AudioDecoderImpl* audio_decoder = new AudioDecoderImpl(
      audio_parameters.audio_codec, audio_parameters.audio_header,
      audio_parameters.drm_system);
  if (!audio_decoder->is_valid()) {
    delete audio_decoder;
    return scoped_ptr<PlayerComponents>(NULL);
  }

  VideoDecoderImpl* video_decoder = new VideoDecoderImpl(
      video_parameters.video_codec, video_parameters.drm_system,
      video_parameters.output_mode,
      video_parameters.decode_target_graphics_context_provider);

  if (!video_decoder->is_valid()) {
    delete video_decoder;
    return scoped_ptr<PlayerComponents>(NULL);
  }

  AudioRendererImpl* audio_renderer =
      new AudioRendererImpl(scoped_ptr<AudioDecoder>(audio_decoder).Pass(),
                            audio_parameters.audio_header);

  VideoRendererImpl* video_renderer =
      new VideoRendererImpl(scoped_ptr<VideoDecoderImpl>(video_decoder).Pass());

  return scoped_ptr<PlayerComponents>(
      new PlayerComponents(audio_renderer, video_renderer));
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

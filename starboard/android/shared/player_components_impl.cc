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
#include "starboard/android/shared/video_render_algorithm.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/shared/starboard/player/filter/audio_frame_tracker.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"

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
  using VideoRenderAlgorithmImpl =
      ::starboard::android::shared::VideoRenderAlgorithm;

  scoped_ptr<AudioDecoderImpl> audio_decoder(new AudioDecoderImpl(
      audio_parameters.audio_codec, audio_parameters.audio_header,
      audio_parameters.drm_system));
  if (!audio_decoder->is_valid()) {
    return scoped_ptr<PlayerComponents>(NULL);
  }

  scoped_ptr<VideoDecoderImpl> video_decoder(new VideoDecoderImpl(
      video_parameters.video_codec, video_parameters.drm_system,
      video_parameters.output_mode,
      video_parameters.decode_target_graphics_context_provider));

  if (!video_decoder->is_valid()) {
    return scoped_ptr<PlayerComponents>(NULL);
  }

  scoped_ptr<AudioRenderer> audio_renderer(new AudioRenderer(
      audio_decoder.PassAs<AudioDecoder>(),
      make_scoped_ptr<AudioRendererSink>(new AudioRendererSinkImpl),
      audio_parameters.audio_header));
  scoped_refptr<VideoRendererSink> video_renderer_sink =
      video_decoder->GetSink();
  scoped_ptr<VideoRenderer> video_renderer(new VideoRenderer(
      video_decoder.PassAs<VideoDecoder>(), audio_renderer.get(),
      make_scoped_ptr<VideoRenderAlgorithm>(new VideoRenderAlgorithmImpl),
      video_renderer_sink));

  return scoped_ptr<PlayerComponents>(
      new PlayerComponents(audio_renderer.Pass(), video_renderer.Pass()));
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

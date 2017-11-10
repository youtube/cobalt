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

#include "starboard/raspi/shared/open_max/video_decoder.h"
#include "starboard/raspi/shared/video_renderer_sink_impl.h"
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// static
scoped_ptr<PlayerComponents> PlayerComponents::Create(
    const AudioParameters& audio_parameters,
    const VideoParameters& video_parameters) {
  using AudioDecoderImpl = ::starboard::shared::ffmpeg::AudioDecoder;
  using VideoDecoderImpl = ::starboard::raspi::shared::open_max::VideoDecoder;
  using ::starboard::raspi::shared::VideoRendererSinkImpl;

  AudioDecoderImpl* audio_decoder = new AudioDecoderImpl(
      audio_parameters.audio_codec, audio_parameters.audio_header);
  if (!audio_decoder->is_valid()) {
    delete audio_decoder;
    return scoped_ptr<PlayerComponents>(NULL);
  }

  scoped_ptr<VideoDecoder> video_decoder(new VideoDecoderImpl(
      video_parameters.video_codec, video_parameters.job_queue));

  scoped_ptr<AudioRenderer> audio_renderer(new AudioRenderer(
      make_scoped_ptr<AudioDecoder>(audio_decoder),
      make_scoped_ptr<AudioRendererSink>(new AudioRendererSinkImpl),
      audio_parameters.audio_header));

  scoped_ptr<VideoRenderAlgorithm> algorithm(new VideoRenderAlgorithmImpl);
  scoped_ptr<VideoRenderer> video_renderer(new VideoRenderer(
      video_decoder.Pass(), audio_renderer.get(), algorithm.Pass(),
      new VideoRendererSinkImpl(video_parameters.player,
                                video_parameters.job_queue)));

  return scoped_ptr<PlayerComponents>(
      new PlayerComponents(audio_renderer.Pass(), video_renderer.Pass()));
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

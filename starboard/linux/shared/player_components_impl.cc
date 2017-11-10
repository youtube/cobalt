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

#include "starboard/audio_sink.h"
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"
#include "starboard/shared/ffmpeg/ffmpeg_video_decoder.h"
#include "starboard/shared/libvpx/vpx_video_decoder.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/punchout_video_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// static
scoped_ptr<PlayerComponents> PlayerComponents::Create(
    const AudioParameters& audio_parameters,
    const VideoParameters& video_parameters) {
  typedef ::starboard::shared::ffmpeg::AudioDecoder AudioDecoderImpl;
  typedef ::starboard::shared::ffmpeg::VideoDecoder FfmpegVideoDecoderImpl;
  typedef ::starboard::shared::vpx::VideoDecoder VpxVideoDecoderImpl;

  const SbTime kVideoSinkRenderInterval = 10 * kSbTimeMillisecond;

  // TODO: This is not ideal as we should really handle the creation failure of
  // audio sink inside the audio renderer to give the renderer a chance to
  // resample the decoded audio.
  const int audio_channels = audio_parameters.audio_header.number_of_channels;
  if (audio_channels > SbAudioSinkGetMaxChannels()) {
    return scoped_ptr<PlayerComponents>(NULL);
  }

  AudioDecoderImpl* audio_decoder = new AudioDecoderImpl(
      audio_parameters.audio_codec, audio_parameters.audio_header);
  if (!audio_decoder->is_valid()) {
    delete audio_decoder;
    return scoped_ptr<PlayerComponents>(NULL);
  }

  scoped_ptr<VideoDecoder> video_decoder;
  if (video_parameters.video_codec == kSbMediaVideoCodecVp9) {
    VpxVideoDecoderImpl* vpx_video_decoder = new VpxVideoDecoderImpl(
        video_parameters.video_codec, video_parameters.output_mode,
        video_parameters.decode_target_graphics_context_provider);
    video_decoder.reset(vpx_video_decoder);
  } else {
    FfmpegVideoDecoderImpl* ffmpeg_video_decoder = new FfmpegVideoDecoderImpl(
        video_parameters.video_codec, video_parameters.output_mode,
        video_parameters.decode_target_graphics_context_provider);
    if (!ffmpeg_video_decoder->is_valid()) {
      delete ffmpeg_video_decoder;
      return scoped_ptr<PlayerComponents>(NULL);
    }
    video_decoder.reset(ffmpeg_video_decoder);
  }

  scoped_ptr<AudioRenderer> audio_renderer(new AudioRenderer(
      make_scoped_ptr<AudioDecoder>(audio_decoder),
      make_scoped_ptr<AudioRendererSink>(new AudioRendererSinkImpl),
      audio_parameters.audio_header));
  scoped_ptr<VideoRenderer> video_renderer(new VideoRenderer(
      video_decoder.Pass(), audio_renderer.get(),
      make_scoped_ptr<VideoRenderAlgorithm>(new VideoRenderAlgorithmImpl),
      new PunchoutVideoRendererSink(video_parameters.player,
                                    kVideoSinkRenderInterval)));

  return scoped_ptr<PlayerComponents>(
      new PlayerComponents(audio_renderer.Pass(), video_renderer.Pass()));
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

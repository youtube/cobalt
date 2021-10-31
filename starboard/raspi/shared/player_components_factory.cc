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

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/raspi/shared/open_max/video_decoder.h"
#include "starboard/raspi/shared/video_renderer_sink_impl.h"
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"
#include "starboard/shared/opus/opus_audio_decoder.h"
#include "starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

class PlayerComponentsFactory : public PlayerComponents::Factory {
  bool CreateSubComponents(
      const CreationParameters& creation_parameters,
      scoped_ptr<AudioDecoder>* audio_decoder,
      scoped_ptr<AudioRendererSink>* audio_renderer_sink,
      scoped_ptr<VideoDecoder>* video_decoder,
      scoped_ptr<VideoRenderAlgorithm>* video_render_algorithm,
      scoped_refptr<VideoRendererSink>* video_renderer_sink,
      std::string* error_message) override {
    SB_DCHECK(error_message);

    if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
      SB_DCHECK(audio_decoder);
      SB_DCHECK(audio_renderer_sink);

      auto decoder_creator = [](const SbMediaAudioSampleInfo& audio_sample_info,
                                SbDrmSystem drm_system) {
        typedef ::starboard::shared::ffmpeg::AudioDecoder AudioDecoderImpl;
        typedef ::starboard::shared::opus::OpusAudioDecoder OpusAudioDecoderImpl;

        if(audio_sample_info.codec == kSbMediaAudioCodecOpus) {
          scoped_ptr<OpusAudioDecoderImpl> opus_audio_decoder_impl(
              new OpusAudioDecoderImpl(audio_sample_info));
          if (opus_audio_decoder_impl && opus_audio_decoder_impl->is_valid()) {
            return opus_audio_decoder_impl.PassAs<AudioDecoder>();
          }
        } else {
          scoped_ptr<AudioDecoderImpl> audio_decoder_impl(
              AudioDecoderImpl::Create(audio_sample_info.codec,
                                      audio_sample_info));
          if (audio_decoder_impl && audio_decoder_impl->is_valid()) {
            return audio_decoder_impl.PassAs<AudioDecoder>();
          }
        }
        return scoped_ptr<AudioDecoder>();
      };

      audio_decoder->reset(new AdaptiveAudioDecoder(
          creation_parameters.audio_sample_info(),
          creation_parameters.drm_system(), decoder_creator));
      audio_renderer_sink->reset(new AudioRendererSinkImpl);
    }

    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      using VideoDecoderImpl =
          ::starboard::raspi::shared::open_max::VideoDecoder;
      using ::starboard::raspi::shared::VideoRendererSinkImpl;

      SB_DCHECK(video_decoder);
      SB_DCHECK(video_render_algorithm);
      SB_DCHECK(video_renderer_sink);

      video_decoder->reset(
          new VideoDecoderImpl(creation_parameters.video_codec()));
      video_render_algorithm->reset(new VideoRenderAlgorithmImpl);
      *video_renderer_sink =
          new VideoRendererSinkImpl(creation_parameters.player());
    }

    return true;
  }
};

}  // namespace

// static
scoped_ptr<PlayerComponents::Factory> PlayerComponents::Factory::Create() {
  return make_scoped_ptr<PlayerComponents::Factory>(
      new PlayerComponentsFactory);
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

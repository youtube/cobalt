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
#include "starboard/raspi/shared/open_max/video_decoder.h"
#include "starboard/raspi/shared/video_renderer_sink_impl.h"
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"
#include "starboard/shared/opus/opus_audio_decoder.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"

namespace starboard {
namespace {

class PlayerComponentsFactory : public PlayerComponents::Factory {
  Result<PlayerComponents::Factory::MediaComponents> CreateSubComponents(
      const CreationParameters& creation_parameters) override {
    MediaComponents components;

    if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
      auto decoder_creator =
          [](const AudioStreamInfo& audio_stream_info,
             SbDrmSystem drm_system) -> std::unique_ptr<AudioDecoder> {
        if (audio_stream_info.codec == kSbMediaAudioCodecOpus) {
          auto opus_audio_decoder =
              std::make_unique<OpusAudioDecoder>(audio_stream_info);
          if (opus_audio_decoder && opus_audio_decoder->is_valid()) {
            return opus_audio_decoder;
          }
        } else {
          auto ffmpeg_audio_decoder = std::unique_ptr<FfmpegAudioDecoder>(
              FfmpegAudioDecoder::Create(audio_stream_info));
          if (ffmpeg_audio_decoder && ffmpeg_audio_decoder->is_valid()) {
            return ffmpeg_audio_decoder;
          }
        }
        return nullptr;
      };

      components.audio.decoder = std::make_unique<AdaptiveAudioDecoder>(
          creation_parameters.audio_stream_info(),
          creation_parameters.drm_system(), decoder_creator);
      components.audio.renderer_sink =
          std::make_unique<AudioRendererSinkImpl>();
    }

    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      components.video.decoder = std::make_unique<OpenMaxVideoDecoder>(
          creation_parameters.video_codec());
      components.video.render_algorithm =
          std::make_unique<VideoRenderAlgorithmImpl>();
      components.video.renderer_sink =
          make_scoped_refptr<VideoRendererSinkImpl>(
              creation_parameters.player());
    }

    return Result<MediaComponents>(std::move(components));
  }
};

}  // namespace

// static
std::unique_ptr<PlayerComponents::Factory> PlayerComponents::Factory::Create() {
  return std::make_unique<PlayerComponentsFactory>();
}

// static
bool PlayerComponents::Factory::OutputModeSupported(
    SbPlayerOutputMode output_mode,
    SbMediaVideoCodec codec,
    SbDrmSystem drm_system) {
  return output_mode == kSbPlayerOutputModePunchOut;
}

}  // namespace starboard

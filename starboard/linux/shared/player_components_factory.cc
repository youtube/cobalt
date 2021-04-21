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
#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/common/string.h"
#include "starboard/gles.h"
#include "starboard/media.h"
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"
#include "starboard/shared/ffmpeg/ffmpeg_video_decoder.h"
#include "starboard/shared/libdav1d/dav1d_video_decoder.h"
#include "starboard/shared/libde265/de265_video_decoder.h"
#include "starboard/shared/libvpx/vpx_video_decoder.h"
#include "starboard/shared/opus/opus_audio_decoder.h"
#include "starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/punchout_video_renderer_sink.h"
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
 public:
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

      typedef ::starboard::shared::ffmpeg::AudioDecoder FfmpegAudioDecoder;
      typedef ::starboard::shared::opus::OpusAudioDecoder OpusAudioDecoder;

      auto decoder_creator = [](const SbMediaAudioSampleInfo& audio_sample_info,
                                SbDrmSystem drm_system) {
        if (audio_sample_info.codec == kSbMediaAudioCodecOpus) {
          scoped_ptr<OpusAudioDecoder> audio_decoder_impl(
              new OpusAudioDecoder(audio_sample_info));
          if (audio_decoder_impl->is_valid()) {
            return audio_decoder_impl.PassAs<AudioDecoder>();
          }
        } else {
          scoped_ptr<FfmpegAudioDecoder> audio_decoder_impl(
              FfmpegAudioDecoder::Create(audio_sample_info.codec,
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
      typedef ::starboard::shared::libdav1d::VideoDecoder Av1VideoDecoderImpl;
      typedef ::starboard::shared::de265::VideoDecoder H265VideoDecoderImpl;
      typedef ::starboard::shared::ffmpeg::VideoDecoder FfmpegVideoDecoderImpl;
      typedef ::starboard::shared::vpx::VideoDecoder VpxVideoDecoderImpl;

      const SbTime kVideoSinkRenderInterval = 10 * kSbTimeMillisecond;

      SB_DCHECK(video_decoder);
      SB_DCHECK(video_render_algorithm);
      SB_DCHECK(video_renderer_sink);

      video_decoder->reset();

      const SbMediaVideoCodec kAv1VideoCodec = kSbMediaVideoCodecAv1;

      if (creation_parameters.video_codec() == kSbMediaVideoCodecVp9) {
        video_decoder->reset(new VpxVideoDecoderImpl(
            creation_parameters.video_codec(),
            creation_parameters.output_mode(),
            creation_parameters.decode_target_graphics_context_provider()));
      } else if (creation_parameters.video_codec() == kAv1VideoCodec) {
        video_decoder->reset(new Av1VideoDecoderImpl(
            creation_parameters.video_codec(),
            creation_parameters.output_mode(),
            creation_parameters.decode_target_graphics_context_provider()));
      } else if (creation_parameters.video_codec() == kSbMediaVideoCodecH265) {
        video_decoder->reset(new H265VideoDecoderImpl(
            creation_parameters.video_codec(),
            creation_parameters.output_mode(),
            creation_parameters.decode_target_graphics_context_provider()));
      } else {
        scoped_ptr<FfmpegVideoDecoderImpl> ffmpeg_video_decoder(
            FfmpegVideoDecoderImpl::Create(
                creation_parameters.video_codec(),
                creation_parameters.output_mode(),
                creation_parameters.decode_target_graphics_context_provider()));
        if (ffmpeg_video_decoder && ffmpeg_video_decoder->is_valid()) {
          video_decoder->reset(ffmpeg_video_decoder.release());
        } else {
          SB_LOG(ERROR) << "Failed to create video decoder for codec "
                        << creation_parameters.video_codec();
          *error_message =
              FormatString("Failed to create video decoder for codec %d.",
                           creation_parameters.video_codec());
          return false;
        }
      }

      video_render_algorithm->reset(new VideoRenderAlgorithmImpl([]() {
        return 60.;  // default refresh rate
      }));
      if (creation_parameters.output_mode() ==
          kSbPlayerOutputModeDecodeToTexture) {
        *video_renderer_sink = NULL;
      } else {
        *video_renderer_sink = new PunchoutVideoRendererSink(
            creation_parameters.player(), kVideoSinkRenderInterval);
      }
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
  bool has_gles_support = SbGetGlesInterface();

  if (!has_gles_support) {
    return output_mode == kSbPlayerOutputModePunchOut;
  }

#if defined(SB_FORCE_DECODE_TO_TEXTURE_ONLY)
  // Starboard lib targets may not draw directly to the window, so punch through
  // video is not made available.
  return output_mode == kSbPlayerOutputModeDecodeToTexture;
#endif  // defined(SB_FORCE_DECODE_TO_TEXTURE_ONLY)

  if (output_mode == kSbPlayerOutputModePunchOut ||
      output_mode == kSbPlayerOutputModeDecodeToTexture) {
    return true;
  }

  return false;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

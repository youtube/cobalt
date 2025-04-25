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

#include <memory>

#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/string.h"
#include "starboard/gles.h"
#include "starboard/media.h"
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"
#include "starboard/shared/ffmpeg/ffmpeg_video_decoder.h"
#include "starboard/shared/libdav1d/dav1d_video_decoder.h"
#include "starboard/shared/libde265/de265_video_decoder.h"
#include "starboard/shared/libfdkaac/fdk_aac_audio_decoder.h"
#include "starboard/shared/libfdkaac/libfdkaac_library_loader.h"
#include "starboard/shared/libvpx/vpx_video_decoder.h"
#include "starboard/shared/openh264/openh264_library_loader.h"
#include "starboard/shared/openh264/openh264_video_decoder.h"
#include "starboard/shared/opus/opus_audio_decoder.h"
#include "starboard/shared/starboard/media/media_util.h"
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

using ::starboard::shared::openh264::is_openh264_supported;

class PlayerComponentsFactory : public PlayerComponents::Factory {
 public:
  SubComponents CreateSubComponents(
      const CreationParameters& creation_parameters) override {
    AudioComponents audio_components;
    if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
      typedef ::starboard::shared::ffmpeg::AudioDecoder FfmpegAudioDecoder;
      typedef ::starboard::shared::opus::OpusAudioDecoder OpusAudioDecoder;
      typedef ::starboard::shared::libfdkaac::FdkAacAudioDecoder
          FdkAacAudioDecoder;

      auto decoder_creator =
          [](const media::AudioStreamInfo& audio_stream_info,
             SbDrmSystem drm_system) -> std::unique_ptr<AudioDecoder> {
        if (audio_stream_info.codec == kSbMediaAudioCodecOpus) {
          auto audio_decoder_impl =
              std::make_unique<OpusAudioDecoder>(audio_stream_info);
          if (audio_decoder_impl->is_valid()) {
            return audio_decoder_impl;
          } else {
            SB_LOG(ERROR) << "Failed to create audio decoder for codec "
                          << audio_stream_info.codec;
          }
        } else if (audio_stream_info.codec == kSbMediaAudioCodecAac &&
                   audio_stream_info.number_of_channels <=
                       FdkAacAudioDecoder::kMaxChannels &&
                   libfdkaac::LibfdkaacHandle::GetHandle()->IsLoaded()) {
          SB_LOG(INFO) << "Playing audio using FdkAacAudioDecoder.";
          return std::make_unique<FdkAacAudioDecoder>();
        } else {
          std::unique_ptr<FfmpegAudioDecoder> audio_decoder_impl(
              FfmpegAudioDecoder::Create(audio_stream_info));
          if (audio_decoder_impl && audio_decoder_impl->is_valid()) {
            SB_LOG(INFO) << "Playing audio using FfmpegAudioDecoder";
            return audio_decoder_impl;
          } else {
            SB_LOG(ERROR) << "Failed to create audio decoder for codec "
                          << audio_stream_info.codec;
          }
        }
        return nullptr;
      };

      audio_components.decoder = std::make_unique<AdaptiveAudioDecoder>(
          creation_parameters.audio_stream_info(),
          creation_parameters.drm_system(), decoder_creator);
      audio_components.renderer_sink =
          std::make_unique<AudioRendererSinkImpl>();
    }

    VideoComponents video_components;
    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      typedef ::starboard::shared::libdav1d::VideoDecoder Av1VideoDecoderImpl;
      typedef ::starboard::shared::de265::VideoDecoder H265VideoDecoderImpl;
      typedef ::starboard::shared::ffmpeg::VideoDecoder FfmpegVideoDecoderImpl;
      typedef ::starboard::shared::vpx::VideoDecoder VpxVideoDecoderImpl;
      typedef ::starboard::shared::openh264::VideoDecoder
          Openh264VideoDecoderImpl;

      const int64_t kVideoSinkRenderIntervalUsec = 10'000;

      const SbMediaVideoCodec kAv1VideoCodec = kSbMediaVideoCodecAv1;

      std::unique_ptr<VideoDecoder> video_decoder;
      if (creation_parameters.video_codec() == kSbMediaVideoCodecVp9) {
        video_decoder = std::make_unique<VpxVideoDecoderImpl>(
            creation_parameters.video_codec(),
            creation_parameters.output_mode(),
            creation_parameters.decode_target_graphics_context_provider());
      } else if (creation_parameters.video_codec() == kAv1VideoCodec) {
        video_decoder = std::make_unique<Av1VideoDecoderImpl>(
            creation_parameters.video_codec(),
            creation_parameters.output_mode(),
            creation_parameters.decode_target_graphics_context_provider());
      } else if (creation_parameters.video_codec() == kSbMediaVideoCodecH265) {
        video_decoder = std::make_unique<H265VideoDecoderImpl>(
            creation_parameters.video_codec(),
            creation_parameters.output_mode(),
            creation_parameters.decode_target_graphics_context_provider());
      } else if ((creation_parameters.video_codec() ==
                  kSbMediaVideoCodecH264) &&
                 is_openh264_supported()) {
        SB_LOG(INFO) << "Playing video using openh264::VideoDecoder.";
        video_decoder = std::make_unique<Openh264VideoDecoderImpl>(
            creation_parameters.video_codec(),
            creation_parameters.output_mode(),
            creation_parameters.decode_target_graphics_context_provider());
      } else {
        std::unique_ptr<FfmpegVideoDecoderImpl> ffmpeg_video_decoder(
            FfmpegVideoDecoderImpl::Create(
                creation_parameters.video_codec(),
                creation_parameters.output_mode(),
                creation_parameters.decode_target_graphics_context_provider()));
        if (ffmpeg_video_decoder && ffmpeg_video_decoder->is_valid()) {
          SB_LOG(INFO) << "Playing video using ffmpeg::VideoDecoder.";
          video_decoder = std::move(ffmpeg_video_decoder);
        } else {
          SB_LOG(ERROR) << "Failed to create video decoder for codec "
                        << creation_parameters.video_codec();
          return {
              .error_message =
                  FormatString("Failed to create video decoder for codec %d.",
                               creation_parameters.video_codec()),
          };
        }
      }

      auto video_render_algorithm =
          std::make_unique<VideoRenderAlgorithmImpl>([] {
            return 60.;  // default refresh rate
          });
      scoped_refptr<VideoRendererSink> video_renderer_sink;
      if (creation_parameters.output_mode() !=
          kSbPlayerOutputModeDecodeToTexture) {
        video_renderer_sink = make_scoped_refptr<PunchoutVideoRendererSink>(
            creation_parameters.player(), kVideoSinkRenderIntervalUsec);
      }

      video_components = VideoComponents{
          .decoder = std::move(video_decoder),
          .render_algorithm = std::move(video_render_algorithm),
          .renderer_sink = std::move(video_renderer_sink),
      };
    }

    return {
        .audio = std::move(audio_components),
        .video = std::move(video_components),
    };
  }
};

}  // namespace

// static
std::unique_ptr<PlayerComponents::Factory> PlayerComponents::Factory::Create() {
  return std::unique_ptr<PlayerComponents::Factory>(
      new PlayerComponentsFactory);
}

// static
bool PlayerComponents::Factory::OutputModeSupported(
    SbPlayerOutputMode output_mode,
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

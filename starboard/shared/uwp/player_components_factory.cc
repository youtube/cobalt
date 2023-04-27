// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include <wrl/client.h>

#include <functional>

#include "internal/starboard/xb1/shared/av1_video_decoder.h"
#include "internal/starboard/xb1/shared/video_decoder_uwp.h"
#include "internal/starboard/xb1/shared/vpx_video_decoder.h"
#include "starboard/common/device_type.h"
#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/common/system_property.h"
#include "starboard/shared/opus/opus_audio_decoder.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal_pcm.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/media_time_provider_impl.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"
#include "starboard/shared/uwp/application_uwp.h"
#include "starboard/shared/uwp/audio_renderer_passthrough.h"
#include "starboard/shared/uwp/extended_resources_manager.h"
#include "starboard/shared/win32/audio_decoder.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

using ::starboard::shared::uwp::AudioRendererPassthrough;

double GetRefreshRate() {
  return static_cast<double>(uwp::ApplicationUwp::Get()->GetRefreshRate());
}

bool IsHdrVideo(const media::VideoStreamInfo& video_stream_info) {
  const auto& mime = video_stream_info.mime;
  const auto& primaries = video_stream_info.color_metadata.primaries;
  return mime.find("codecs=\"vp9.2") != mime.npos ||
         mime.find("codecs=\"vp09.02") != mime.npos ||
         primaries == kSbMediaPrimaryIdBt2020;
}

class PlayerComponentsPassthrough
    : public ::starboard::shared::starboard::player::filter::PlayerComponents {
 public:
  PlayerComponentsPassthrough(
      scoped_ptr<AudioRendererPassthrough> audio_renderer,
      scoped_ptr<VideoRenderer> video_renderer)
      : audio_renderer_(audio_renderer.Pass()),
        video_renderer_(video_renderer.Pass()) {}

 private:
  // PlayerComponents methods
  MediaTimeProvider* GetMediaTimeProvider() override {
    return audio_renderer_.get();
  }
  AudioRenderer* GetAudioRenderer() override { return audio_renderer_.get(); }
  VideoRenderer* GetVideoRenderer() override { return video_renderer_.get(); }

  scoped_ptr<AudioRendererPassthrough> audio_renderer_;
  scoped_ptr<VideoRenderer> video_renderer_;
};

class PlayerComponentsFactory : public PlayerComponents::Factory {
  using AudioRendererPassthrough =
      ::starboard::shared::uwp::AudioRendererPassthrough;
  scoped_ptr<PlayerComponents> CreateComponents(
      const CreationParameters& creation_parameters,
      std::string* error_message) override {
    SB_DCHECK(creation_parameters.audio_codec() != kSbMediaAudioCodecNone ||
              creation_parameters.video_codec() != kSbMediaVideoCodecNone);
    SB_DCHECK(error_message);

    if (creation_parameters.audio_codec() != kSbMediaAudioCodecAc3 &&
        creation_parameters.audio_codec() != kSbMediaAudioCodecEac3) {
      SB_LOG(INFO) << "Creating non pass-through components.";
      return PlayerComponents::Factory::CreateComponents(creation_parameters,
                                                         error_message);
    }

    SB_LOG(INFO) << "Creating pass-through components.";

    scoped_ptr<AudioDecoder> audio_decoder;
    scoped_ptr<AudioRendererPassthrough> audio_renderer;
    scoped_ptr<AudioRendererSink> audio_renderer_sink;
    scoped_ptr<VideoDecoder> video_decoder;
    scoped_ptr<VideoRenderAlgorithm> video_render_algorithm;
    scoped_refptr<VideoRendererSink> video_renderer_sink;
    scoped_ptr<VideoRendererImpl> video_renderer;

    if (!CreateSubComponents(creation_parameters, &audio_decoder,
                             &audio_renderer_sink, &video_decoder,
                             &video_render_algorithm, &video_renderer_sink,
                             error_message)) {
      return scoped_ptr<PlayerComponents>();
    }
    audio_renderer =
        scoped_ptr<AudioRendererPassthrough>(new AudioRendererPassthrough(
            audio_decoder.Pass(), creation_parameters.audio_stream_info()));
    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      SB_DCHECK(video_decoder);
      SB_DCHECK(video_render_algorithm);

      MediaTimeProvider* media_time_provider = audio_renderer.get();
      video_renderer.reset(new VideoRendererImpl(
          video_decoder.Pass(), media_time_provider,
          video_render_algorithm.Pass(), video_renderer_sink));
    }

    return scoped_ptr<PlayerComponents>(new PlayerComponentsPassthrough(
        audio_renderer.Pass(), video_renderer.Pass()));
  }

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

      auto decoder_creator = [](const media::AudioStreamInfo& audio_stream_info,
                                SbDrmSystem drm_system) {
        using AacAudioDecoder = ::starboard::shared::win32::AudioDecoder;
        using OpusAudioDecoder = ::starboard::shared::opus::OpusAudioDecoder;

        if (audio_stream_info.codec == kSbMediaAudioCodecAac) {
          return scoped_ptr<AudioDecoder>(
              new AacAudioDecoder(audio_stream_info, drm_system));
        } else if (audio_stream_info.codec == kSbMediaAudioCodecOpus) {
          scoped_ptr<OpusAudioDecoder> audio_decoder_impl(
              new OpusAudioDecoder(audio_stream_info));
          if (audio_decoder_impl->is_valid()) {
            return audio_decoder_impl.PassAs<AudioDecoder>();
          }
        } else {
          SB_NOTREACHED();
        }
        return scoped_ptr<AudioDecoder>();
      };

      auto audio_codec = creation_parameters.audio_stream_info().codec;
      if (audio_codec != kSbMediaAudioCodecAc3 &&
          audio_codec != kSbMediaAudioCodecEac3) {
        audio_decoder->reset(new AdaptiveAudioDecoder(
            creation_parameters.audio_stream_info(),
            creation_parameters.drm_system(), decoder_creator));
      } else {
        // Use win32::AudioDecoder to decrypt and reformat the bitstream for the
        // passthrough AudioRenderer.
        audio_decoder->reset(new ::starboard::shared::win32::AudioDecoder(
            creation_parameters.audio_stream_info(),
            creation_parameters.drm_system()));
      }
      audio_renderer_sink->reset(new AudioRendererSinkImpl);
    }

    const auto video_codec = creation_parameters.video_codec();
    if (video_codec == kSbMediaVideoCodecNone) {
      return true;
    }

    using MftVideoDecoder = ::starboard::xb1::shared::VideoDecoderUwp;

    const auto output_mode = creation_parameters.output_mode();
    const auto is_hdr_video =
        IsHdrVideo(creation_parameters.video_stream_info());

    if (video_codec == kSbMediaVideoCodecH264 ||
        (video_codec == kSbMediaVideoCodecVp9 &&
         MftVideoDecoder::IsHardwareVp9DecoderSupported()) ||
        (video_codec == kSbMediaVideoCodecAv1 &&
         MftVideoDecoder::IsHardwareAv1DecoderSupported())) {
      video_render_algorithm->reset(
          new VideoRenderAlgorithmImpl(std::bind(GetRefreshRate)));
      video_decoder->reset(new MftVideoDecoder(
          video_codec, output_mode,
          creation_parameters.decode_target_graphics_context_provider(),
          creation_parameters.drm_system()));
      return true;
    }

#if !SB_HAS(GPU_DECODERS_ON_DESKTOP)
#if SB_API_VERSION < SB_SYSTEM_DEVICE_TYPE_AS_STRING_API_VERSION
    if (SbSystemGetDeviceType() == kSbSystemDeviceTypeDesktopPC) {
      SB_LOG(WARNING) << "GPU decoder disabled on Desktop.";
      return false;
    }
#else
    if (GetSystemPropertyString(kSbSystemPropertyDeviceType) ==
        kSystemDeviceTypeDesktopPC) {
      SB_LOG(WARNING) << "GPU decoder disabled on Desktop.";
      return false;
    }
#endif
#endif  // !SB_HAS(GPU_DECODERS_ON_DESKTOP)
    if (video_codec != kSbMediaVideoCodecVp9 &&
        video_codec != kSbMediaVideoCodecAv1) {
      return false;
    }
    SB_DCHECK(output_mode == kSbPlayerOutputModeDecodeToTexture);

    Microsoft::WRL::ComPtr<ID3D12Device> d3d12device;
    void* d3d12queue = nullptr;
    if (!uwp::ExtendedResourcesManager::GetInstance()->GetD3D12Objects(
            &d3d12device, &d3d12queue)) {
      // Somehow extended resources get lost.  Returns directly to trigger an
      // error to the player.
      *error_message =
          "Failed to obtain D3D12Device and/or D3D12queue required for "
          "instantiating GPU based decoders.";
      SB_LOG(ERROR) << *error_message;
      return false;
    }
    SB_DCHECK(d3d12device);
    SB_DCHECK(d3d12queue);

    using GpuVp9VideoDecoder = ::starboard::xb1::shared::VpxVideoDecoder;
    using GpuAv1VideoDecoder = ::starboard::xb1::shared::Av1VideoDecoder;

    if (video_codec == kSbMediaVideoCodecVp9) {
      video_decoder->reset(new GpuVp9VideoDecoder(
          creation_parameters.decode_target_graphics_context_provider(),
          creation_parameters.video_stream_info(), is_hdr_video, d3d12device,
          d3d12queue));
    }

    if (video_codec == kSbMediaVideoCodecAv1) {
      video_decoder->reset(new GpuAv1VideoDecoder(
          creation_parameters.decode_target_graphics_context_provider(),
          creation_parameters.video_stream_info(), is_hdr_video, d3d12device,
          d3d12queue));
    }

    if (video_decoder) {
      video_render_algorithm->reset(
          new VideoRenderAlgorithmImpl(std::bind(GetRefreshRate)));
      return true;
    }

    *error_message = FormatString(
        "Unsupported video codec %d or insufficient resources to its creation.",
        video_codec);
    SB_LOG(ERROR) << *error_message;
    return false;
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
  return output_mode == kSbPlayerOutputModeDecodeToTexture;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

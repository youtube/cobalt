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

#include "starboard/shared/starboard/player/filter/player_components.h"

#include "starboard/android/shared/audio_decoder.h"
#include "starboard/android/shared/video_decoder.h"
#include "starboard/android/shared/video_render_algorithm.h"
#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/shared/opus/opus_audio_decoder.h"
#include "starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
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

class PlayerComponentsImpl : public PlayerComponents {
  bool CreateAudioComponents(const AudioParameters& audio_parameters,
                             scoped_ptr<AudioDecoder>* audio_decoder,
                             scoped_ptr<AudioRendererSink>* audio_renderer_sink,
                             std::string* error_message) override {
    SB_DCHECK(audio_decoder);
    SB_DCHECK(audio_renderer_sink);
    SB_DCHECK(error_message);

    auto decoder_creator = [](const SbMediaAudioSampleInfo& audio_sample_info,
                              SbDrmSystem drm_system) {
      using AacAudioDecoder = ::starboard::android::shared::AudioDecoder;
      using OpusAudioDecoder = ::starboard::shared::opus::OpusAudioDecoder;

      if (audio_sample_info.codec == kSbMediaAudioCodecAac) {
        scoped_ptr<AacAudioDecoder> audio_decoder_impl(new AacAudioDecoder(
            audio_sample_info.codec, audio_sample_info, drm_system));
        if (audio_decoder_impl->is_valid()) {
          return audio_decoder_impl.PassAs<AudioDecoder>();
        }
      } else if (audio_sample_info.codec == kSbMediaAudioCodecOpus) {
        scoped_ptr<OpusAudioDecoder> audio_decoder_impl(
            new OpusAudioDecoder(audio_sample_info));
        if (audio_decoder_impl->is_valid()) {
          return audio_decoder_impl.PassAs<AudioDecoder>();
        }
      } else {
        SB_NOTREACHED();
      }
      return scoped_ptr<AudioDecoder>();
    };

    audio_decoder->reset(
        new AdaptiveAudioDecoder(audio_parameters.audio_sample_info,
                                 audio_parameters.drm_system, decoder_creator));
    audio_renderer_sink->reset(new AudioRendererSinkImpl);
    return true;
  }

  bool CreateVideoComponents(
      const VideoParameters& video_parameters,
      scoped_ptr<VideoDecoder>* video_decoder,
      scoped_ptr<VideoRenderAlgorithm>* video_render_algorithm,
      scoped_refptr<VideoRendererSink>* video_renderer_sink,
      std::string* error_message) override {
    using VideoDecoderImpl = ::starboard::android::shared::VideoDecoder;
    using VideoRenderAlgorithmImpl =
        ::starboard::android::shared::VideoRenderAlgorithm;

    SB_DCHECK(video_decoder);
    SB_DCHECK(video_render_algorithm);
    SB_DCHECK(video_renderer_sink);
    SB_DCHECK(error_message);

    scoped_ptr<VideoDecoderImpl> video_decoder_impl(new VideoDecoderImpl(
        video_parameters.video_codec, video_parameters.drm_system,
        video_parameters.output_mode,
        video_parameters.decode_target_graphics_context_provider));
    if (video_decoder_impl->is_valid()) {
      *video_renderer_sink = video_decoder_impl->GetSink();
      video_decoder->reset(video_decoder_impl.release());
    } else {
      video_decoder->reset();
      *video_renderer_sink = NULL;
      *error_message = "Failed to create video decoder.";
      return false;
    }

    video_render_algorithm->reset(new VideoRenderAlgorithmImpl);
    return true;
  }

  void GetAudioRendererParams(int* max_cached_frames,
                              int* max_frames_per_append) const override {
    SB_DCHECK(max_cached_frames);
    SB_DCHECK(max_frames_per_append);

    *max_cached_frames = 128 * 1024;
    *max_frames_per_append = 16384;
  }
};

}  // namespace

// static
scoped_ptr<PlayerComponents> PlayerComponents::Create() {
  return make_scoped_ptr<PlayerComponents>(new PlayerComponentsImpl);
}

// static
bool VideoDecoder::OutputModeSupported(SbPlayerOutputMode output_mode,
                                       SbMediaVideoCodec codec,
                                       SbDrmSystem drm_system) {
  if (output_mode == kSbPlayerOutputModePunchOut) {
    return true;
  }

  if (output_mode == kSbPlayerOutputModeDecodeToTexture) {
    return !SbDrmSystemIsValid(drm_system);
  }

  return false;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

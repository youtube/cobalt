// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#import <UIKit/UIKit.h>

#include "starboard/atomic.h"
#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/string.h"
#include "starboard/media.h"
#include "starboard/shared/opus/opus_audio_decoder.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"

#import "starboard/shared/uikit/audio_decoder.h"
#import "starboard/shared/uikit/av_sample_buffer_audio_renderer.h"
#import "starboard/shared/uikit/av_sample_buffer_synchronizer.h"
#import "starboard/shared/uikit/av_sample_buffer_video_renderer.h"
#import "starboard/shared/uikit/playback_capabilities.h"
#import "starboard/shared/uikit/video_decoder.h"
#if SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64
#import "starboard/shared/uikit/vpx_video_decoder.h"  // nogncheck
#endif  // SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace {

using uikit::AVSBAudioRenderer;
using uikit::AVSBSynchronizer;
using uikit::AVSBVideoRenderer;
using uikit::PlaybackCapabilities;

class SurroundAwarePlayerComponents : public PlayerComponents {
 public:
  SurroundAwarePlayerComponents(bool is_5_1_playback,
                                std::unique_ptr<PlayerComponents> components)
      : is_5_1_playback_(is_5_1_playback), components_{components.release()} {
    SB_DCHECK(components_);

    if (!is_5_1_playback_) {
      return;
    }
    if (SbAtomicNoBarrier_Increment(&s_active_5_1_playback, 1) == 1) {
      SetPreferredOutputNumberOfChannels(6);
    }
  }

 private:
  ~SurroundAwarePlayerComponents() override {
    components_.reset();
    if (!is_5_1_playback_) {
      return;
    }
    if (SbAtomicNoBarrier_Increment(&s_active_5_1_playback, -1) == 0) {
      SetPreferredOutputNumberOfChannels(2);
    }
  }

  MediaTimeProvider* GetMediaTimeProvider() override {
    return components_->GetMediaTimeProvider();
  }
  AudioRenderer* GetAudioRenderer() override {
    return components_->GetAudioRenderer();
  }
  VideoRenderer* GetVideoRenderer() override {
    return components_->GetVideoRenderer();
  }

  void SetPreferredOutputNumberOfChannels(int channels) {
    @autoreleasepool {
      SB_DCHECK(channels == 1 || channels == 2 || channels == 6);
      channels = channels == 6 ? 6 : 2;

      NSError* error = nil;
      BOOL result = [[AVAudioSession sharedInstance]
          setPreferredOutputNumberOfChannels:channels
                                       error:&error];
      if (result && !error) {
        SB_LOG(INFO) << "Set preferred output number of channels to "
                     << channels;
      } else if (!result || error) {
        SB_LOG(WARNING)
            << "Failed to set preferred output number of channels to "
            << channels;
      }
    }
  }

  static SbAtomic32 s_active_5_1_playback;

  const bool is_5_1_playback_;
  std::unique_ptr<PlayerComponents> components_;
};

SbAtomic32 SurroundAwarePlayerComponents::s_active_5_1_playback = 0;

class AVSampleBufferPlayerComponentsImpl : public PlayerComponents {
 public:
  AVSampleBufferPlayerComponentsImpl(
      std::unique_ptr<AVSBSynchronizer> synchronizer,
      std::unique_ptr<AVSBAudioRenderer> audio_renderer,
      std::unique_ptr<AVSBVideoRenderer> video_renderer)
      : synchronizer_{synchronizer.release()},
        audio_renderer_{audio_renderer.release()},
        video_renderer_{video_renderer.release()} {
    SB_DCHECK(synchronizer_);
    SB_DCHECK(audio_renderer_ || video_renderer_);
  }
  ~AVSampleBufferPlayerComponentsImpl() override {
    synchronizer_->Pause();
    synchronizer_->ResetRenderers();
  }

  MediaTimeProvider* GetMediaTimeProvider() override {
    return synchronizer_.get();
  }
  AudioRenderer* GetAudioRenderer() override { return audio_renderer_.get(); }
  VideoRenderer* GetVideoRenderer() override { return video_renderer_.get(); }

 private:
  std::unique_ptr<AVSBSynchronizer> synchronizer_;
  std::unique_ptr<AVSBAudioRenderer> audio_renderer_;
  std::unique_ptr<AVSBVideoRenderer> video_renderer_;
};

class PlayerComponentsFactory : public PlayerComponents::Factory {
  std::unique_ptr<PlayerComponents> CreateComponents(
      const CreationParameters& creation_parameters,
      std::string* error_message) override {
    std::unique_ptr<PlayerComponents> components;
    if (creation_parameters.output_mode() == kSbPlayerOutputModePunchOut) {
      components = CreatePunchoutComponents(creation_parameters, error_message);
    } else {
      components = PlayerComponents::Factory::CreateComponents(
          creation_parameters, error_message);
    }
    bool is_5_1_playback =
        creation_parameters.audio_codec() != kSbMediaAudioCodecNone &&
        creation_parameters.audio_stream_info().number_of_channels == 6;
    return std::unique_ptr<PlayerComponents>(
        new SurroundAwarePlayerComponents(is_5_1_playback, std::move(components)));
  }

  bool CreateSubComponents(
      const CreationParameters& creation_parameters,
      std::unique_ptr<AudioDecoder>* audio_decoder,
      std::unique_ptr<AudioRendererSink>* audio_renderer_sink,
      std::unique_ptr<VideoDecoder>* video_decoder,
      std::unique_ptr<VideoRenderAlgorithm>* video_render_algorithm,
      scoped_refptr<VideoRendererSink>* video_renderer_sink,
      std::string* error_message) override {
    SB_DCHECK(error_message);

    if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
      SB_DCHECK(audio_decoder);
      SB_DCHECK(audio_renderer_sink);

      auto decoder_creator = [](const media::AudioStreamInfo& audio_stream_info,
                                SbDrmSystem drm_system) {
        using ::starboard::shared::opus::OpusAudioDecoder;

        if (audio_stream_info.codec == kSbMediaAudioCodecAac ||
            audio_stream_info.codec == kSbMediaAudioCodecAc3 ||
            audio_stream_info.codec == kSbMediaAudioCodecEac3) {
          return std::unique_ptr<AudioDecoder>(
              new uikit::TvosAudioDecoder(audio_stream_info));
        } else if (audio_stream_info.codec == kSbMediaAudioCodecOpus) {
          return std::unique_ptr<AudioDecoder>(
              new OpusAudioDecoder(audio_stream_info));
        } else {
          SB_NOTREACHED();
        }
        return std::unique_ptr<AudioDecoder>();
      };

      audio_decoder->reset(new AdaptiveAudioDecoder(
          creation_parameters.audio_stream_info(),
          creation_parameters.drm_system(), decoder_creator));
      audio_renderer_sink->reset(new AudioRendererSinkImpl);
    }

    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      SB_DCHECK(video_decoder);
      SB_DCHECK(video_render_algorithm);
      SB_DCHECK(video_renderer_sink);

      video_decoder->reset();

      if (creation_parameters.video_codec() == kSbMediaVideoCodecH264) {
        video_decoder->reset(new uikit::TvosVideoDecoder(
            creation_parameters.output_mode(),
            creation_parameters.decode_target_graphics_context_provider()));
      }
#if SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64
      else if (creation_parameters.video_codec() == kSbMediaVideoCodecVp9) {
        video_decoder->reset(new uikit::VpxVideoDecoder(
            creation_parameters.output_mode(),
            creation_parameters.decode_target_graphics_context_provider()));
      }
#endif  // SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64
      else {
        SB_LOG(ERROR) << "Unsupported video codec "
                      << creation_parameters.video_codec();
        *error_message = FormatString("Unsupported video codec %d",
                                      creation_parameters.video_codec());
        SB_NOTREACHED();
        return false;
      }

      video_render_algorithm->reset(new VideoRenderAlgorithmImpl([]() {
        @autoreleasepool {
          return UIScreen.mainScreen.maximumFramesPerSecond;
        }
      }));
      *video_renderer_sink = NULL;
    }

    return true;
  }

  std::unique_ptr<PlayerComponents> CreatePunchoutComponents(
      const CreationParameters& creation_parameters,
      std::string* error_message) {
    SB_DCHECK(creation_parameters.output_mode() == kSbPlayerOutputModePunchOut);

    std::unique_ptr<AVSBSynchronizer> synchronizer(new AVSBSynchronizer());
    std::unique_ptr<AVSBAudioRenderer> audio_renderer;
    std::unique_ptr<AVSBVideoRenderer> video_renderer;

    if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
      audio_renderer.reset(
          new AVSBAudioRenderer(creation_parameters.audio_stream_info(),
                                creation_parameters.drm_system()));
      synchronizer->SetRenderer(audio_renderer.get());
    }

    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      video_renderer.reset(
          new AVSBVideoRenderer(creation_parameters.video_stream_info(),
                                creation_parameters.drm_system()));
      synchronizer->SetRenderer(video_renderer.get());
    }

    return std::unique_ptr<PlayerComponents>(new AVSampleBufferPlayerComponentsImpl(
        std::move(synchronizer), std::move(audio_renderer), std::move(video_renderer)));
  }
};

}  // namespace

// static
std::unique_ptr<PlayerComponents::Factory> PlayerComponents::Factory::Create() {
  return std::unique_ptr<PlayerComponents::Factory>(
      new PlayerComponentsFactory);
}

bool PlayerComponents::Factory::OutputModeSupported(
    SbPlayerOutputMode output_mode,
    SbMediaVideoCodec codec,
    SbDrmSystem drm_system) {
  SB_DCHECK(output_mode == kSbPlayerOutputModeDecodeToTexture ||
            output_mode == kSbPlayerOutputModePunchOut);

  if (codec == kSbMediaVideoCodecNone || codec == kSbMediaVideoCodecH264) {
    return true;
  }

  if (codec == kSbMediaVideoCodecVp9) {
#if SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64
    return true;
#else   // SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64
    return PlaybackCapabilities::IsHwVp9Supported();
#endif  // SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64
  }
  return false;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

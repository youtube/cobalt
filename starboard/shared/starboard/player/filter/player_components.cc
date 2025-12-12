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

#include <memory>
#include <utility>

#include "starboard/common/check_op.h"
#include "starboard/common/command_line.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/features.h"
#include "starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal_pcm.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/media_time_provider_impl.h"
#include "starboard/shared/starboard/player/filter/punchout_video_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/stub_audio_decoder.h"
#include "starboard/shared/starboard/player/filter/stub_video_decoder.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal_impl.h"

namespace starboard {

namespace {

const int kAudioSinkFramesAlignment = 256;
const int kDefaultAudioSinkMinFramesPerAppend = 1024;

typedef MediaTimeProviderImpl::MonotonicSystemTimeProvider
    MonotonicSystemTimeProvider;

class MonotonicSystemTimeProviderImpl : public MonotonicSystemTimeProvider {
  int64_t GetMonotonicNow() const override { return CurrentMonotonicTime(); }
};

class PlayerComponentsImpl : public PlayerComponents {
 public:
  PlayerComponentsImpl(
      std::unique_ptr<MediaTimeProviderImpl> media_time_provider,
      std::unique_ptr<AudioRendererPcm> audio_renderer,
      std::unique_ptr<VideoRendererImpl> video_renderer)
      : media_time_provider_(std::move(media_time_provider)),
        audio_renderer_(std::move(audio_renderer)),
        video_renderer_(std::move(video_renderer)) {
    SB_DCHECK(media_time_provider_ || audio_renderer_);
    SB_DCHECK(audio_renderer_ || video_renderer_);
  }

  MediaTimeProvider* GetMediaTimeProvider() override {
    return audio_renderer_
               ? static_cast<MediaTimeProvider*>(audio_renderer_.get())
               : media_time_provider_.get();
  }
  AudioRenderer* GetAudioRenderer() override { return audio_renderer_.get(); }
  VideoRenderer* GetVideoRenderer() override { return video_renderer_.get(); }

 private:
  // |media_time_provider_| will only be used when |audio_renderer_| is nullptr.
  std::unique_ptr<MediaTimeProviderImpl> media_time_provider_;
  std::unique_ptr<AudioRendererPcm> audio_renderer_;
  std::unique_ptr<VideoRendererImpl> video_renderer_;
};

int AlignUp(int value, int alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

}  // namespace

PlayerComponents::Factory::CreationParameters::CreationParameters(
    const AudioStreamInfo& audio_stream_info,
    SbDrmSystem drm_system)
    : audio_stream_info_(audio_stream_info), drm_system_(drm_system) {
  SB_DCHECK_NE(audio_stream_info_.codec, kSbMediaAudioCodecNone);
}

PlayerComponents::Factory::CreationParameters::CreationParameters(
    const VideoStreamInfo& video_stream_info,
    SbPlayer player,
    SbPlayerOutputMode output_mode,
    int max_video_input_size,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider,
    SbDrmSystem drm_system)
    : video_stream_info_(video_stream_info),
      player_(player),
      output_mode_(output_mode),
      max_video_input_size_(max_video_input_size),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      drm_system_(drm_system) {
  SB_DCHECK_NE(video_stream_info_.codec, kSbMediaVideoCodecNone);
  SB_DCHECK(SbPlayerIsValid(player_));
  SB_DCHECK_NE(output_mode_, kSbPlayerOutputModeInvalid);
}

PlayerComponents::Factory::CreationParameters::CreationParameters(
    const AudioStreamInfo& audio_stream_info,
    const VideoStreamInfo& video_stream_info,
    SbPlayer player,
    SbPlayerOutputMode output_mode,
    int max_video_input_size,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider,
    SbDrmSystem drm_system)
    : audio_stream_info_(audio_stream_info),
      video_stream_info_(video_stream_info),
      player_(player),
      output_mode_(output_mode),
      max_video_input_size_(max_video_input_size),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      drm_system_(drm_system) {
  SB_DCHECK(audio_stream_info_.codec != kSbMediaAudioCodecNone ||
            video_stream_info_.codec != kSbMediaVideoCodecNone);
}

NonNullResult<std::unique_ptr<PlayerComponents>>
PlayerComponents::Factory::CreateComponents(
    const CreationParameters& creation_parameters) {
  SB_DCHECK(creation_parameters.audio_codec() != kSbMediaAudioCodecNone ||
            creation_parameters.video_codec() != kSbMediaVideoCodecNone);

  bool use_stub_audio_decoder = false;
  bool use_stub_video_decoder = false;
#if BUILDFLAG(IS_ANDROID)
  use_stub_audio_decoder =
      features::FeatureList::IsEnabled(features::kUseStubAudioDecoder);
  use_stub_video_decoder =
      features::FeatureList::IsEnabled(features::kUseStubVideoDecoder);
#else
  auto command_line = Application::Get()->GetCommandLine();
  use_stub_audio_decoder = command_line->HasSwitch("use_stub_audio_decoder");
  use_stub_video_decoder = command_line->HasSwitch("use_stub_video_decoder");
#endif  // BUILDFLAG(IS_ANDROID)

  MediaComponents components;
  if (use_stub_audio_decoder && use_stub_video_decoder) {
    components.audio = CreateStubAudioComponents(creation_parameters);
    components.video = CreateStubVideoComponents(creation_parameters);
  } else {
    auto copy_of_creation_parameters = creation_parameters;
    if (use_stub_audio_decoder) {
      copy_of_creation_parameters.reset_audio_codec();
    } else if (use_stub_video_decoder) {
      copy_of_creation_parameters.reset_video_codec();
    }
    auto sub_components = CreateSubComponents(copy_of_creation_parameters);
    if (!sub_components) {
      return Failure(sub_components.error());
    }
    components = std::move(*sub_components);
    if (use_stub_audio_decoder) {
      SB_DCHECK(!components.audio.decoder);
      SB_DCHECK(!components.audio.renderer_sink);
      components.audio = CreateStubAudioComponents(creation_parameters);
    } else if (use_stub_video_decoder) {
      SB_DCHECK(!components.video.decoder);
      SB_DCHECK(!components.video.render_algorithm);
      SB_DCHECK(!components.video.renderer_sink);
      components.video = CreateStubVideoComponents(creation_parameters);
    }
  }

  std::unique_ptr<MediaTimeProviderImpl> media_time_provider_impl;
  std::unique_ptr<AudioRendererPcm> audio_renderer;
  std::unique_ptr<VideoRendererImpl> video_renderer;

  if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
    SB_DCHECK(components.audio.decoder);
    SB_DCHECK(components.audio.renderer_sink);

    int max_cached_frames, min_frames_per_append;
    GetAudioRendererParams(creation_parameters, &max_cached_frames,
                           &min_frames_per_append);

    audio_renderer = std::make_unique<AudioRendererPcm>(
        std::move(components.audio.decoder),
        std::move(components.audio.renderer_sink),
        creation_parameters.audio_stream_info(), max_cached_frames,
        min_frames_per_append);
  }

  if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
    SB_DCHECK(components.video.decoder);
    SB_DCHECK(components.video.render_algorithm);

    MediaTimeProvider* media_time_provider = nullptr;
    if (audio_renderer) {
      media_time_provider = audio_renderer.get();
    } else {
      media_time_provider_impl = std::make_unique<MediaTimeProviderImpl>(
          std::make_unique<MonotonicSystemTimeProviderImpl>());
      media_time_provider = media_time_provider_impl.get();
    }
    video_renderer = std::make_unique<VideoRendererImpl>(
        std::move(components.video.decoder), media_time_provider,
        std::move(components.video.render_algorithm),
        std::move(components.video.renderer_sink));
  }

  SB_DCHECK(audio_renderer || video_renderer);
  return std::make_unique<PlayerComponentsImpl>(
      std::move(media_time_provider_impl), std::move(audio_renderer),
      std::move(video_renderer));
}

PlayerComponents::Factory::AudioComponents
PlayerComponents::Factory::CreateStubAudioComponents(
    const CreationParameters& creation_parameters) {
  PlayerComponents::Factory::AudioComponents components;

  auto decoder_creator = [](const AudioStreamInfo& audio_stream_info,
                            SbDrmSystem drm_system) {
    return std::make_unique<StubAudioDecoder>(audio_stream_info);
  };
  components.decoder = std::make_unique<AdaptiveAudioDecoder>(
      creation_parameters.audio_stream_info(), creation_parameters.drm_system(),
      decoder_creator);
  components.renderer_sink = std::make_unique<AudioRendererSinkImpl>();
  return components;
}

PlayerComponents::Factory::VideoComponents
PlayerComponents::Factory::CreateStubVideoComponents(
    const CreationParameters& creation_parameters) {
  const int64_t kVideoSinkRenderIntervalUsec = 10'000;  // 10ms

  PlayerComponents::Factory::VideoComponents components;
  components.decoder = std::make_unique<StubVideoDecoder>();
  components.render_algorithm = std::make_unique<VideoRenderAlgorithmImpl>();
  components.renderer_sink = make_scoped_refptr<PunchoutVideoRendererSink>(
      creation_parameters.player(), kVideoSinkRenderIntervalUsec);
  return components;
}

void PlayerComponents::Factory::GetAudioRendererParams(
    const CreationParameters& creation_parameters,
    int* max_cached_frames,
    int* min_frames_per_append) const {
  SB_CHECK(max_cached_frames);
  SB_CHECK(min_frames_per_append);
  SB_DCHECK(kDefaultAudioSinkMinFramesPerAppend % kAudioSinkFramesAlignment ==
            0);
  *min_frames_per_append = kDefaultAudioSinkMinFramesPerAppend;
  // AudioRenderer prefers to use kSbMediaAudioSampleTypeFloat32 and only uses
  // kSbMediaAudioSampleTypeInt16Deprecated when float32 is not supported.
  int min_frames_required = SbAudioSinkGetMinBufferSizeInFrames(
      creation_parameters.audio_stream_info().number_of_channels,
      SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)
          ? kSbMediaAudioSampleTypeFloat32
          : kSbMediaAudioSampleTypeInt16Deprecated,
      creation_parameters.audio_stream_info().samples_per_second);
  // Audio renderer would sleep for a while if it thinks there're enough
  // frames in the sink. The sleeping time is 1/4 of |max_cached_frames|. So, to
  // maintain required min buffer size of audio sink, the |max_cached_frames|
  // need to be larger than |min_frames_required| * 4/3.
  *max_cached_frames = static_cast<int>(min_frames_required * 1.4) +
                       kDefaultAudioSinkMinFramesPerAppend;
  *max_cached_frames = AlignUp(*max_cached_frames, kAudioSinkFramesAlignment);
}

}  // namespace starboard

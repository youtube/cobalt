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

#include <utility>

#include "starboard/common/time.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/command_line.h"
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
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

const int kAudioSinkFramesAlignment = 256;
const int kDefaultAudioSinkMinFramesPerAppend = 1024;
const int kDefaultAudioSinkMaxCachedFrames =
    8 * kDefaultAudioSinkMinFramesPerAppend;

typedef MediaTimeProviderImpl::MonotonicSystemTimeProvider
    MonotonicSystemTimeProvider;

class MonotonicSystemTimeProviderImpl : public MonotonicSystemTimeProvider {
  int64_t GetMonotonicNow() const override { return CurrentMonotonicTime(); }
};

class PlayerComponentsImpl : public PlayerComponents {
 public:
  PlayerComponentsImpl(
      std::unique_ptr<MediaTimeProviderImpl> media_time_provider,
      unique_ptr_alias<AudioRendererPcm> audio_renderer,
      unique_ptr_alias<VideoRendererImpl> video_renderer)
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
  unique_ptr_alias<AudioRendererPcm> audio_renderer_;
  unique_ptr_alias<VideoRendererImpl> video_renderer_;
};

int AlignUp(int value, int alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

}  // namespace

PlayerComponents::Factory::CreationParameters::CreationParameters(
    const media::AudioStreamInfo& audio_stream_info,
    SbDrmSystem drm_system)
    : audio_stream_info_(audio_stream_info), drm_system_(drm_system) {
  SB_DCHECK(audio_stream_info_.codec != kSbMediaAudioCodecNone);
}

PlayerComponents::Factory::CreationParameters::CreationParameters(
    const media::VideoStreamInfo& video_stream_info,
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
  SB_DCHECK(video_stream_info_.codec != kSbMediaVideoCodecNone);
  SB_DCHECK(SbPlayerIsValid(player_));
  SB_DCHECK(output_mode_ != kSbPlayerOutputModeInvalid);
}

PlayerComponents::Factory::CreationParameters::CreationParameters(
    const media::AudioStreamInfo& audio_stream_info,
    const media::VideoStreamInfo& video_stream_info,
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

PlayerComponents::Factory::CreationParameters::CreationParameters(
    const CreationParameters& that) {
  this->audio_stream_info_ = that.audio_stream_info_;
  this->video_stream_info_ = that.video_stream_info_;
  this->player_ = that.player_;
  this->output_mode_ = that.output_mode_;
  this->max_video_input_size_ = that.max_video_input_size_;
  this->decode_target_graphics_context_provider_ =
      that.decode_target_graphics_context_provider_;
  this->drm_system_ = that.drm_system_;
}

unique_ptr_alias<PlayerComponents> PlayerComponents::Factory::CreateComponents(
    const CreationParameters& creation_parameters,
    std::string* error_message) {
  SB_DCHECK(creation_parameters.audio_codec() != kSbMediaAudioCodecNone ||
            creation_parameters.video_codec() != kSbMediaVideoCodecNone);
  SB_DCHECK(error_message);

  unique_ptr_alias<AudioDecoder> audio_decoder;
  unique_ptr_alias<AudioRendererSink> audio_renderer_sink;
  unique_ptr_alias<VideoDecoder> video_decoder;
  unique_ptr_alias<VideoRenderAlgorithm> video_render_algorithm;
  scoped_refptr<VideoRendererSink> video_renderer_sink;

  auto command_line = shared::starboard::Application::Get()->GetCommandLine();
  bool use_stub_audio_decoder =
      command_line->HasSwitch("use_stub_audio_decoder");
  bool use_stub_video_decoder =
      command_line->HasSwitch("use_stub_video_decoder");

  if (use_stub_audio_decoder && use_stub_video_decoder) {
    CreateStubAudioComponents(creation_parameters, &audio_decoder,
                              &audio_renderer_sink);
    CreateStubVideoComponents(creation_parameters, &video_decoder,
                              &video_render_algorithm, &video_renderer_sink);
  } else {
    auto copy_of_creation_parameters = creation_parameters;
    if (use_stub_audio_decoder) {
      copy_of_creation_parameters.reset_audio_codec();
    } else if (use_stub_video_decoder) {
      copy_of_creation_parameters.reset_video_codec();
    }
    if (!CreateSubComponents(copy_of_creation_parameters, &audio_decoder,
                             &audio_renderer_sink, &video_decoder,
                             &video_render_algorithm, &video_renderer_sink,
                             error_message)) {
      return unique_ptr_alias<PlayerComponents>();
    }
    if (use_stub_audio_decoder) {
      SB_DCHECK(!audio_decoder);
      SB_DCHECK(!audio_renderer_sink);
      CreateStubAudioComponents(creation_parameters, &audio_decoder,
                                &audio_renderer_sink);
    } else if (use_stub_video_decoder) {
      SB_DCHECK(!video_decoder);
      SB_DCHECK(!video_render_algorithm);
      SB_DCHECK(!video_renderer_sink);
      CreateStubVideoComponents(creation_parameters, &video_decoder,
                                &video_render_algorithm, &video_renderer_sink);
    }
  }

  std::unique_ptr<MediaTimeProviderImpl> media_time_provider_impl;
  unique_ptr_alias<AudioRendererPcm> audio_renderer;
  unique_ptr_alias<VideoRendererImpl> video_renderer;

  if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
    SB_DCHECK(audio_decoder);
    SB_DCHECK(audio_renderer_sink);

    int max_cached_frames, min_frames_per_append;
    GetAudioRendererParams(creation_parameters, &max_cached_frames,
                           &min_frames_per_append);

    audio_renderer.reset(new AudioRendererPcm(
        std::move(audio_decoder), std::move(audio_renderer_sink),
        creation_parameters.audio_stream_info(), max_cached_frames,
        min_frames_per_append));
  }

  if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
    SB_DCHECK(video_decoder);
    SB_DCHECK(video_render_algorithm);

    MediaTimeProvider* media_time_provider = nullptr;
    if (audio_renderer) {
      media_time_provider = audio_renderer.get();
    } else {
      media_time_provider_impl.reset(new MediaTimeProviderImpl(
          std::unique_ptr<MonotonicSystemTimeProvider>(
              new MonotonicSystemTimeProviderImpl)));
      media_time_provider = media_time_provider_impl.get();
    }
    video_renderer.reset(new VideoRendererImpl(
        std::move(video_decoder), media_time_provider,
        std::move(video_render_algorithm), video_renderer_sink));
  }

  SB_DCHECK(audio_renderer || video_renderer);
  return unique_ptr_alias<PlayerComponents>(new PlayerComponentsImpl(
      std::move(media_time_provider_impl), std::move(audio_renderer),
      std::move(video_renderer)));
}

void PlayerComponents::Factory::CreateStubAudioComponents(
    const CreationParameters& creation_parameters,
    unique_ptr_alias<AudioDecoder>* audio_decoder,
    unique_ptr_alias<AudioRendererSink>* audio_renderer_sink) {
  SB_DCHECK(audio_decoder);
  SB_DCHECK(audio_renderer_sink);

  auto decoder_creator = [](const media::AudioStreamInfo& audio_stream_info,
                            SbDrmSystem drm_system) {
    return unique_ptr_alias<AudioDecoder>(
        new StubAudioDecoder(audio_stream_info));
  };
  audio_decoder->reset(new AdaptiveAudioDecoder(
      creation_parameters.audio_stream_info(), creation_parameters.drm_system(),
      decoder_creator));
  audio_renderer_sink->reset(new AudioRendererSinkImpl);
}

void PlayerComponents::Factory::CreateStubVideoComponents(
    const CreationParameters& creation_parameters,
    unique_ptr_alias<VideoDecoder>* video_decoder,
    unique_ptr_alias<VideoRenderAlgorithm>* video_render_algorithm,
    scoped_refptr<VideoRendererSink>* video_renderer_sink) {
  const int64_t kVideoSinkRenderIntervalUsec = 10'000;  // 10ms

  SB_DCHECK(video_decoder);
  SB_DCHECK(video_render_algorithm);
  SB_DCHECK(video_renderer_sink);

  video_decoder->reset(new StubVideoDecoder);
  video_render_algorithm->reset(new VideoRenderAlgorithmImpl);
  *video_renderer_sink = new PunchoutVideoRendererSink(
      creation_parameters.player(), kVideoSinkRenderIntervalUsec);
}

void PlayerComponents::Factory::GetAudioRendererParams(
    const CreationParameters& creation_parameters,
    int* max_cached_frames,
    int* min_frames_per_append) const {
  SB_DCHECK(max_cached_frames);
  SB_DCHECK(min_frames_per_append);
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

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

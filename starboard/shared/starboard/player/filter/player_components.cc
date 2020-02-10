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

#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/command_line.h"
#include "starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/punchout_video_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/stub_audio_decoder.h"
#include "starboard/shared/starboard/player/filter/stub_video_decoder.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm_impl.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

PlayerComponents::CreationParameters::CreationParameters(
    SbMediaAudioCodec audio_codec,
    const std::string& audio_mime,
    const SbMediaAudioSampleInfo& audio_sample_info,
    SbDrmSystem drm_system)
    : audio_codec_(audio_codec),
      audio_mime_(audio_mime),
      audio_sample_info_(audio_sample_info),
      drm_system_(drm_system) {
  SB_DCHECK(audio_codec_ != kSbMediaAudioCodecNone);
  TryToCopyAudioSpecificConfig();
}

PlayerComponents::CreationParameters::CreationParameters(
    SbMediaVideoCodec video_codec,
    const std::string& video_mime,
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
    const SbMediaVideoSampleInfo& video_sample_info,
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
    const std::string& max_video_capabilities,
    SbPlayer player,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider,
    MediaTimeProvider* media_time_provider,
    SbDrmSystem drm_system)
    : video_codec_(video_codec),
      video_mime_(video_mime),
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
      video_sample_info_(video_sample_info),
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
      max_video_capabilities_(max_video_capabilities),
      player_(player),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      media_time_provider_(media_time_provider),
      drm_system_(drm_system) {
  SB_DCHECK(video_codec_ != kSbMediaVideoCodecNone);
  SB_DCHECK(SbPlayerIsValid(player_));
  SB_DCHECK(output_mode_ != kSbPlayerOutputModeInvalid);
}

PlayerComponents::CreationParameters::CreationParameters(
    SbMediaAudioCodec audio_codec,
    const std::string& audio_mime,
    const SbMediaAudioSampleInfo& audio_sample_info,
    SbMediaVideoCodec video_codec,
    const std::string& video_mime,
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
    const SbMediaVideoSampleInfo& video_sample_info,
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
    const std::string& max_video_capabilities,
    SbPlayer player,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider,
    MediaTimeProvider* media_time_provider,
    SbDrmSystem drm_system)
    : audio_codec_(audio_codec),
      audio_mime_(audio_mime),
      audio_sample_info_(audio_sample_info),
      video_codec_(video_codec),
      video_mime_(video_mime),
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
      video_sample_info_(video_sample_info),
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
      max_video_capabilities_(max_video_capabilities),
      player_(player),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider),
      media_time_provider_(media_time_provider),
      drm_system_(drm_system) {
  SB_DCHECK(audio_codec_ != kSbMediaAudioCodecNone ||
            video_codec_ != kSbMediaVideoCodecNone);
  TryToCopyAudioSpecificConfig();
}

PlayerComponents::CreationParameters::CreationParameters(
    const CreationParameters& that) {
  this->audio_codec_ = that.audio_codec_;
  this->audio_mime_ = that.audio_mime_;
  this->audio_sample_info_ = that.audio_sample_info_;
  this->video_codec_ = that.video_codec_;
  this->video_mime_ = that.video_mime_;
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  this->video_sample_info_ = that.video_sample_info_;
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  this->max_video_capabilities_ = that.max_video_capabilities_;
  this->player_ = that.player_;
  this->output_mode_ = that.output_mode_;
  this->decode_target_graphics_context_provider_ =
      that.decode_target_graphics_context_provider_;
  this->media_time_provider_ = that.media_time_provider_;
  this->drm_system_ = that.drm_system_;

  TryToCopyAudioSpecificConfig();
}

void PlayerComponents::CreationParameters::TryToCopyAudioSpecificConfig() {
  if (audio_sample_info_.audio_specific_config_size > 0) {
    auto audio_specific_config = reinterpret_cast<const uint8_t*>(
        audio_sample_info_.audio_specific_config);
    audio_specific_config_.assign(
        audio_specific_config,
        audio_specific_config + audio_sample_info_.audio_specific_config_size);
    audio_sample_info_.audio_specific_config = audio_specific_config_.data();
  }
}

bool PlayerComponents::CreateRenderers(
    const CreationParameters& creation_parameters,
    scoped_ptr<AudioRenderer>* audio_renderer,
    scoped_ptr<VideoRenderer>* video_renderer,
    std::string* error_message) {
  SB_DCHECK(creation_parameters.audio_codec() != kSbMediaAudioCodecNone ||
            creation_parameters.video_codec() != kSbMediaVideoCodecNone);
  SB_DCHECK(error_message);

  if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
    SB_DCHECK(audio_renderer);
    audio_renderer->reset();
  }
  if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
    SB_DCHECK(video_renderer);
    video_renderer->reset();
  }

  scoped_ptr<AudioDecoder> audio_decoder;
  scoped_ptr<AudioRendererSink> audio_renderer_sink;
  scoped_ptr<VideoDecoder> video_decoder;
  scoped_ptr<VideoRenderAlgorithm> video_render_algorithm;
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
    if (!CreateComponents(copy_of_creation_parameters, &audio_decoder,
                          &audio_renderer_sink, &video_decoder,
                          &video_render_algorithm, &video_renderer_sink,
                          error_message)) {
      return false;
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

  if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
    SB_DCHECK(audio_decoder);
    SB_DCHECK(audio_renderer_sink);

    int max_cached_frames, min_frames_per_append;
    GetAudioRendererParams(creation_parameters, &max_cached_frames,
                           &min_frames_per_append);

    audio_renderer->reset(
        new AudioRenderer(audio_decoder.Pass(), audio_renderer_sink.Pass(),
                          creation_parameters.audio_sample_info(),
                          max_cached_frames, min_frames_per_append));
  }
  if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
    SB_DCHECK(video_decoder);
    SB_DCHECK(video_render_algorithm);

    auto media_time_provider = creation_parameters.media_time_provider();
    if (audio_renderer && *audio_renderer) {
      media_time_provider = audio_renderer->get();
    }
    SB_DCHECK(media_time_provider);
    video_renderer->reset(
        new VideoRenderer(video_decoder.Pass(), media_time_provider,
                          video_render_algorithm.Pass(), video_renderer_sink));
  }
  return true;
}

void PlayerComponents::CreateStubAudioComponents(
    const CreationParameters& creation_parameters,
    scoped_ptr<AudioDecoder>* audio_decoder,
    scoped_ptr<AudioRendererSink>* audio_renderer_sink) {
  SB_DCHECK(audio_decoder);
  SB_DCHECK(audio_renderer_sink);

#if SB_API_VERSION >= 11
  auto decoder_creator = [](const SbMediaAudioSampleInfo& audio_sample_info,
                            SbDrmSystem drm_system) {
    return scoped_ptr<AudioDecoder>(
        new StubAudioDecoder(audio_sample_info.codec, audio_sample_info));
  };
  audio_decoder->reset(new AdaptiveAudioDecoder(
      creation_parameters.audio_sample_info(), creation_parameters.drm_system(),
      decoder_creator));
#else   // SB_API_VERSION >= 11
  audio_decoder->reset(
      new StubAudioDecoder(creation_parameters.audio_codec(),
                           creation_parameters.audio_sample_info()));
#endif  // SB_API_VERISON >= 11
  audio_renderer_sink->reset(new AudioRendererSinkImpl);
}

void PlayerComponents::CreateStubVideoComponents(
    const CreationParameters& creation_parameters,
    scoped_ptr<VideoDecoder>* video_decoder,
    scoped_ptr<VideoRenderAlgorithm>* video_render_algorithm,
    scoped_refptr<VideoRendererSink>* video_renderer_sink) {
  const SbTime kVideoSinkRenderInterval = 10 * kSbTimeMillisecond;

  SB_DCHECK(video_decoder);
  SB_DCHECK(video_render_algorithm);
  SB_DCHECK(video_renderer_sink);

  video_decoder->reset(new StubVideoDecoder);
  video_render_algorithm->reset(new VideoRenderAlgorithmImpl);
  *video_renderer_sink = new PunchoutVideoRendererSink(
      creation_parameters.player(), kVideoSinkRenderInterval);
}

void PlayerComponents::GetAudioRendererParams(
    const CreationParameters& creation_parameters,
    int* max_cached_frames,
    int* min_frames_per_append) const {
  SB_DCHECK(max_cached_frames);
  SB_DCHECK(min_frames_per_append);

#if SB_API_VERSION >= 11
  *min_frames_per_append = 1024;
  // AudioRenderer prefers to use kSbMediaAudioSampleTypeFloat32 and only uses
  // kSbMediaAudioSampleTypeInt16Deprecated when float32 is not supported.
  int min_frames_required = SbAudioSinkGetMinBufferSizeInFrames(
      creation_parameters.audio_sample_info().number_of_channels,
      SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)
          ? kSbMediaAudioSampleTypeFloat32
          : kSbMediaAudioSampleTypeInt16Deprecated,
      creation_parameters.audio_sample_info().samples_per_second);
  *max_cached_frames = min_frames_required + *min_frames_per_append * 2;
#else   // SB_API_VERSION >= 11
  *max_cached_frames = 8 * 1024;
  *min_frames_per_append = 1024;
#endif  // SB_API_VERSION >= 11
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

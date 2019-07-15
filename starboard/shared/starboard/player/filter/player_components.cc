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

scoped_ptr<AudioRenderer> PlayerComponents::CreateAudioRenderer(
    const AudioParameters& audio_parameters) {
  scoped_ptr<AudioDecoder> audio_decoder;
  scoped_ptr<AudioRendererSink> audio_renderer_sink;

  auto command_line = shared::starboard::Application::Get()->GetCommandLine();
  if (command_line->HasSwitch("use_stub_audio_decoder")) {
    CreateStubAudioComponents(audio_parameters, &audio_decoder,
                              &audio_renderer_sink);
  } else {
    CreateAudioComponents(audio_parameters, &audio_decoder,
                          &audio_renderer_sink);
  }
  if (!audio_decoder || !audio_renderer_sink) {
    return scoped_ptr<AudioRenderer>();
  }
  int max_cached_frames, max_frames_per_append;
  GetAudioRendererParams(&max_cached_frames, &max_frames_per_append);
  return make_scoped_ptr(
      new AudioRenderer(audio_decoder.Pass(), audio_renderer_sink.Pass(),
                        audio_parameters.audio_sample_info, max_cached_frames,
                        max_frames_per_append));
}

scoped_ptr<VideoRenderer> PlayerComponents::CreateVideoRenderer(
    const VideoParameters& video_parameters,
    MediaTimeProvider* media_time_provider) {
  scoped_ptr<VideoDecoder> video_decoder;
  scoped_ptr<VideoRenderAlgorithm> video_render_algorithm;
  scoped_refptr<VideoRendererSink> video_renderer_sink;

  auto command_line = shared::starboard::Application::Get()->GetCommandLine();
  if (command_line->HasSwitch("use_stub_video_decoder")) {
    CreateStubVideoComponents(video_parameters, &video_decoder,
                              &video_render_algorithm, &video_renderer_sink);
  } else {
    CreateVideoComponents(video_parameters, &video_decoder,
                          &video_render_algorithm, &video_renderer_sink);
  }
  if (!video_decoder || !video_render_algorithm) {
    return scoped_ptr<VideoRenderer>();
  }
  return make_scoped_ptr(
      new VideoRenderer(video_decoder.Pass(), media_time_provider,
                        video_render_algorithm.Pass(), video_renderer_sink));
}

void PlayerComponents::CreateStubAudioComponents(
    const AudioParameters& audio_parameters,
    scoped_ptr<AudioDecoder>* audio_decoder,
    scoped_ptr<AudioRendererSink>* audio_renderer_sink) {
  SB_DCHECK(audio_decoder);
  SB_DCHECK(audio_renderer_sink);

#if SB_API_VERSION >= 11
  auto decoder_creator = [](const SbMediaAudioSampleInfo& audio_sample_info,
                            SbDrmSystem drm_system) {
    return scoped_ptr<AudioDecoder>(new StubAudioDecoder(audio_sample_info));
  };
  audio_decoder->reset(
      new AdaptiveAudioDecoder(audio_parameters.audio_sample_info,
                               audio_parameters.drm_system, decoder_creator));
#else   // SB_API_VERSION >= 11
  audio_decoder->reset(
      new StubAudioDecoder(audio_parameters.audio_sample_info));
#endif  // SB_API_VERISON >= 11
  audio_renderer_sink->reset(new AudioRendererSinkImpl);
}

void PlayerComponents::CreateStubVideoComponents(
    const VideoParameters& video_parameters,
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
      video_parameters.player, kVideoSinkRenderInterval);
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

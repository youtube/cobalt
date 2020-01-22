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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PLAYER_COMPONENTS_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PLAYER_COMPONENTS_H_

#include <string>

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/decode_target.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// This class creates all the platform specific components that is required by
// |FilterBasedPlayerWorkerHandler| to function.
class PlayerComponents {
 public:
  struct AudioParameters {
    SbMediaAudioCodec audio_codec;
    const SbMediaAudioSampleInfo& audio_sample_info;
    SbDrmSystem drm_system;
  };

  struct VideoParameters {
    SbPlayer player;
    SbMediaVideoCodec video_codec;
    SbDrmSystem drm_system;
    SbPlayerOutputMode output_mode;
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider;
  };

  virtual ~PlayerComponents() {}

  // Individual platform should implement this function to allow the creation of
  // a PlayerComponents instance.
  static scoped_ptr<PlayerComponents> Create();

  scoped_ptr<AudioRenderer> CreateAudioRenderer(
      const AudioParameters& audio_parameters,
      std::string* error_message);

  scoped_ptr<VideoRenderer> CreateVideoRenderer(
      const VideoParameters& video_parameters,
      MediaTimeProvider* media_time_provider,
      std::string* error_message);

#if COBALT_BUILD_TYPE_GOLD
 private:
#endif  // COBALT_BUILD_TYPE_GOLD
  // Note that the following functions are exposed in non-Gold build to allow
  // unit tests to run.

  virtual bool CreateAudioComponents(
      const AudioParameters& audio_parameters,
      scoped_ptr<AudioDecoder>* audio_decoder,
      scoped_ptr<AudioRendererSink>* audio_renderer_sink,
      std::string* error_message) = 0;

  virtual bool CreateVideoComponents(
      const VideoParameters& video_parameters,
      scoped_ptr<VideoDecoder>* video_decoder,
      scoped_ptr<VideoRenderAlgorithm>* video_render_algorithm,
      scoped_refptr<VideoRendererSink>* video_renderer_sink,
      std::string* error_message) = 0;

  // Check AudioRenderer ctor for more details on the parameters.
  virtual void GetAudioRendererParams(int* max_cached_frames,
                                      int* max_frames_per_append) const = 0;

 protected:
  PlayerComponents() {}

  void CreateStubAudioComponents(
      const AudioParameters& audio_parameters,
      scoped_ptr<AudioDecoder>* audio_decoder,
      scoped_ptr<AudioRendererSink>* audio_renderer_sink);

  void CreateStubVideoComponents(
      const VideoParameters& video_parameters,
      scoped_ptr<VideoDecoder>* video_decoder,
      scoped_ptr<VideoRenderAlgorithm>* video_render_algorithm,
      scoped_refptr<VideoRendererSink>* video_renderer_sink);

 private:
  SB_DISALLOW_COPY_AND_ASSIGN(PlayerComponents);
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PLAYER_COMPONENTS_H_

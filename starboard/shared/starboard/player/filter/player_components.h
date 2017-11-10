// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

struct AudioParameters {
  SbMediaAudioCodec audio_codec;
  const SbMediaAudioHeader& audio_header;
  SbDrmSystem drm_system;
  JobQueue* job_queue;
};

struct VideoParameters {
  SbPlayer player;
  SbMediaVideoCodec video_codec;
  SbDrmSystem drm_system;
  JobQueue* job_queue;
  SbPlayerOutputMode output_mode;
  SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider;
};

// All the platform specific components that a
// |FilterBasedPlayerWorkerHandler| needs to function.  Note that the
// existence of a |PlayerComponents| object implies that it is valid.  If
// creation fails in |PlayerComponents::Create|, then NULL will be returned.
class PlayerComponents {
 public:
  // Individual implementations have to implement this function to create the
  // player components required by |FilterBasedPlayerWorkerHandler|.
  static scoped_ptr<PlayerComponents> Create(
      const AudioParameters& audio_parameters,
      const VideoParameters& video_parameters);

  // After successful creation, the client should retrieve both
  // |audio_renderer_| and |video_renderer_| from the |PlayerComponents|.
  void GetRenderers(scoped_ptr<AudioRenderer>* audio_renderer,
                    scoped_ptr<VideoRenderer>* video_renderer) {
    SB_DCHECK(audio_renderer);
    SB_DCHECK(video_renderer);
    SB_DCHECK(is_valid());
    *audio_renderer = audio_renderer_.Pass();
    *video_renderer = video_renderer_.Pass();
  }

  bool is_valid() const {
    SB_DCHECK(!!audio_renderer_ == !!video_renderer_);
    return audio_renderer_ && video_renderer_;
  }

 private:
  // |PlayerComponent|s must only be created through |Create|.
  PlayerComponents(scoped_ptr<AudioRenderer> audio_renderer,
                   scoped_ptr<VideoRenderer> video_renderer)
      : audio_renderer_(audio_renderer.Pass()),
        video_renderer_(video_renderer.Pass()) {
    SB_DCHECK(is_valid());
  }
  scoped_ptr<AudioRenderer> audio_renderer_;
  scoped_ptr<VideoRenderer> video_renderer_;

  SB_DISALLOW_COPY_AND_ASSIGN(PlayerComponents);
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_PLAYER_COMPONENTS_H_

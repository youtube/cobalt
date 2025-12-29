// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_PLAYER_CREATION_PARAM_HELPERS_H_
#define STARBOARD_NPLB_PLAYER_CREATION_PARAM_HELPERS_H_

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace nplb {

// Encapsulates all information contained in `SbPlayerCreationParam`.  It
// doesn't aim to maintain the same binary layout as `SbPlayerCreationParam`,
// and is intended to be used as a C++ wrapper in unit test code to allow of
// converting between types more conveniently.
struct PlayerCreationParam {
  SbDrmSystem drm_system = kSbDrmSystemInvalid;

  starboard::AudioStreamInfo audio_stream_info;
  starboard::VideoStreamInfo video_stream_info;

  SbPlayerOutputMode output_mode = kSbPlayerOutputModeInvalid;

  // Convert to an SbPlayerCreationParam that's only valid during the life time
  // of this PlayerCreationParam instance, as the member pointers of the
  // SbPlayerCreationParam point to memory held in this PlayerCreationParam
  // instance.
  void ConvertTo(SbPlayerCreationParam* creation_param) const {
    SB_DCHECK(creation_param);

    *creation_param = {};

    creation_param->drm_system = drm_system;

    audio_stream_info.ConvertTo(&creation_param->audio_stream_info);
    video_stream_info.ConvertTo(&creation_param->video_stream_info);

    creation_param->output_mode = output_mode;
  }
};

starboard::AudioStreamInfo CreateAudioStreamInfo(SbMediaAudioCodec codec);
starboard::VideoStreamInfo CreateVideoStreamInfo(SbMediaVideoCodec codec);
PlayerCreationParam CreatePlayerCreationParam(SbMediaAudioCodec audio_codec,
                                              SbMediaVideoCodec video_codec,
                                              SbPlayerOutputMode output_mode);

PlayerCreationParam CreatePlayerCreationParam(const SbPlayerTestConfig& config);

SbPlayerOutputMode GetPreferredOutputMode(
    const PlayerCreationParam& creation_param);

}  // namespace nplb

#endif  // STARBOARD_NPLB_PLAYER_CREATION_PARAM_HELPERS_H_

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

#include "starboard/configuration.h"

#include "starboard/media.h"
#include "starboard/player.h"

namespace starboard {
namespace nplb {

SbMediaAudioSampleInfo CreateAudioSampleInfo(SbMediaAudioCodec codec);
SbMediaVideoSampleInfo CreateVideoSampleInfo(SbMediaVideoCodec codec);
SbPlayerCreationParam CreatePlayerCreationParam(SbMediaAudioCodec audio_codec,
                                                SbMediaVideoCodec video_codec);

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_PLAYER_CREATION_PARAM_HELPERS_H_

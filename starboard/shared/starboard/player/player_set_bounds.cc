// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/player.h"

#include "starboard/log.h"
#include "starboard/shared/starboard/player/player_internal.h"

#if SB_API_VERSION >= 4 || SB_IS(PLAYER_PUNCHED_OUT)

void SbPlayerSetBounds(SbPlayer player,
#if SB_API_VERSION >= 4
                       int /*z_index*/,
#endif  // SB_API_VERSION >= 4
                       int x,
                       int y,
                       int width,
                       int height) {
  if (!SbPlayerIsValid(player)) {
    SB_DLOG(WARNING) << "player is invalid.";
    return;
  }
  player->SetBounds(x, y, width, height);
}

#endif  // SB_API_VERSION >= 4 || \
           SB_IS(PLAYER_PUNCHED_OUT)

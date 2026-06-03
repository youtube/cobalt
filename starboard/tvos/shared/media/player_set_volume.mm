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

#include "starboard/player.h"
#include "starboard/shared/starboard/player/player_internal.h"
#import "starboard/tvos/shared/media/application_player.h"
#import "starboard/tvos/shared/media/player_manager.h"
#import "starboard/tvos/shared/starboard_application.h"

void SbPlayerSetVolume(SbPlayer player, double volume) {
  if (!player) {
    return;
  }
  @autoreleasepool {
    SBDPlayerManager* playerManager = SBDGetApplication().playerManager;
    if ([playerManager isUrlPlayer:player]) {
      // Url player process
      SBDApplicationPlayer* applicationPlayer =
          [playerManager applicationPlayerForStarboardPlayer:player];
      applicationPlayer.volume = volume;
      return;
    }
  }
  // Normal player process
  player->SetVolume(volume);
}

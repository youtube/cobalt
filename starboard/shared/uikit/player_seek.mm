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

#include "starboard/media.h"
#include "starboard/shared/starboard/player/player_internal.h"

#import "starboard/shared/uikit/application_player.h"
#import "starboard/shared/uikit/player_manager.h"
#import "starboard/shared/uikit/starboard_application.h"

#if SB_API_VERSION >= 15
void SbPlayerSeek(SbPlayer player, int64_t seek_to_timestamp, int ticket) {
#else   // SB_API_VERSION >= 15
void SbPlayerSeek2(SbPlayer player, int64_t seek_to_timestamp, int ticket) {
#endif  // SB_API_VERSION >= 15
  if (!player) {
    return;
  }
  @autoreleasepool {
    SBDPlayerManager* playerManager = SBDGetApplication().playerManager;
    if ([playerManager isUrlPlayer:player]) {
      // Url player process
      SBDApplicationPlayer* applicationPlayer =
          [playerManager applicationPlayerForStarboardPlayer:player];
      [applicationPlayer seekTo:seek_to_timestamp ticket:ticket];
      return;
    }
  }
  // Normal player process
  player->Seek(seek_to_timestamp, ticket);
}

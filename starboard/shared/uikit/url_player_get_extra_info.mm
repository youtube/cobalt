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

#import <Foundation/Foundation.h>

#include "starboard/common/log.h"
#include "starboard/player.h"

#import "starboard/shared/uikit/application_player.h"
#import "starboard/shared/uikit/player_manager.h"
#import "starboard/shared/uikit/starboard_application.h"

void SbUrlPlayerGetExtraInfo(SbPlayer player,
                             SbUrlPlayerExtraInfo* out_url_player_info) {
  if (!player) {
    return;
  }
  if (!out_url_player_info) {
    return;
  }
  @autoreleasepool {
    SBDPlayerManager* playerManager = SBDGetApplication().playerManager;
    SB_DCHECK([playerManager isUrlPlayer:player]);
    SBDApplicationPlayer* applicationPlayer =
        [playerManager applicationPlayerForStarboardPlayer:player];
    NSRange bufferRange = applicationPlayer.bufferRange;
    out_url_player_info->buffer_start_timestamp = bufferRange.location;
    out_url_player_info->buffer_duration = bufferRange.length;
  }
}

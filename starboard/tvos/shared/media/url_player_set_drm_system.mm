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

#include "starboard/common/log.h"
#include "starboard/drm.h"
#include "starboard/player.h"
#import "starboard/tvos/shared/media/application_player.h"
#import "starboard/tvos/shared/media/drm_manager.h"
#import "starboard/tvos/shared/media/player_manager.h"
#import "starboard/tvos/shared/starboard_application.h"

void SbUrlPlayerSetDrmSystem(SbPlayer player, SbDrmSystem drm_system) {
  if (!player) {
    return;
  }

  @autoreleasepool {
    id<SBDStarboardApplication> application = SBDGetApplication();
    SBDDrmManager* drmManager = application.drmManager;
    SBDPlayerManager* playerManager = application.playerManager;

    SB_DCHECK([drmManager isApplicationDrmSystem:drm_system]);
    SB_DCHECK([playerManager isUrlPlayer:player]);

    SBDApplicationDrmSystem* applicationDrmSystem =
        [drmManager applicationDrmSystemForStarboardDrmSystem:drm_system];
    SBDApplicationPlayer* applicationPlayer =
        [playerManager applicationPlayerForStarboardPlayer:player];
    applicationPlayer.drmSystem = applicationDrmSystem;
  }
}

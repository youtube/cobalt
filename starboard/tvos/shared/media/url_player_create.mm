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

#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/player.h"
#import "starboard/tvos/shared/media/player_manager.h"
#import "starboard/tvos/shared/starboard_application.h"
#import "starboard/tvos/shared/window_manager.h"

SbPlayer SbUrlPlayerCreate(const char* url,
                           SbWindow window,
                           SbPlayerStatusFunc player_status_func,
                           SbPlayerEncryptedMediaInitDataEncounteredCB
                               encrypted_media_init_data_encountered_cb,
                           SbPlayerErrorFunc player_error_func,
                           void* context) {
  if (!player_status_func || !encrypted_media_init_data_encountered_cb ||
      !player_error_func) {
    return kSbPlayerInvalid;
  }

  @autoreleasepool {
    id<SBDStarboardApplication> application = SBDGetApplication();
    SBDWindowManager* windowManager = application.windowManager;
    SBDApplicationWindow* applicationWindow =
        [windowManager applicationWindowForStarboardWindow:window];
    SBDPlayerManager* playerManager = application.playerManager;

    NSString* urlString = [NSString stringWithCString:url
                                             encoding:NSUTF8StringEncoding];
    NSURL* urlObject = [NSURL URLWithString:urlString];
    SBDApplicationPlayer* player =
        [playerManager playerWithUrl:urlObject
                       playerContext:context
                    playerStatusFunc:player_status_func
              encryptedMediaCallback:encrypted_media_init_data_encountered_cb
                     playerErrorFunc:player_error_func
                            inWindow:applicationWindow];
    return [playerManager starboardPlayerForApplicationPlayer:player];
  }
}

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

#import "starboard/shared/uikit/player_manager.h"

#import <Foundation/Foundation.h>

#include <algorithm>

#import "starboard/shared/uikit/application_player.h"
#import "starboard/shared/uikit/application_window.h"
#import "starboard/shared/uikit/defines.h"
#import "starboard/shared/uikit/starboard_application.h"
#import "starboard/common/log.h"

@implementation SBDPlayerManager {
  /**
   *  @brief This maintains a strong reference to all Url players.
   */
  NSMutableArray<SBDApplicationPlayer*>* _urlPlayers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _urlPlayers = [[NSMutableArray alloc] init];
  }
  return self;
}

- (SBDApplicationPlayer*)playerWithUrl:(NSURL*)url
                         playerContext:(void*)playerContext
                      playerStatusFunc:(SbPlayerStatusFunc)statusFunc
                encryptedMediaCallback:
                    (SbPlayerEncryptedMediaInitDataEncounteredCB)
                        encryptedMediaCallback
                       playerErrorFunc:(SbPlayerErrorFunc)errorFunc
                              inWindow:(SBDApplicationWindow*)window {
  __block SBDApplicationPlayer* player;
  onApplicationMainThread(^{
    player = [[SBDApplicationPlayer alloc] initWithUrl:url
                                         playerContext:playerContext
                                      playerStatusFunc:statusFunc
                                encryptedMediaCallback:encryptedMediaCallback
                                       playerErrorFunc:errorFunc];
    [window attachPlayerView:player.view];
  });

  @synchronized(self) {
    [_urlPlayers addObject:player];
  }
  return player;
}

- (void)destroyUrlPlayer:(SBDApplicationPlayer*)player {
  @synchronized(self) {
    [_urlPlayers removeObject:player];
  }

  // Synchronously disable callbacks so they won't be invoked by the time this
  // function returns. The player itself will be destroyed asynchronously.
  [player disableCallbacks];
  dispatch_async(dispatch_get_main_queue(), ^{
    [player.view removeFromSuperview];
    [player destroy];
  });
}

- (SBDApplicationPlayer*)applicationPlayerForStarboardPlayer:(SbPlayer)player {
  return (__bridge SBDApplicationPlayer*)player;
}

- (SbPlayer)starboardPlayerForApplicationPlayer:(SBDApplicationPlayer*)player {
  return (__bridge SbPlayer)player;
}

- (bool)isUrlPlayer:(SbPlayer)player {
  SB_CHECK(SbPlayerIsValid(player));
  @synchronized(self) {
    for (SBDApplicationPlayer* applicationPlayer : _urlPlayers) {
      if ([self starboardPlayerForApplicationPlayer:applicationPlayer] ==
          player) {
        return true;
      }
    }
    return false;
  }
}

- (HdcpProtectionState)hdcpState {
  @synchronized(self) {
    for (SBDApplicationPlayer* applicationPlayer : _urlPlayers) {
      HdcpProtectionState state = applicationPlayer.hdcpState;
      if (state != kHdcpProtectionStateUnknown) {
        return applicationPlayer.hdcpState;
      }
    }
  }
  return kHdcpProtectionStateUnknown;
}

@end

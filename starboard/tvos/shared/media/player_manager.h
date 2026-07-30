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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_PLAYER_MANAGER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_PLAYER_MANAGER_H_

#import <Foundation/Foundation.h>

#include "starboard/player.h"
#import "starboard/tvos/shared/media/application_player.h"
#include "starboard/tvos/shared/media/url_player.h"

@class SBDApplicationPlayer;

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief Maintains a collection of players created via the Starboard
 *      interface.
 */
@interface SBDPlayerManager : NSObject

/**
 *  @brief Create a URL player which will be displayed in the specified window.
 */
- (SBDApplicationPlayer*)playerWithUrl:(NSURL*)url
                         playerContext:(void*)playerContext
                      playerStatusFunc:(SbPlayerStatusFunc)statusFunc
                encryptedMediaCallback:
                    (SbPlayerEncryptedMediaInitDataEncounteredCB)
                        encryptedMediaCallback
                       playerErrorFunc:(SbPlayerErrorFunc)errorFunc;

/**
 *  @brief Destroy the specified player.
 */
- (void)destroyUrlPlayer:(SBDApplicationPlayer*)player;

/**
 *  @brief Returns the application player associated with the given SbPlayer.
 */
- (SBDApplicationPlayer*)applicationPlayerForStarboardPlayer:(SbPlayer)player;

/**
 *  @brief Returns the SbPlayer associated with the given SBDApplicationPlayer.
 */
- (SbPlayer)starboardPlayerForApplicationPlayer:(SBDApplicationPlayer*)player;

/**
 *  @brief Returns true if the player is a URL player.
 */
- (bool)isUrlPlayer:(SbPlayer)player;

/**
 *  @brief Returns the hdcp state.
 */
- (HdcpProtectionState)hdcpState;

/**
 *  @brief Designated initializer
 */
- (instancetype)init;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_MEDIA_PLAYER_MANAGER_H_

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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_APPLICATION_PLAYER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_APPLICATION_PLAYER_H_

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

#include "starboard/player.h"
#include "starboard/tvos/shared/media/url_player.h"

NS_ASSUME_NONNULL_BEGIN

@class SBDApplication;
@class SBDApplicationDrmSystem;

enum HdcpProtectionState {
  kHdcpProtectionStateUnknown,
  kHdcpProtectionStateUnauthenticated,
  kHdcpProtectionStateAuthenticated,
};

/**
 *  @brief Wraps a single @c AVPlayer and its @c UIView.
 */
@interface SBDApplicationPlayer : NSObject <AVContentKeySessionDelegate>

/**
 *  @brief The view which should be added to the view hierarchy.
 */
@property(nonatomic, readonly) UIView* view;

/**
 *  @brief The duration of the current video in microseconds.
 */
@property(nonatomic, readonly) NSInteger duration;

/**
 *  @brief The current time of the playback as the number of seconds since 1970.
 *      If playback is not mapped to a date, 0 is returned.
 */
@property(nonatomic, readonly) NSTimeInterval currentDate;

/**
 *  @brief The width of the player's current frame.
 */
@property(nonatomic, readonly) NSInteger frameWidth;

/**
 *  @brief The height of the player's current frame.
 */
@property(nonatomic, readonly) NSInteger frameHeight;

/**
 *  @brief The total number of dropped frames during the lifetime of the player.
 */
@property(nonatomic, readonly) NSInteger totalDroppedFrames;

/**
 *  @brief The total number of frames displayed during the lifetime of the
 *      player.
 */
@property(nonatomic, readonly) NSInteger totalFrames;

/**
 *  @brief The range of the buffer, in microseconds.
 */
@property(nonatomic, readonly) NSRange bufferRange;

/**
 *  @brief The current position of the play head in microseconds.
 */
@property(nonatomic) NSInteger currentMediaTime;

/**
 *  @brief The rate of playback for this player.
 */
@property(nonatomic) double playbackRate;

/**
 *  @brief The volume of the player.
 */
@property(nonatomic) double volume;

/**
 *  @brief The playback status of the player.
 */
@property(nonatomic) BOOL paused;

/**
 *  @brief The DRM system associated with the player.
 */
@property(nonatomic) SBDApplicationDrmSystem* drmSystem;

/**
 *  @brief Similar to setting the currentMediaTime, but ensure it is done
 *      on the proper thread.
 */
- (void)seekTo:(NSInteger)time ticket:(int)ticket;

/**
 *  @brief Sets the frame for the player.
 *  @param x The leftmost position for the player.
 *  @param y The topmost position for the player.
 *  @param width The width for the player.
 *  @param height The height for the player.
 */
- (void)setBoundsX:(NSInteger)x
                 y:(NSInteger)y
             width:(NSInteger)width
            height:(NSInteger)height;

/**
 *  @brief Sets the z index for the player.
 *  @param zIndex The position in the stack of players that this player
 *      should be positioned.
 */
- (void)setZIndex:(NSInteger)zIndex;

- (instancetype)init NS_UNAVAILABLE;

/**
 *  @brief Designated Initializer.
 *  @param url The URL that the player should play.
 */
- (instancetype)initWithUrl:(NSURL*)url
              playerContext:(void*)playerContext
           playerStatusFunc:(SbPlayerStatusFunc)statusFunc
     encryptedMediaCallback:
         (SbPlayerEncryptedMediaInitDataEncounteredCB)encryptedMediaCallback
            playerErrorFunc:(SbPlayerErrorFunc)errorFunc
    NS_DESIGNATED_INITIALIZER;

/**
 *  @brief Disable player callbacks. This can be called from any thread and
 *      ensures the player callbacks will not be used upon return.
 */
- (void)disableCallbacks;

/**
 *  @brief Destroys the player.
 *  @remarks Must be called on the main thread.
 */
- (void)destroy;

/**
 *  @brief Returns the hdcp state.
 */
- (HdcpProtectionState)hdcpState;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_MEDIA_APPLICATION_PLAYER_H_

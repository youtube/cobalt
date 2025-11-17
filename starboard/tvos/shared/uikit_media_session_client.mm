// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/tvos/shared/uikit_media_session_client.h"

#import <Foundation/Foundation.h>
#import <MediaPlayer/MediaPlayer.h>

#include <limits>
#include <memory>
#include <vector>

#include "starboard/common/log.h"

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief This object translates commands from MPRemoteCommandCenter to
 *      MediaSessionActions.
 */
@interface SBDMediaSessionClient : NSObject
+ (SBDMediaSessionClient*)sharedInstance;
- (void)onSessionChanged:(CobaltExtensionMediaSessionState)sessionState;
- (void)onRegisterCallbacks:(void*)callbackContext
       invokeActionCallback:(CobaltExtensionMediaSessionInvokeActionCallback)
                                invokeActionCallback;
- (void)onUnregisterCallbacks;
@end

NS_ASSUME_NONNULL_END

@implementation SBDMediaSessionClient {
  /**
   *  @brief Media information to be displayed in "now playing".
   */
  NSDictionary<NSString*, id>* _mediaInfo;

  /**
   *  @brief Callbacks to the last MediaSessionClient to become active, or null.
   *    In practice, only one MediaSessionClient will become active at a time.
   *    Protected by the @synchronized directive.
   */
  void* registered_callback_context;
  CobaltExtensionMediaSessionInvokeActionCallback registered_callback_function;
}

+ (SBDMediaSessionClient*)sharedInstance {
  // Ensure the singleton exists.
  static SBDMediaSessionClient* instance = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    instance = [[self alloc] init];
  });
  return instance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    MPRemoteCommandCenter* remote = [MPRemoteCommandCenter sharedCommandCenter];

    // These commands are supported only when a media session is active.
    remote.nextTrackCommand.enabled = NO;
    remote.previousTrackCommand.enabled = NO;
    remote.pauseCommand.enabled = NO;
    remote.playCommand.enabled = NO;
    remote.changePlaybackPositionCommand.enabled = NO;
    remote.skipBackwardCommand.enabled = NO;
    remote.skipForwardCommand.enabled = NO;

    // Reset the "now playing" info. This gets updated on session changed.
    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nil;

    [remote.nextTrackCommand addTarget:self action:@selector(nextTrack:)];
    [remote.previousTrackCommand addTarget:self
                                    action:@selector(previousTrack:)];
    [remote.pauseCommand addTarget:self action:@selector(pause:)];
    [remote.playCommand addTarget:self action:@selector(play:)];
    [remote.changePlaybackPositionCommand
        addTarget:self
           action:@selector(changePlaybackPosition:)];
    [remote.skipBackwardCommand addTarget:self action:@selector(skipBackward:)];
    [remote.skipForwardCommand addTarget:self action:@selector(skipForward:)];
  }
  return self;
}

- (void)onSessionChanged:(CobaltExtensionMediaSessionState)sessionState {
  @synchronized(self) {
    if (sessionState.actual_playback_state !=
            kCobaltExtensionMediaSessionNone &&
        sessionState.metadata != NULL) {
      CobaltExtensionMediaMetadata* metadata(sessionState.metadata);
      NSString* album = [NSString stringWithUTF8String:metadata->album];
      NSString* artist = [NSString stringWithUTF8String:metadata->artist];
      NSString* title = [NSString stringWithUTF8String:metadata->title];
      _mediaInfo = @{
        MPMediaItemPropertyAlbumTitle : album,
        MPMediaItemPropertyArtist : artist,
        MPMediaItemPropertyTitle : title,
      };
    } else {
      _mediaInfo = nil;
    }
  }

  dispatch_async(dispatch_get_main_queue(), ^{
    [self updateMediaState:sessionState];
  });
}

- (void)onRegisterCallbacks:(void*)callbackContext
       invokeActionCallback:(CobaltExtensionMediaSessionInvokeActionCallback)
                                invokeActionCallback {
  @synchronized(self) {
    registered_callback_context = callbackContext;
    registered_callback_function = invokeActionCallback;
  }
}

- (void)onUnregisterCallbacks {
  @synchronized(self) {
    registered_callback_context = NULL;
    registered_callback_function = NULL;
  }
}

- (void)updateMediaState:(CobaltExtensionMediaSessionState)sessionState {
  // This should only be updated from one thread, otherwise the state of
  // MPRemoteCommandCenter or MPNowPlayingInfoCenter can get out of sync.
  SB_CHECK([NSThread isMainThread]);

  NSMutableDictionary<NSString*, id>* newInfo;
  @synchronized(self) {
    // Get a local copy of _mediaInfo to be customized.
    newInfo = [[NSMutableDictionary alloc] initWithDictionary:_mediaInfo];
  }

  const bool* availableActions = sessionState.available_actions;
  bool playbackIsActive =
      sessionState.actual_playback_state != kCobaltExtensionMediaSessionNone;

  if (playbackIsActive && sessionState.has_position_state &&
      sessionState.duration == 0) {
    // When seeking, the player may temporarily report duration as 0.
    // Propagate the current state until the media state can be determined.
    return;
  }

  bool playbackIsSeekable =
      sessionState.has_position_state &&
      sessionState.duration != std::numeric_limits<int64_t>::max() &&
      sessionState.duration > 0;

  // Show / Hide controls based on the playback state.
  MPRemoteCommandCenter* remote = [MPRemoteCommandCenter sharedCommandCenter];
  remote.nextTrackCommand.enabled =
      playbackIsActive &&
      availableActions[kCobaltExtensionMediaSessionActionNexttrack];
  remote.previousTrackCommand.enabled =
      playbackIsActive &&
      availableActions[kCobaltExtensionMediaSessionActionPrevioustrack];
  remote.pauseCommand.enabled =
      playbackIsActive &&
      availableActions[kCobaltExtensionMediaSessionActionPause];
  remote.playCommand.enabled =
      playbackIsActive &&
      availableActions[kCobaltExtensionMediaSessionActionPlay];
  remote.changePlaybackPositionCommand.enabled =
      playbackIsActive && playbackIsSeekable &&
      availableActions[kCobaltExtensionMediaSessionActionSeekto];
  remote.skipBackwardCommand.enabled =
      playbackIsActive && playbackIsSeekable &&
      availableActions[kCobaltExtensionMediaSessionActionSeekbackward];
  remote.skipForwardCommand.enabled =
      playbackIsActive && playbackIsSeekable &&
      availableActions[kCobaltExtensionMediaSessionActionSeekforward];

  // Update "now playing" information.
  if (newInfo.count == 0) {
    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nil;
    return;
  }
  newInfo[MPNowPlayingInfoPropertyPlaybackRate] =
      @(sessionState.actual_playback_rate);
  if (!playbackIsSeekable) {
    newInfo[MPNowPlayingInfoPropertyIsLiveStream] = @(YES);
  } else {
    NSTimeInterval duration =
        static_cast<NSTimeInterval>(sessionState.duration) / 1000000;
    NSTimeInterval position =
        static_cast<NSTimeInterval>(sessionState.current_playback_position) /
        1000000;
    newInfo[MPMediaItemPropertyPlaybackDuration] = @(duration);
    newInfo[MPNowPlayingInfoPropertyElapsedPlaybackTime] = @(position);
  }
  [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = newInfo;
}

- (void)invokeAction:(CobaltExtensionMediaSessionActionDetails)actionDetails {
  @synchronized(self) {
    if (registered_callback_function != NULL &&
        registered_callback_context != NULL) {
      registered_callback_function(actionDetails, registered_callback_context);
    }
  }
}

- (MPRemoteCommandHandlerStatus)nextTrack:(MPRemoteCommandEvent*)event {
  CobaltExtensionMediaSessionActionDetails actionDetails = {};
  CobaltExtensionMediaSessionActionDetailsInit(
      &actionDetails, kCobaltExtensionMediaSessionActionNexttrack);
  [self invokeAction:CobaltExtensionMediaSessionActionDetails(actionDetails)];
  return MPRemoteCommandHandlerStatusSuccess;
}

- (MPRemoteCommandHandlerStatus)previousTrack:(MPRemoteCommandEvent*)event {
  CobaltExtensionMediaSessionActionDetails actionDetails = {};
  CobaltExtensionMediaSessionActionDetailsInit(
      &actionDetails, kCobaltExtensionMediaSessionActionPrevioustrack);
  [self invokeAction:CobaltExtensionMediaSessionActionDetails(actionDetails)];
  return MPRemoteCommandHandlerStatusSuccess;
}

- (MPRemoteCommandHandlerStatus)pause:(MPRemoteCommandEvent*)event {
  CobaltExtensionMediaSessionActionDetails actionDetails;
  CobaltExtensionMediaSessionActionDetailsInit(
      &actionDetails, kCobaltExtensionMediaSessionActionPause);
  [self invokeAction:CobaltExtensionMediaSessionActionDetails(actionDetails)];
  return MPRemoteCommandHandlerStatusSuccess;
}

- (MPRemoteCommandHandlerStatus)play:(MPRemoteCommandEvent*)event {
  CobaltExtensionMediaSessionActionDetails actionDetails;
  CobaltExtensionMediaSessionActionDetailsInit(
      &actionDetails, kCobaltExtensionMediaSessionActionPlay);
  [self invokeAction:CobaltExtensionMediaSessionActionDetails(actionDetails)];
  return MPRemoteCommandHandlerStatusSuccess;
}

- (MPRemoteCommandHandlerStatus)changePlaybackPosition:
    (MPChangePlaybackPositionCommandEvent*)event {
  CobaltExtensionMediaSessionActionDetails actionDetails;
  CobaltExtensionMediaSessionActionDetailsInit(
      &actionDetails, kCobaltExtensionMediaSessionActionSeekto);
  actionDetails.seek_time = event.positionTime;
  [self invokeAction:CobaltExtensionMediaSessionActionDetails(actionDetails)];
  return MPRemoteCommandHandlerStatusSuccess;
}

- (MPRemoteCommandHandlerStatus)skipBackward:
    (MPSkipIntervalCommandEvent*)event {
  CobaltExtensionMediaSessionActionDetails actionDetails;
  CobaltExtensionMediaSessionActionDetailsInit(
      &actionDetails, kCobaltExtensionMediaSessionActionSeekbackward);
  actionDetails.seek_offset = event.interval;
  [self invokeAction:CobaltExtensionMediaSessionActionDetails(actionDetails)];
  return MPRemoteCommandHandlerStatusSuccess;
}

- (MPRemoteCommandHandlerStatus)skipForward:(MPSkipIntervalCommandEvent*)event {
  CobaltExtensionMediaSessionActionDetails actionDetails;
  CobaltExtensionMediaSessionActionDetailsInit(
      &actionDetails, kCobaltExtensionMediaSessionActionSeekforward);
  actionDetails.seek_offset = event.interval;
  [self invokeAction:CobaltExtensionMediaSessionActionDetails(actionDetails)];
  return MPRemoteCommandHandlerStatusSuccess;
}

@end

namespace starboard {
namespace {

void OnMediaSessionStateChanged(CobaltExtensionMediaSessionState sessionState) {
  @autoreleasepool {
    [[SBDMediaSessionClient sharedInstance] onSessionChanged:sessionState];
  }
}

void RegisterMediaSessionCallbacks(
    void* callback_context,
    CobaltExtensionMediaSessionInvokeActionCallback invoke_action_callback,
    CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback
        update_platform_playback_state_callback) {
  @autoreleasepool {
    [[SBDMediaSessionClient sharedInstance]
         onRegisterCallbacks:callback_context
        invokeActionCallback:invoke_action_callback];
  }
}

void DestroyMediaSessionClientCallback() {
  @autoreleasepool {
    [[SBDMediaSessionClient sharedInstance] onUnregisterCallbacks];
  }
}

void UpdateActiveSessionPlatformPlaybackState(
    CobaltExtensionMediaSessionPlaybackState state) {
  SB_NOTIMPLEMENTED();
}
}  // namespace

const CobaltExtensionMediaSessionApi kMediaSessionApi = {
    kCobaltExtensionMediaSessionName,
    1,
    &OnMediaSessionStateChanged,
    &RegisterMediaSessionCallbacks,
    &DestroyMediaSessionClientCallback,
    &UpdateActiveSessionPlatformPlaybackState};

const void* GetMediaSessionApi() {
  return &kMediaSessionApi;
}

}  // namespace starboard

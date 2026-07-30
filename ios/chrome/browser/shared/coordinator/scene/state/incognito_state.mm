// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"

#import <string_view>

#import "base/apple/foundation_util.h"
#import "base/ios/crb_protocol_observers.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_prefs.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_lock_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

// Preference key used to store which profile is current.
constexpr std::string_view kIncognitoCurrentKey = "IncognitoActive";

}  // namespace

@interface IncognitoStateObserverList
    : CRBProtocolObservers <IncognitoStateObserver>
@end
@implementation IncognitoStateObserverList
@end

@implementation IncognitoState {
  IncognitoStateObserverList* _observers;
  BOOL _incognitoContentVisible;
}

@synthesize lockState = _lockState;

- (instancetype)initWithSceneState:(SceneState*)sceneState {
  self = [super init];
  if (self) {
    _observers = [IncognitoStateObserverList
        observersWithProtocol:@protocol(IncognitoStateObserver)];
    _sceneState = sceneState;
  }
  return self;
}

- (void)addObserver:(id<IncognitoStateObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<IncognitoStateObserver>)observer {
  [_observers removeObserver:observer];
}

- (void)preferencesDidLoad {
  const bool incognitoContentVisible =
      [_sceneState.prefs boolForKey:kIncognitoCurrentKey];
  [self setIncognitoContentVisible:incognitoContentVisible
             saveInSceneStatePrefs:NO];
}

- (BOOL)incognitoContentVisible {
  return _incognitoContentVisible;
}

- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible {
  [self setIncognitoContentVisible:incognitoContentVisible
             saveInSceneStatePrefs:YES];
}

- (BOOL)isAuthenticationRequired {
  return self.lockState != IncognitoLockState::kNone;
}

- (void)setLockState:(IncognitoLockState)lockState {
  if (_lockState == lockState) {
    return;
  }
  BOOL wasAuthenticationRequired = self.isAuthenticationRequired;
  _lockState = lockState;
  if (IsIOSSoftLockEnabled()) {
    [_observers didUpdateIncognitoLockStateForState:self];
  } else if (wasAuthenticationRequired != self.isAuthenticationRequired) {
    [_observers didUpdateAuthenticationRequirementForState:self];
  }
}

// Helper method that sets the property `-incognitoContentVisible`, notify
// the observers and optionally update the value in the SceneStatePrefs if
// `saveInSceneStatePrefs` is true.
- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible
             saveInSceneStatePrefs:(BOOL)saveInSceneStatePrefs {
  if (_incognitoContentVisible == incognitoContentVisible) {
    return;
  }

  _incognitoContentVisible = incognitoContentVisible;
  if (_incognitoContentVisible) {
    [_observers willEnterIncognitoForState:self];
  } else {
    [_observers willExitIncognitoForState:self];
  }
  if (saveInSceneStatePrefs) {
    [_sceneState.prefs setBool:_incognitoContentVisible
                        forKey:kIncognitoCurrentKey];
  }
}

@end

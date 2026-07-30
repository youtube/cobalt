// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_state_prefs.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/json/values_util.h"
#import "base/values.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

// The way the SceneState scoped preferences are saved have evolved over
// time with the addition of the support for multi-window and then multi-
// profile.
//
// Historically, before introduction of multi-window or multi-profile,
// those preferences were not scoped to a SceneState and were saved into
// NSUserDefaults.
//
// When multi-window was introduced, the preferences should now be scoped
// to SceneState, and thus were moved to the UISceneSession -userInfo. It
// was not used on iPhone though because the swipe gesture to terminate
// the app was sometimes interpreted by the OS as a request to close the
// window instead, resulting in the destruction of the storage.
//
// For multi-profile, the value should be scoped to the SceneState and the
// profile, but this has not yet been implemented.

@implementation SceneStatePrefs {
  std::string _sessionIdentifier;
  std::string _profileName;
  UISceneSession* _sceneSession;
}

- (instancetype)initWithSessionIdentifier:(std::string)sessionIdentifier
                              profileName:(std::string)profileName
                             sceneSession:(UISceneSession*)sceneSession {
  CHECK(!sessionIdentifier.empty());
  CHECK(!profileName.empty());
  if ((self = [super init])) {
    _sessionIdentifier = std::move(sessionIdentifier);
    _profileName = std::move(profileName);
    _sceneSession = sceneSession;
  }
  return self;
}

- (std::optional<bool>)boolForKey:(NSString*)key {
  NSObject* object = [self valueForKey:key];
  if (NSNumber* number = base::apple::ObjCCast<NSNumber>(object)) {
    return [number boolValue];
  }
  return std::nullopt;
}

- (void)setBool:(bool)value forKey:(NSString*)key {
  [self setValue:[NSNumber numberWithBool:value] forKey:key];
}

- (std::optional<base::Time>)timeForKey:(NSString*)key {
  NSObject* object = [self valueForKey:key];
  if (NSDate* date = base::apple::ObjCCast<NSDate>(object)) {
    return base::Time::FromNSDate(date);
  }
  return std::nullopt;
}

- (void)setTime:(base::Time)value forKey:(NSString*)key {
  [self setValue:value.ToNSDate() forKey:key];
}

#pragma mark - Private methods

// Getter for NSObject preference with the given key.
- (NSObject*)valueForKey:(NSString*)key {
  NSObject* object = nil;
  if (_sceneSession) {
    if ((object = [_sceneSession.userInfo objectForKey:key])) {
      [[NSUserDefaults standardUserDefaults] removeObjectForKey:key];
    }
  }
  if (!object) {
    object = [[NSUserDefaults standardUserDefaults] objectForKey:key];
  }
  return object;
}

// Setter for NSObject preference with the given key.
- (void)setValue:(NSObject*)value forKey:(NSString*)key {
  if (_sceneSession) {
    NSMutableDictionary<NSString*, id>* userInfo =
        [NSMutableDictionary dictionaryWithDictionary:_sceneSession.userInfo];
    [userInfo setObject:value forKey:key];
    _sceneSession.userInfo = userInfo;
  } else {
    [[NSUserDefaults standardUserDefaults] setObject:value forKey:key];
  }
}

@end

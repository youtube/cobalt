// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_PREFS_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_PREFS_H_

#import <Foundation/Foundation.h>

#import <optional>
#import <string>
#import <string_view>

#import "base/time/time.h"

@class UISceneSession;

// Provides access to SceneState scoped preferences.
@interface SceneStatePrefs : NSObject

// Designated initializer.
- (instancetype)initWithSessionIdentifier:(std::string)sessionIdentifier
                              profileName:(std::string)profileName
                             sceneSession:(UISceneSession*)sceneSession
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Getter and setter for boolean preference with the given key.
- (std::optional<bool>)boolForKey:(NSString*)key;
- (void)setBool:(bool)value forKey:(NSString*)key;

// Getter and setter for base::Time preference with the given key.
- (std::optional<base::Time>)timeForKey:(NSString*)key;
- (void)setTime:(base::Time)value forKey:(NSString*)key;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_PREFS_H_

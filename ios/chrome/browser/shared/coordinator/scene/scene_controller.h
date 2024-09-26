// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

@protocol BrowserProviderInterface;

// The controller object for a scene. Reacts to scene state changes.
@interface SceneController : NSObject <SceneStateObserver,
                                       ApplicationCommands,
                                       ConnectionInformation,
                                       TabOpening,
                                       WebStateListObserving>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithSceneState:(SceneState*)sceneState
    NS_DESIGNATED_INITIALIZER;

// The state of the scene controlled by this object.
@property(nonatomic, weak, readonly) SceneState* sceneState;

// The interface provider for this scene.
@property(nonatomic, strong, readonly) id<BrowserProviderInterface>
    browserProviderInterface;

// YES if incognito mode is forced by enterprise policy.
@property(nonatomic, readonly, getter=isIncognitoForced) BOOL incognitoForced;

// YES if the scene is presenting the signin view.
@property(nonatomic, readonly) BOOL isPresentingSigninView;

// YES if the tab grid is the main user interface at the moment.
@property(nonatomic, readonly, getter=isTabGridVisible) BOOL tabGridVisible;

// Handler for the UIWindowSceneDelegate callback with the same selector.
- (void)performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                   completionHandler:
                       (void (^)(BOOL succeeded))completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_H_

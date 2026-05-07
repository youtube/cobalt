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

#ifndef STARBOARD_TVOS_SHARED_STARBOARD_APPLICATION_H_
#define STARBOARD_TVOS_SHARED_STARBOARD_APPLICATION_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "starboard/system.h"

@class SBDDrmManager;
@class SBDEglAdapter;
@class SBDPlayerManager;
@protocol SBDStarboardApplication;

NS_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @memberof SBDStarboardApplication-p
 *  @brief Returns the global @c SBDStarboardApplication instance.
 */
id<SBDStarboardApplication> SBDGetApplication(void);

#ifdef __cplusplus
}  // extern "C"
#endif

@protocol SBDOnScreenKeyboardManagerDelegate

- (void)keyboardBlurred;

- (void)keyboardFocused;

- (void)keyboardTextChanged:(NSString*)text;

@end

@protocol SBDOnScreenKeyboardManager

@property(nonatomic) id<SBDOnScreenKeyboardManagerDelegate>
    keyboardManagerDelegate;

@property(nonatomic, readonly) CGRect boundingRect;

@property(nonatomic, readonly) BOOL isShowing;

- (void)showOnScreenKeyboard:(NSString*)searchText
               keyboardStyle:(UIUserInterfaceStyle)keyboardStyle
               colorOverride:(UIColor*)colorOverride
           completionHandler:(void (^)(void))completionHandler;

- (void)hideOnScreenKeyboard;

- (void)focusOnScreenKeyboard:(void (^)(void))completionHandler;

@end

/**
 *  @brief A Starboard application.
 */
@protocol SBDStarboardApplication

/**
 *  @brief Enables Starboard to manage DRM playback.
 */
@property(nonatomic, readonly) SBDDrmManager* drmManager;

/**
 *  @brief Enables Starboard to manage platform players.
 */
@property(nonatomic, readonly) SBDPlayerManager* playerManager;

// The object responsible for managing (showing, hiding etc) the native
// on-screen keyboard used in the search page.
@property(nonatomic) id<SBDOnScreenKeyboardManager> onScreenKeyboardManager;

// Sets the UIView to which player views will be added to.
- (void)setPlayerContainerView:(UIView*)view;

// Attaches a video player view that will be shown as an underlay of the web
// contents. Does nothing if setPlayerContainerView() has not been called.
- (void)attachPlayerView:(UIView*)subView;

// Suspends the application by forwarding the press events stored by calls to
// `registerMenuPressBegan` and `registerMenuPressEnded` to UIKit.
- (void)suspendApplication;

// Caches the menu press from a `pressesBegan` event for later use when
// suspending the application.
- (void)registerMenuPressBegan:(UIPress*)press
                  pressesEvent:(UIPressesEvent*)pressesEvent;

// Caches the menu press from a `pressesEnded` event for later use when
// suspending the application.
- (void)registerMenuPressEnded:(UIPress*)press
                  pressesEvent:(UIPressesEvent*)pressesEvent;
@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_STARBOARD_APPLICATION_H_

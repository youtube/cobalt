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

#ifndef STARBOARD_TVOS_SHARED_APPLICATION_WINDOW_H_
#define STARBOARD_TVOS_SHARED_APPLICATION_WINDOW_H_

#import <UIKit/UIKit.h>

#import "starboard/tvos/shared/keyboard_input_device.h"
#include "starboard/window.h"

@class SBDApplication;
@class SBDEglSurface;

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief A window with container views for EGL surfaces and players.
 */
@interface SBDApplicationWindow : UIWindow

/**
 *  @brief The name of the window.
 */
@property(nonatomic, readonly, nullable) NSString* name;

/**
 *  @brief Input device to send keyboard events. (This is separate from the
 *      on-screen keyboard used with the search container.)
 */
@property(nonatomic, readonly) SBDKeyboardInputDevice* keyboardInputDevice;

/**
 *  @brief The size of the window.
 */
@property(nonatomic, readonly) SbWindowSize size;

/**
 *  @brief If the window is full screen or not.
 */
@property(nonatomic, readonly) BOOL windowed;

/**
 *  @brief Specify whether UIPress events should generate SbInputEvents.
 */
@property(nonatomic) BOOL processUIPresses;

/**
 *  @brief Attach an EGL surface so it is displayed with this window.
 *  @param surface The EGL surface to be displayed.
 */
- (void)attachSurface:(SBDEglSurface*)surface;

/**
 *  @brief Attach a player's view so it is displayed with this window.
 */
- (void)attachPlayerView:(UIView*)playerView;

/**
 *  @brief Update focus to the requested item.
 */
- (void)setFocus:(id<UIFocusEnvironment>)focus;

/**
 *  @brief Update window size.
 */
- (void)updateWindowSize;

/**
 *  @brief Closes the window.
 */
- (void)close;

- (nullable instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

/**
 *  @brief Initializes a @c SBDWindow with default settings.
 *  @param name The name of the window.
 */
- (instancetype)initWithName:(nullable NSString*)name NS_DESIGNATED_INITIALIZER;

/**
 *  @brief Return the intended background color for the application window.
 *    This is @c YTApplicationBackgroundColor from info.plist.
 */
+ (UIColor*)defaultWindowColor;
@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_APPLICATION_WINDOW_H_

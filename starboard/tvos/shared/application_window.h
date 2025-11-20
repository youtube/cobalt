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

#include "starboard/extension/on_screen_keyboard.h"
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
 *  @brief Indicates the on-screen-keyboard is currently showing.
 */
@property(nonatomic, readonly) BOOL keyboardShowing;

/**
 *  @brief Specify whether UIPress events should generate SbInputEvents.
 */
@property(nonatomic) BOOL processUIPresses;

/**
 *  @brief Returns the current frame or a zero rect if the on-screen keyboard
 *      isn't showing.
 */
- (SbWindowRect)getOnScreenKeyboardFrame;

/**
 *  @brief Gives focus to the already-displayed on-screen keyboard.
 *  @param ticket The ticket corresponding to the promise stored by the on
 *      screen keyboard.
 *  @remarks Safe to call even if the on-screen keyboard isn't already shown (in
 *      which case, this method is a no-op).
 */
- (void)focusOnScreenKeyboardWithTicket:(NSInteger)ticket;

/**
 *  @brief Removes focus from the already-displayed on-screen keyboard.
 *  @param ticket The ticket corresponding to the promise stored by the on
 *      screen keyboard.
 *  @remarks Safe to call even if the on-screen keyboard isn't already shown (in
 *      which case, this method is a no-op).
 */
- (void)blurOnScreenKeyboardWithTicket:(NSInteger)ticket;

/**
 *  @brief Shows the on-screen keyboard.
 *  @param text The existing text of the text field.
 *  @param ticket The ticket corresponding to the promise stored by the on
 *      screen keyboard.
 *  @remarks Safe to call even if the on-screen keyboard is already shown (in
 *      which case, this method is a no-op).
 */
- (void)showOnScreenKeyboardWithText:(NSString*)text ticket:(NSInteger)ticket;

/**
 *  @brief Hides any visible on-screen keyboard.
 *  @param ticket The ticket corresponding to the promise stored by the on
 *      screen keyboard.
 *  @remarks Safe to call even if the on-screen keyboard isn't being
 *      shown.
 */
- (void)hideOnScreenKeyboardWithTicket:(NSInteger)ticket;

/**
 *  @brief When @c keepFocus is @c YES, the on-screen keyboard should not allow
 *      focus to be lost.
 *  @param keepFocus Whether the on-screen keyboard should allow focus to be
 *      lost.
 */
- (void)onScreenKeyboardKeepFocus:(BOOL)keepFocus;

/**
 *  @brief Set on-screen keyboard's background color in RGB colorspace.
 *  @param red, green, blue, alpha are specified as a value from 0.0 to 1.0.
 */
- (void)setOnScreenKeyboardBackgroundColorWithRed:(CGFloat)red
                                            green:(CGFloat)green
                                             blue:(CGFloat)blue
                                            alpha:(CGFloat)alpha;

/**
 *  @brief When @c lightTheme is @c YES, the on-screen keyboard should switch to
 *      light theme.
 *  @param lightTheme Whether the on-screen keyboard should  enable light theme.
 */
- (void)setOnScreenKeyboardLightTheme:(BOOL)lightTheme;

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

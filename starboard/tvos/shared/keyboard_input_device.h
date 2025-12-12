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

#ifndef STARBOARD_TVOS_SHARED_KEYBOARD_INPUT_DEVICE_H_
#define STARBOARD_TVOS_SHARED_KEYBOARD_INPUT_DEVICE_H_

#import "starboard/tvos/shared/input_device.h"

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief Represents a keyboard.
 */
@interface SBDKeyboardInputDevice : SBDInputDevice

/**
 *  @brief Should be called when the user updates search text in the on screen
 *      keyboard.
 */
- (void)onScreenKeyboardTextEntered:(NSString*)text;

/**
 *  @brief Should be called when the user initially presses a key.
 */
- (void)keyPressed:(SbKey)key modifiers:(SbKeyModifiers)modifiers;

/**
 *  @brief Should be called when the user releases a key.
 */
- (void)keyUnpressed:(SbKey)key modifiers:(SbKeyModifiers)modifiers;

/**
 *  @brief Abort any auto key repeat that may be running.
 */
- (void)stopKeyRepeat;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_KEYBOARD_INPUT_DEVICE_H_

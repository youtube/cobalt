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

#ifndef STARBOARD_TVOS_SHARED_GAME_CONTROLLER_INPUT_DEVICE_H_
#define STARBOARD_TVOS_SHARED_GAME_CONTROLLER_INPUT_DEVICE_H_

#import <GameController/GameController.h>

#import "starboard/tvos/shared/input_device.h"

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief Represents a game controller (or other remote control).
 */
@interface SBDGameControllerInputDevice : SBDInputDevice

/**
 *  @brief The @c GCController related to this controller.
 */
@property(nonatomic, readonly) GCController* controller;

/**
 *  @brief Binds the left-thumbstick input to a @c GCControllerDirectionPad.
 *  @param dpad The @c GCControllerDirectionPad instance to interpret as left-
 *      thumbstick analog input.
 */
- (void)bindLeftThumbstick:(GCControllerDirectionPad*)dpad;

/**
 *  @brief Binds the right-thumbstick input to a @c GCControllerDirectionPad.
 *  @param dpad The @c GCControllerDirectionPad instance to interpret as right-
 *      thumbstick analog input.
 */
- (void)bindRightThumbstick:(GCControllerDirectionPad*)dpad;

/**
 *  @brief Binds a key press/unpress to a @c GCControllerButtonInput.
 *  @param button The @c GCControllerButtonInput to bind.
 *  @param key The keycode which is associated with the
 *      @c GCControllerButtonInput.
 */
- (void)bindButton:(GCControllerButtonInput*)button toKey:(SbKey)key;

- (instancetype)init NS_UNAVAILABLE;

/**
 *  @brief The designated initializer.
 *  @param controller The GameKit controller associated with the input device.
 */
- (instancetype)initWithGameController:(GCController*)controller
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_GAME_CONTROLLER_INPUT_DEVICE_H_

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

#ifndef STARBOARD_TVOS_SHARED_INPUT_DEVICE_H_
#define STARBOARD_TVOS_SHARED_INPUT_DEVICE_H_

#import <Foundation/Foundation.h>

#include "starboard/key.h"
#include "starboard/window.h"

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief A base class for input devices.
 */
@interface SBDInputDevice : NSObject

/**
 *  @brief The Starboard identifier for the device.
 *  @remarks Auto-generated at device construction time.
 */
@property(nonatomic, readonly) NSInteger deviceID;

/**
 *  @brief A handle to the window which is focused and accepting inputs.
 */
@property(nonatomic, readonly) SbWindow currentWindowHandle;

/**
 *  @brief Disconnects the device from the application.
 *  @remarks The input device will be retained until manually disconnected via
 *      this message.
 */
- (void)disconnect;

/**
 *  @brief The designated initializer.
 */
- (instancetype)init;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_INPUT_DEVICE_H_

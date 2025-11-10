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

#ifndef STARBOARD_TVOS_SHARED_TRACKPAD_INPUT_DEVICE_H_
#define STARBOARD_TVOS_SHARED_TRACKPAD_INPUT_DEVICE_H_

#import "starboard/tvos/shared/input_device.h"
#import "starboard/tvos/shared/vector.h"

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief Represents a trackpad-based remote like the Siri Remote.
 */
@interface SBDTrackpadInputDevice : SBDInputDevice

/**
 *  @brief Should be called when the user touches their finger down at
 *      a particular location on the trackpad.
 */
- (void)touchDownAtPosition:(SBDVector)position;

/**
 *  @brief Should be called after the user has touched their finger down
 *      and then moved it to a new location.
 */
- (void)moveToPosition:(SBDVector)position;

/**
 *  @brief Should be called after the user has touched the finger down
 *      on the trackpad and then lifted it off.
 */
- (void)touchUpAtPosition:(SBDVector)position;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_TRACKPAD_INPUT_DEVICE_H_

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

#import "starboard/tvos/shared/game_controller_input_device.h"

#import "starboard/tvos/shared/input_events.h"

@implementation SBDGameControllerInputDevice

- (instancetype)initWithGameController:(GCController*)controller {
  self = [super init];
  if (self) {
    _controller = controller;
  }
  return self;
}

- (void)bindLeftThumbstick:(GCControllerDirectionPad*)dpad {
  __weak SBDGameControllerInputDevice* weakself = self;
  dpad.valueChangedHandler =
      ^(GCControllerDirectionPad*, float xValue, float yValue) {
        SBDGameControllerInputDevice* strongself = weakself;
        if (!strongself) {
          return;
        }

        [SBDInputEvents moveEventWithKey:kSbKeyGamepadLeftStick
                                  window:nullptr
                               xPosition:xValue
                               yPosition:yValue
                                deviceID:strongself.deviceID];
      };
}

- (void)bindRightThumbstick:(GCControllerDirectionPad*)dpad {
  __weak SBDGameControllerInputDevice* weakself = self;
  dpad.valueChangedHandler =
      ^(GCControllerDirectionPad*, float xValue, float yValue) {
        SBDGameControllerInputDevice* strongself = weakself;
        if (!strongself) {
          return;
        }

        [SBDInputEvents moveEventWithKey:kSbKeyGamepadRightStick
                                  window:nullptr
                               xPosition:xValue
                               yPosition:yValue
                                deviceID:strongself.deviceID];
      };
}

- (void)bindButton:(GCControllerButtonInput*)button toKey:(SbKey)key {
  __weak SBDGameControllerInputDevice* weakself = self;
  button.pressedChangedHandler =
      ^(GCControllerButtonInput*, float value, BOOL pressed) {
        SBDGameControllerInputDevice* strongself = weakself;
        if (!strongself) {
          return;
        }

        if (pressed) {
          [SBDInputEvents keyPressedEventWithKey:key
                                       modifiers:kSbKeyModifiersNone
                                          window:nil
                                        deviceID:strongself.deviceID];
        } else {
          [SBDInputEvents keyUnpressedEventWithKey:key
                                         modifiers:kSbKeyModifiersNone
                                            window:nil
                                          deviceID:strongself.deviceID];
        }
      };
}

@end

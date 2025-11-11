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

#import "starboard/tvos/shared/keyboard_input_device.h"

#import "starboard/tvos/shared/input_events.h"
#import "starboard/tvos/shared/window_manager.h"

/**
 *  @brief Auto key repeat intervals.
 *      See YouTube 2018 Technical Requirements 9.5:
 *      After a key is held down for 500ms, the Window.keydown event MUST
 *      repeat every 50ms until a user stops holding the key down.
 */
static NSTimeInterval kKeyRepeatStartDelay = 0.5;
static NSTimeInterval kKeyRepeatInterval = 0.05;

/*
 *  @brief Only certain keys are allowed to repeat. This helps avoid
 *      unintended behavior.
 */
static const NSArray<NSNumber*>* repeatedKeys =
    @[ @(kSbKeyUp), @(kSbKeyDown), @(kSbKeyLeft), @(kSbKeyRight) ];

@implementation SBDKeyboardInputDevice {
  /**
   *  @brief Timer to facilitate auto-repeat if a key is held down.
   */
  NSTimer* _keyRepeatTimer;

  /**
   *  @brief The key to auto-repeat.
   */
  SbKey _keyRepeatKey;
}

- (void)onScreenKeyboardTextEntered:(NSString*)text {
  if (!self.currentWindowHandle) {
    return;
  }
  [SBDInputEvents onScreenKeyboardTextUpdated:text
                                       window:self.currentWindowHandle];
}

- (void)keyPressed:(SbKey)key modifiers:(SbKeyModifiers)modifiers {
  // For simplicity, only one key is allowed to repeat at any given time.
  [self stopKeyRepeat];

  [SBDInputEvents keyPressedEventWithKey:key
                               modifiers:modifiers
                                  window:self.currentWindowHandle
                                deviceID:self.deviceID];

  if ([repeatedKeys containsObject:@(key)] &&
      modifiers == kSbKeyModifiersNone) {
    [self repeatKey:key];
  }
}

- (void)keyUnpressed:(SbKey)key modifiers:(SbKeyModifiers)modifiers {
  [self stopKeyRepeat];
  [SBDInputEvents keyUnpressedEventWithKey:key
                                 modifiers:modifiers
                                    window:self.currentWindowHandle
                                  deviceID:self.deviceID];
}

- (void)repeatKey:(SbKey)key {
  _keyRepeatKey = key;
  NSDate* fireDate = [NSDate dateWithTimeIntervalSinceNow:kKeyRepeatStartDelay];
  _keyRepeatTimer = [[NSTimer alloc] initWithFireDate:fireDate
                                             interval:kKeyRepeatInterval
                                               target:self
                                             selector:@selector(onKeyRepeat)
                                             userInfo:nil
                                              repeats:YES];
  [[NSRunLoop currentRunLoop] addTimer:_keyRepeatTimer
                               forMode:NSDefaultRunLoopMode];
}

- (void)stopKeyRepeat {
  [_keyRepeatTimer invalidate];
  _keyRepeatTimer = nil;
}

- (void)onKeyRepeat {
  [SBDInputEvents keyUnpressedEventWithKey:_keyRepeatKey
                                 modifiers:kSbKeyModifiersNone
                                    window:self.currentWindowHandle
                                  deviceID:self.deviceID];

  [SBDInputEvents keyPressedEventWithKey:_keyRepeatKey
                               modifiers:kSbKeyModifiersNone
                                  window:self.currentWindowHandle
                                deviceID:self.deviceID];
}

@end

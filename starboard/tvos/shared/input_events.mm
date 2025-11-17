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

#import "starboard/tvos/shared/input_events.h"

#include <cmath>

#include "starboard/common/time.h"
#include "starboard/input.h"
#include "starboard/tvos/shared/application_darwin.h"
#import "starboard/tvos/shared/defines.h"

using starboard::ApplicationDarwin;

namespace {
void DeleteOnScreenKeyboardInputData(void* ptr) {
  SbInputData* data = static_cast<SbInputData*>(ptr);
  free(const_cast<char*>(data->input_text));
  free(ptr);
}
}  // namespace

@implementation SBDInputEvents

+ (void)onScreenKeyboardTextUpdated:(NSString*)text window:(SbWindow)window {
  NSUInteger stringLength =
      [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
  char* str = static_cast<char*>(calloc(stringLength + 1, sizeof(char)));
  [text getCString:str
         maxLength:stringLength + 1
          encoding:NSUTF8StringEncoding];
  SbInputData* data = static_cast<SbInputData*>(calloc(1, sizeof(SbInputData)));
  data->window = window;
  data->type = kSbInputEventTypeInput;
  data->device_type = kSbInputDeviceTypeOnScreenKeyboard;
  data->input_text = str;
  ApplicationDarwin::InjectEvent(kSbEventTypeInput,
                                 starboard::CurrentMonotonicTime(), data,
                                 &DeleteOnScreenKeyboardInputData);
}

+ (void)keyPressedEventWithKey:(SbKey)key
                     modifiers:(SbKeyModifiers)modifiers
                        window:(SbWindow)window
                      deviceID:(NSInteger)deviceID {
  SbInputData* data = static_cast<SbInputData*>(calloc(1, sizeof(SbInputData)));
  data->window = window;
  data->type = kSbInputEventTypePress;
  data->device_type = kSbInputDeviceTypeGamepad;
  data->device_id = (int)deviceID;
  data->key = key;
  data->character = 0;
  data->key_location = kSbKeyLocationUnspecified;
  data->key_modifiers = modifiers;
  data->pressure = 1;
  ApplicationDarwin::InjectEvent(
      kSbEventTypeInput, starboard::CurrentMonotonicTime(), data, &free);
}

+ (void)keyUnpressedEventWithKey:(SbKey)key
                       modifiers:(SbKeyModifiers)modifiers
                          window:(SbWindow)window
                        deviceID:(NSInteger)deviceID {
  SbInputData* data = static_cast<SbInputData*>(calloc(1, sizeof(SbInputData)));
  data->window = window;
  data->type = kSbInputEventTypeUnpress;
  data->device_type = kSbInputDeviceTypeGamepad;
  data->device_id = (int)deviceID;
  data->key = key;
  data->character = 0;
  data->key_location = kSbKeyLocationUnspecified;
  data->key_modifiers = modifiers;
  data->pressure = 1;
  ApplicationDarwin::InjectEvent(
      kSbEventTypeInput, starboard::CurrentMonotonicTime(), data, &free);
}

+ (void)moveEventWithKey:(SbKey)key
                  window:(SbWindow)window
               xPosition:(float)xPosition
               yPosition:(float)yPosition
                deviceID:(NSInteger)deviceID {
  SbInputData* data = static_cast<SbInputData*>(calloc(1, sizeof(SbInputData)));
  data->type = kSbInputEventTypeMove;
  data->device_type = kSbInputDeviceTypeGamepad;
  data->window = window;
  data->device_id = (int)deviceID;
  data->key = key;
  data->character = 0;
  data->key_location = kSbKeyLocationUnspecified;
  data->key_modifiers = 0;
  data->pressure = NAN;
  data->position = (SbInputVector){static_cast<float>(xPosition),
                                   static_cast<float>(yPosition)};
  ApplicationDarwin::InjectEvent(
      kSbEventTypeInput, starboard::CurrentMonotonicTime(), data, &free);
}

+ (void)touchpadMoveEventWithWindow:(SbWindow)window
                          xPosition:(float)xPosition
                          yPosition:(float)yPosition
                           deviceID:(NSInteger)deviceID {
  SbInputData* data = static_cast<SbInputData*>(calloc(1, sizeof(SbInputData)));
  data->type = kSbInputEventTypeMove;
  data->device_type = kSbInputDeviceTypeTouchPad;
  data->pressure = NAN;
  data->size = {1., 1.};
  data->tilt = {NAN, NAN};
  data->window = window;
  data->device_id = (int)deviceID;
  data->character = 0;
  data->key_location = kSbKeyLocationUnspecified;
  data->key_modifiers = 0;
  data->position = (SbInputVector){static_cast<float>(xPosition),
                                   static_cast<float>(yPosition)};
  ApplicationDarwin::InjectEvent(
      kSbEventTypeInput, starboard::CurrentMonotonicTime(), data, &free);
}

+ (void)touchpadDownEventWithWindow:(SbWindow)window
                          xPosition:(float)xPosition
                          yPosition:(float)yPosition
                           deviceID:(NSInteger)deviceID {
  SbInputData* data = static_cast<SbInputData*>(calloc(1, sizeof(SbInputData)));
  data->type = kSbInputEventTypePress;
  data->device_type = kSbInputDeviceTypeTouchPad;
  data->pressure = NAN;
  data->size = {1., 1.};
  data->tilt = {NAN, NAN};
  data->window = window;
  data->device_id = (int)deviceID;
  data->character = 0;
  data->key_location = kSbKeyLocationUnspecified;
  data->key_modifiers = 0;
  data->position = (SbInputVector){static_cast<float>(xPosition),
                                   static_cast<float>(yPosition)};
  ApplicationDarwin::InjectEvent(
      kSbEventTypeInput, starboard::CurrentMonotonicTime(), data, &free);
}

+ (void)touchpadUpEventWithWindow:(SbWindow)window
                        xPosition:(float)xPosition
                        yPosition:(float)yPosition
                         deviceID:(NSInteger)deviceID {
  SbInputData* data = static_cast<SbInputData*>(calloc(1, sizeof(SbInputData)));
  data->type = kSbInputEventTypeUnpress;
  data->device_type = kSbInputDeviceTypeTouchPad;
  data->pressure = NAN;
  data->size = {1., 1.};
  data->tilt = {NAN, NAN};
  data->window = window;
  data->device_id = (int)deviceID;
  data->character = 0;
  data->key_location = kSbKeyLocationUnspecified;
  data->key_modifiers = 0;
  data->position = (SbInputVector){static_cast<float>(xPosition),
                                   static_cast<float>(yPosition)};
  ApplicationDarwin::InjectEvent(
      kSbEventTypeInput, starboard::CurrentMonotonicTime(), data, &free);
}

@end

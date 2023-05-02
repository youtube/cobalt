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

#ifndef STARBOARD_ANDROID_SHARED_INPUT_EVENTS_GENERATOR_H_
#define STARBOARD_ANDROID_SHARED_INPUT_EVENTS_GENERATOR_H_

#include <map>
#include <memory>
#include <vector>

#include "game-activity/GameActivity.h"

#include "starboard/input.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/window.h"

namespace starboard {
namespace android {
namespace shared {

class InputEventsGenerator {
 public:
  typedef ::starboard::shared::starboard::Application::Event Event;
  typedef std::vector<std::unique_ptr<Event> > Events;

  explicit InputEventsGenerator(SbWindow window);
  virtual ~InputEventsGenerator();

  // Translates an Android input event into a series of Starboard application
  // events. The caller owns the new events and must delete them when done with
  // them.
  bool CreateInputEventsFromGameActivityEvent(
      GameActivityMotionEvent* android_event,
      Events* events);
  bool CreateInputEventsFromGameActivityEvent(
      GameActivityKeyEvent* android_event,
      Events* events);

  // Create press/unpress events from SbKey
  // (for use with CobaltA11yHelper injection)
  void CreateInputEventsFromSbKey(SbKey key, Events* events);

 private:
  enum FlatAxis {
    kLeftX,
    kLeftY,
    kRightX,
    kRightY,
    kNumAxes,
  };

  bool ProcessPointerEvent(GameActivityMotionEvent* android_event,
                           Events* events);
  void ProcessJoyStickEvent(FlatAxis axis,
                            int32_t motion_axis,
                            GameActivityMotionEvent* android_event,
                            Events* events);
  void UpdateDeviceFlatMapIfNecessary(GameActivityMotionEvent* android_event,
                                      jobject input_device);
  void UpdateDeviceFlatMapIfNecessary(GameActivityKeyEvent* android_event);

  void ProcessFallbackDPadEvent(SbInputEventType type,
                                SbKey key,
                                GameActivityKeyEvent* android_event,
                                Events* events);
  void UpdateHatValuesAndPossiblySynthesizeKeyEvents(
      GameActivityMotionEvent* android_event,
      Events* events);

  SbWindow window_;

  // Map the device id with joystick flat position.
  // Cache the flat area of joystick to avoid calling jni functions frequently.
  std::map<int32_t, std::vector<float> > device_flat_;

  // The current X/Y analog values of the "hat" (dpad on the game controller).
  float hat_value_[2];

  // The last known value of the left thumbstick, used to track when we should
  // generate key unpressed events for it.  We store values for horizontal and
  // vertical directions independently.
  SbKey left_thumbstick_key_pressed_[2];
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_INPUT_EVENTS_GENERATOR_H_

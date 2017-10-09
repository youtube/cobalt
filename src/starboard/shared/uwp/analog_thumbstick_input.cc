/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "starboard/shared/uwp/analog_thumbstick_input.h"

#include <Windows.Gaming.Input.h>
#include <algorithm>

#include "starboard/log.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/types.h"

#pragma comment(lib, "xinput9_1_0.lib")

using Windows::Foundation::Collections::IVectorView;
using Windows::Gaming::Input::Gamepad;
using Windows::Gaming::Input::GamepadReading;
using Windows::Gaming::Input::RawGameController;

namespace starboard {
namespace shared {
namespace uwp {
namespace {

const int kMaxPlayerCounter = 4;
const float kDeadZoneThreshold = .24f;

float ApplyLinearDeadZone(float value, float maxValue, float deadZoneSize) {
  if (value < -deadZoneSize) {
    // Increase negative values to remove the deadzone discontinuity.
    value += deadZoneSize;
  } else if (value > deadZoneSize) {
    // Decrease positive values to remove the deadzone discontinuity.
    value -= deadZoneSize;
  } else {
    // Values inside the deadzone come out zero.
    return 0;
  }

  // Scale into 0-1 range.
  float scaledValue = value / (maxValue - deadZoneSize);
  return std::max(-1.f, std::min(scaledValue, 1.f));
}

void ApplyStickDeadZone(float x,
                        float y,
                        float max_value,
                        float dead_zone_size,
                        float* result_x,
                        float* result_y) {
  *result_x = ApplyLinearDeadZone(x, max_value, dead_zone_size);
  *result_y = ApplyLinearDeadZone(y, max_value, dead_zone_size);
}

ThumbSticks ReadThumbStick(Gamepad ^ controller) {
  ThumbSticks output;
  GamepadReading reading = controller->GetCurrentReading();

  ApplyStickDeadZone(static_cast<float>(reading.LeftThumbstickX),
                     static_cast<float>(reading.LeftThumbstickY), 1.f,
                     kDeadZoneThreshold, &output.left_x, &output.left_y);

  ApplyStickDeadZone(static_cast<float>(reading.RightThumbstickX),
                     static_cast<float>(reading.RightThumbstickY), 1.f,
                     kDeadZoneThreshold, &output.right_x, &output.right_y);
  return output;
}
}  // namespace

void GetGamepadThumbSticks(std::vector<ThumbSticks>* destination) {
  destination->erase(destination->begin(), destination->end());

  // This profiled to an average time of 33us to execute.
  IVectorView<Gamepad ^> ^ gamepads = Gamepad::Gamepads;

  // This profiled to take on average 9us of time to read controller state.
  const uint32_t n = gamepads->Size;
  for (uint32_t i = 0; i < n; ++i) {
    Gamepad ^ gamepad = gamepads->GetAt(i);
    ThumbSticks thumb_stick = ReadThumbStick(gamepad);
    destination->push_back(thumb_stick);
  }
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

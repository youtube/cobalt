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

#include "cobalt/input/input_poller_impl.h"

namespace cobalt {
namespace input {

InputPollerImpl::InputPollerImpl() : InputPoller() {
  // Initialize the joystick key mapping.
  key_offset_map_[kSbKeyGamepadLeftStickUp] = 0.0f;
  key_offset_map_[kSbKeyGamepadLeftStickDown] = 0.0f;
  key_offset_map_[kSbKeyGamepadLeftStickLeft] = 0.0f;
  key_offset_map_[kSbKeyGamepadLeftStickRight] = 0.0f;
  key_offset_map_[kSbKeyGamepadRightStickUp] = 0.0f;
  key_offset_map_[kSbKeyGamepadRightStickDown] = 0.0f;
  key_offset_map_[kSbKeyGamepadRightStickLeft] = 0.0f;
  key_offset_map_[kSbKeyGamepadRightStickRight] = 0.0f;
}

bool InputPollerImpl::IsPressed(SbKey keycode) {
  starboard::ScopedLock lock(input_mutex_);

  return pressed_keys_.find(keycode) != pressed_keys_.end();
}

float InputPollerImpl::AnalogInput(SbKey analog_input_id) {
  starboard::ScopedLock lock(input_mutex_);

  KeyOffsetMap::const_iterator found = key_offset_map_.find(analog_input_id);
  if (found == key_offset_map_.end()) {
    return 0.0f;
  } else {
    return found->second;
  }
}

void InputPollerImpl::UpdateInputEvent(
    const system_window::InputEvent* input_event) {
  starboard::ScopedLock lock(input_mutex_);

  switch (input_event->type()) {
    case system_window::InputEvent::kKeyDown:
      pressed_keys_.insert(input_event->key_code());
      break;
    case system_window::InputEvent::kKeyUp:
      pressed_keys_.erase(input_event->key_code());
      break;
    case system_window::InputEvent::kKeyMove: {
      switch (input_event->key_code()) {
        case kSbKeyGamepadLeftStickUp: {
          key_offset_map_[kSbKeyGamepadLeftStickUp] =
              -input_event->position().y();
          key_offset_map_[kSbKeyGamepadLeftStickDown] =
              input_event->position().y();
          break;
        }
        case kSbKeyGamepadLeftStickLeft: {
          key_offset_map_[kSbKeyGamepadLeftStickLeft] =
              -input_event->position().x();
          key_offset_map_[kSbKeyGamepadLeftStickRight] =
              input_event->position().x();
          break;
        }
        case kSbKeyGamepadRightStickUp: {
          key_offset_map_[kSbKeyGamepadRightStickUp] =
              -input_event->position().y();
          key_offset_map_[kSbKeyGamepadRightStickDown] =
              input_event->position().y();
          break;
        }
        case kSbKeyGamepadRightStickLeft: {
          key_offset_map_[kSbKeyGamepadRightStickLeft] =
              -input_event->position().x();
          key_offset_map_[kSbKeyGamepadRightStickRight] =
              input_event->position().x();
          break;
        }
        default:
          NOTREACHED();
      }
    } break;
#if SB_HAS(ON_SCREEN_KEYBOARD)
    case system_window::InputEvent::kInput:
      // Input events (dom::InputEvent) are ignored here.
      break;
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
    case system_window::InputEvent::kPointerDown:
    case system_window::InputEvent::kPointerUp:
    case system_window::InputEvent::kPointerMove:
    case system_window::InputEvent::kTouchpadDown:
    case system_window::InputEvent::kTouchpadUp:
    case system_window::InputEvent::kTouchpadMove:
    case system_window::InputEvent::kWheel:
      // Pointer and Wheel events are ignored here.
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace input
}  // namespace cobalt

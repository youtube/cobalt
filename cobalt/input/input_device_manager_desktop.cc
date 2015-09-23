/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/input/input_device_manager_desktop.h"

#include "cobalt/base/polymorphic_downcast.h"

namespace cobalt {
namespace input {

InputDeviceManagerDesktop::InputDeviceManagerDesktop(
    const KeyboardEventCallback& callback,
    system_window::SystemWindow* system_window)
    :  // We are in the desktop implementation of this class (Windows/Linux),
      // so we can assume that system_window is actually a pointer to an object
      // of the SystemWindowDesktop subclass.
      system_window_(
          base::polymorphic_downcast<system_window::SystemWindowDesktop*>(
              system_window)),
      keyboard_event_callback_(
          base::Bind(&InputDeviceManagerDesktop::HandleKeyboardEvent,
                     base::Unretained(this))),
      keypress_generator_filter_(callback),
      modifier_key_filter_(&keypress_generator_filter_) {
  // Add this object's keyboard event callback to the system window.
  system_window_->AddKeyboardEventCallback(keyboard_event_callback_);
}

InputDeviceManagerDesktop::~InputDeviceManagerDesktop() {
  // If we have an associated system window, remove our callback from it.
  if (system_window_) {
    system_window_->RemoveKeyboardEventCallback(keyboard_event_callback_);
  }
}

void InputDeviceManagerDesktop::HandleKeyboardEvent(
    const scoped_refptr<dom::KeyboardEvent>& keyboard_event) {
  modifier_key_filter_.HandleKeyboardEvent(keyboard_event);
}

}  // namespace input
}  // namespace cobalt

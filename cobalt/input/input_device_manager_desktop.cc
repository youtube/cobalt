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

#include <string>

#include "cobalt/system_window/keyboard_event.h"

namespace cobalt {
namespace input {

InputDeviceManagerDesktop::InputDeviceManagerDesktop(
    const KeyboardEventCallback& callback,
    system_window::SystemWindow* system_window)
    : system_window_(system_window),
      keyboard_event_callback_(
          base::Bind(&InputDeviceManagerDesktop::HandleKeyboardEvent,
                     base::Unretained(this))),
      keypress_generator_filter_(callback) {
  if (system_window_) {
    // Add this object's keyboard event callback to the system window.
    system_window_->event_dispatcher()->AddEventCallback(
        system_window::KeyboardEvent::TypeId(), keyboard_event_callback_);
  }
}

InputDeviceManagerDesktop::~InputDeviceManagerDesktop() {
  // If we have an associated system window, remove our callback from it.
  if (system_window_) {
    system_window_->event_dispatcher()->RemoveEventCallback(
        system_window::KeyboardEvent::TypeId(), keyboard_event_callback_);
  }
}

void InputDeviceManagerDesktop::HandleKeyboardEvent(const base::Event* event) {
  // The user has pressed a key on the keyboard.
  const system_window::KeyboardEvent* key_event =
      base::polymorphic_downcast<const system_window::KeyboardEvent*>(event);
  const int key_code = key_event->key_code();
  const uint32 modifiers = key_event->modifiers();

  dom::KeyboardEvent::KeyLocationCode location =
      dom::KeyboardEvent::KeyCodeToKeyLocation(key_code);
  DCHECK(key_event->type() == system_window::KeyboardEvent::kKeyDown ||
         key_event->type() == system_window::KeyboardEvent::kKeyUp);

  dom::KeyboardEvent::Type key_event_type =
      key_event->type() == system_window::KeyboardEvent::kKeyDown
          ? dom::KeyboardEvent::kTypeKeyDown
          : dom::KeyboardEvent::kTypeKeyUp;
  dom::KeyboardEvent::Data keyboard_event(key_event_type, location, modifiers,
                                          key_code, key_code,
                                          key_event->is_repeat());

  keypress_generator_filter_.HandleKeyboardEvent(keyboard_event);
}

}  // namespace input
}  // namespace cobalt

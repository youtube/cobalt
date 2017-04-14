// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/input/input_device_manager_desktop.h"

#include <string>

#include "cobalt/input/create_default_camera_3d.h"
#include "cobalt/input/input_poller_impl.h"
#include "cobalt/system_window/input_event.h"

namespace cobalt {
namespace input {

InputDeviceManagerDesktop::InputDeviceManagerDesktop(
    const KeyboardEventCallback& callback,
    system_window::SystemWindow* system_window)
    : system_window_(system_window),
      keyboard_event_callback_(
          base::Bind(&InputDeviceManagerDesktop::HandleInputEvent,
                     base::Unretained(this))),
      keypress_generator_filter_(callback) {
  input_poller_ = new InputPollerImpl();
  DCHECK(system_window_);
  camera_3d_ =
      CreatedDefaultCamera3D(system_window->GetSbWindow(), input_poller_);

  if (system_window_) {
    // Add this object's keyboard event callback to the system window.
    system_window_->event_dispatcher()->AddEventCallback(
        system_window::InputEvent::TypeId(), keyboard_event_callback_);
  }
}

InputDeviceManagerDesktop::~InputDeviceManagerDesktop() {
  // If we have an associated system window, remove our callback from it.
  if (system_window_) {
    system_window_->event_dispatcher()->RemoveEventCallback(
        system_window::InputEvent::TypeId(), keyboard_event_callback_);
  }
}

void InputDeviceManagerDesktop::HandleInputEvent(const base::Event* event) {
  // The user has pressed a key on the keyboard.
  const system_window::InputEvent* input_event =
      base::polymorphic_downcast<const system_window::InputEvent*>(event);
  const int key_code = input_event->key_code();
  const uint32 modifiers = input_event->modifiers();

  if (input_event->type() == system_window::InputEvent::kKeyDown ||
      input_event->type() == system_window::InputEvent::kKeyUp) {
    dom::KeyboardEvent::KeyLocationCode location =
        dom::KeyboardEvent::KeyCodeToKeyLocation(key_code);

    dom::KeyboardEvent::Type key_event_type =
        input_event->type() == system_window::InputEvent::kKeyDown
            ? dom::KeyboardEvent::kTypeKeyDown
            : dom::KeyboardEvent::kTypeKeyUp;
    dom::KeyboardEvent::Data keyboard_event(key_event_type, location, modifiers,
                                            key_code, key_code,
                                            input_event->is_repeat());

    keypress_generator_filter_.HandleKeyboardEvent(keyboard_event);
  }

  InputPollerImpl* input_poller_impl =
      static_cast<InputPollerImpl*>(input_poller_.get());
  input_poller_impl->UpdateInputEvent(input_event);
}

}  // namespace input
}  // namespace cobalt

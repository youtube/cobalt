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

#ifndef COBALT_INPUT_INPUT_DEVICE_MANAGER_H_
#define COBALT_INPUT_INPUT_DEVICE_MANAGER_H_

#include "cobalt/dom/input_event_init.h"
#include "cobalt/dom/pointer_event_init.h"
#include "cobalt/dom/wheel_event_init.h"
#include "cobalt/input/camera_3d.h"
#include "cobalt/input/input_poller.h"
#include "cobalt/input/key_event_handler.h"

namespace cobalt {

// Forward-declaring SystemWindow here, as some implementations (e.g. consoles)
// will never need to know about the class.
namespace system_window {
class SystemWindow;
}

namespace input {

typedef base::Callback<void(base::Token type, const dom::PointerEventInit&)>
    PointerEventCallback;

typedef base::Callback<void(base::Token type, const dom::WheelEventInit&)>
    WheelEventCallback;

#if SB_HAS(ON_SCREEN_KEYBOARD)
typedef base::Callback<void(base::Token type, const dom::InputEventInit&)>
    InputEventCallback;
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

// InputDeviceManager listens to events from platform-specific input devices
// and maps them to platform-independent keyboard key events.
class InputDeviceManager {
 public:
  // Creates an instance using a SystemWindow parameter.
  // This allows us to hook up keyboard events on desktop systems.
  static scoped_ptr<InputDeviceManager> CreateFromWindow(
      const KeyboardEventCallback& keyboard_event_callback,
      const PointerEventCallback& pointer_event_callback,
      const WheelEventCallback& wheel_event_callback,
#if SB_HAS(ON_SCREEN_KEYBOARD)
      const InputEventCallback& input_event_callback,
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
      system_window::SystemWindow* system_window);

  virtual ~InputDeviceManager() {}

  scoped_refptr<InputPoller> input_poller() { return input_poller_; }
  scoped_refptr<Camera3D> camera_3d() { return camera_3d_; }

 protected:
  InputDeviceManager() {}

  // Used for polling the input outside of InputDeviceManager.
  scoped_refptr<InputPoller> input_poller_;

  // Used for holding the 3D camera state.
  scoped_refptr<Camera3D> camera_3d_;
};

}  // namespace input
}  // namespace cobalt

#endif  // COBALT_INPUT_INPUT_DEVICE_MANAGER_H_

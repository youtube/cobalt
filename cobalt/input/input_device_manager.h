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

#ifndef COBALT_INPUT_INPUT_DEVICE_MANAGER_H_
#define COBALT_INPUT_INPUT_DEVICE_MANAGER_H_

#include "cobalt/input/key_event_handler.h"

namespace cobalt {

// Forward-declaring SystemWindow here, as some implementations (e.g. consoles)
// will never need to know about the class.
namespace system_window {
class SystemWindow;
}

namespace input {

// InputDeviceManager listens to events from platform-specific input devices
// and maps them to platform-independent keyboard key events.
class InputDeviceManager {
 public:
  // Creates an instance using a SystemWindow parameter.
  // This allows us to hook up keyboard events on desktop systems.
  static scoped_ptr<InputDeviceManager> CreateFromWindow(
      const KeyboardEventCallback& callback,
      system_window::SystemWindow* system_window);

  virtual ~InputDeviceManager() {}

 protected:
  InputDeviceManager() {}
};

}  // namespace input
}  // namespace cobalt

#endif  // COBALT_INPUT_INPUT_DEVICE_MANAGER_H_

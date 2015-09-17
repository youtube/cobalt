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

#ifndef INPUT_INPUT_DEVICE_MANAGER_DESKTOP_H_
#define INPUT_INPUT_DEVICE_MANAGER_DESKTOP_H_

#include "cobalt/input/input_device_manager.h"
#include "cobalt/system_window/system_window_desktop.h"

namespace cobalt {
namespace input {

class InputDeviceManagerDesktop : public InputDeviceManager {
 public:
  explicit InputDeviceManagerDesktop(const KeyboardEventCallback& callback);

  InputDeviceManagerDesktop(const KeyboardEventCallback& callback,
                            system_window::SystemWindow* system_window);

  ~InputDeviceManagerDesktop() OVERRIDE;

 private:
  system_window::SystemWindowDesktop* system_window_;
};

}  // namespace input
}  // namespace cobalt

#endif  // INPUT_INPUT_DEVICE_MANAGER_DESKTOP_H_

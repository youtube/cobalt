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

#ifndef INPUT_INPUT_DEVICE_MANAGER_FUZZER_H_
#define INPUT_INPUT_DEVICE_MANAGER_FUZZER_H_

#include <vector>

#include "base/timer.h"
#include "cobalt/input/input_device_manager.h"

namespace cobalt {
namespace input {

// This input fuzzer will continously generate random input events.  It is a
// debug tool that can be used in place of a platform-specific
// InputDeviceManager that generates input events from an external controller
// device.
class InputDeviceManagerFuzzer : public InputDeviceManager {
 public:
  explicit InputDeviceManagerFuzzer(
      KeyboardEventCallback keyboard_event_callback);
  ~InputDeviceManagerFuzzer() OVERRIDE {}

 private:
  void OnNextEvent();

  KeyboardEventCallback keyboard_event_callback_;

  // The list of keyboard events we may sample from and produce.
  std::vector<scoped_refptr<dom::KeyboardEvent> > sample_events_;
  base::Timer next_event_timer_;
};

}  // namespace input
}  // namespace cobalt

#endif  // INPUT_INPUT_DEVICE_MANAGER_FUZZER_H_

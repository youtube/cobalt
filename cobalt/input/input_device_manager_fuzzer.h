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

#ifndef COBALT_INPUT_INPUT_DEVICE_MANAGER_FUZZER_H_
#define COBALT_INPUT_INPUT_DEVICE_MANAGER_FUZZER_H_

#include <vector>

#include "base/threading/thread.h"
#include "base/time.h"
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
  ~InputDeviceManagerFuzzer() override {}

 private:
  class KeyInfo {
   public:
    KeyInfo(const int* key_codes, size_t num_of_key_codes,
            base::TimeDelta delay);
    KeyInfo(const int* key_codes, size_t num_of_key_codes,
            base::TimeDelta minimum_delay, base::TimeDelta maximum_delay);
    KeyInfo(int key_code, base::TimeDelta delay);
    KeyInfo(int key_code, base::TimeDelta minimum_delay,
            base::TimeDelta maximum_delay);

    int GetRandomKeyCode() const;
    base::TimeDelta GetRandomDelay() const;

   private:
    std::vector<int> key_codes_;
    base::TimeDelta minimum_delay_;
    base::TimeDelta maximum_delay_;
  };

  void OnNextEvent();

  KeyboardEventCallback keyboard_event_callback_;
  size_t next_key_index_;
  std::vector<KeyInfo> key_infos_;

  base::Thread thread_;
};

}  // namespace input
}  // namespace cobalt

#endif  // COBALT_INPUT_INPUT_DEVICE_MANAGER_FUZZER_H_

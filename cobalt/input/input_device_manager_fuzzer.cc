// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/input/input_device_manager_fuzzer.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/dom/keycode.h"

namespace cobalt {
namespace input {

namespace {

// Initialize the set of key events we are able to produce.
const int kKeyCodes[] = {dom::keycode::kUp,     dom::keycode::kDown,
                         dom::keycode::kLeft,   dom::keycode::kRight,
                         dom::keycode::kReturn, dom::keycode::kEscape};

}  // namespace

InputDeviceManagerFuzzer::KeyInfo::KeyInfo(const int* key_codes,
                                           size_t num_of_key_codes,
                                           base::TimeDelta delay) {
  DCHECK_GT(num_of_key_codes, 0);
  key_codes_.assign(key_codes, key_codes + num_of_key_codes);
  minimum_delay_ = delay;
  maximum_delay_ = delay;
}

InputDeviceManagerFuzzer::KeyInfo::KeyInfo(const int* key_codes,
                                           size_t num_of_key_codes,
                                           base::TimeDelta minimum_delay,
                                           base::TimeDelta maximum_delay) {
  DCHECK_GT(num_of_key_codes, 0);
  DCHECK(minimum_delay <= maximum_delay);
  key_codes_.assign(key_codes, key_codes + num_of_key_codes);
  minimum_delay_ = minimum_delay;
  maximum_delay_ = maximum_delay;
}

InputDeviceManagerFuzzer::KeyInfo::KeyInfo(int key_code,
                                           base::TimeDelta delay) {
  key_codes_.push_back(key_code);
  minimum_delay_ = delay;
  maximum_delay_ = delay;
}

InputDeviceManagerFuzzer::KeyInfo::KeyInfo(int key_code,
                                           base::TimeDelta minimum_delay,
                                           base::TimeDelta maximum_delay) {
  DCHECK(minimum_delay <= maximum_delay);
  key_codes_.push_back(key_code);
  minimum_delay_ = minimum_delay;
  maximum_delay_ = maximum_delay;
}

int InputDeviceManagerFuzzer::KeyInfo::GetRandomKeyCode() const {
  int index = static_cast<int>(base::RandGenerator(key_codes_.size()));
  return key_codes_[index];
}

base::TimeDelta InputDeviceManagerFuzzer::KeyInfo::GetRandomDelay() const {
  if (minimum_delay_ == maximum_delay_) {
    return minimum_delay_;
  }
  int64 diff_in_microseconds =
      (maximum_delay_ - minimum_delay_).InMicroseconds();
  diff_in_microseconds = static_cast<int64>(
      base::RandGenerator(static_cast<uint64>(diff_in_microseconds)));
  return minimum_delay_ +
         base::TimeDelta::FromMicroseconds(diff_in_microseconds);
}

InputDeviceManagerFuzzer::InputDeviceManagerFuzzer(
    KeyboardEventCallback keyboard_event_callback)
    : keyboard_event_callback_(keyboard_event_callback),
      next_key_index_(0),
      thread_("InputDevMgrFuzz") {
  key_infos_.push_back(KeyInfo(kKeyCodes, arraysize(kKeyCodes),
                               base::TimeDelta::FromMilliseconds(400)));
  // Modify the key_infos_ to use different input patterns.  For example, the
  // following pattern can be used to test play and stop of a video repeatedly.
  //   key_infos_.push_back(KeyInfo(keycode::kReturn,
  //                        base::TimeDelta::FromSeconds(1)));
  //   key_infos_.push_back(KeyInfo(keycode::kEscape,
  //                        base::TimeDelta::FromSeconds(1)));

  // Schedule task to send the first key event.  Add an explicit delay to avoid
  // possible conflicts with debug console.
  thread_.Start();
  thread_.message_loop()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&InputDeviceManagerFuzzer::OnNextEvent,
                 base::Unretained(this)),
      base::TimeDelta::FromSeconds(5));
}

void InputDeviceManagerFuzzer::OnNextEvent() {
  DCHECK_LT(next_key_index_, key_infos_.size());
  int key_code = key_infos_[next_key_index_].GetRandomKeyCode();

  dom::KeyboardEventInit event_init;
  event_init.set_key_code(key_code);

  keyboard_event_callback_.Run(base::Tokens::keydown(), event_init);
  keyboard_event_callback_.Run(base::Tokens::keyup(), event_init);

  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&InputDeviceManagerFuzzer::OnNextEvent,
                 base::Unretained(this)),
      key_infos_[next_key_index_].GetRandomDelay());
  next_key_index_ = (next_key_index_ + 1) % key_infos_.size();
}

}  // namespace input
}  // namespace cobalt

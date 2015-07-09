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

#include "cobalt/input/input_device_manager_fuzzer.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "cobalt/input/keyboard_code.h"

namespace cobalt {
namespace input {

InputDeviceManagerFuzzer::InputDeviceManagerFuzzer(
    KeyboardEventCallback keyboard_event_callback)
    : InputDeviceManager(keyboard_event_callback),
      next_event_timer_(true, true) {
  // Initialize the set of key events we are able to produce.
  sample_events_.push_back(new dom::KeyboardEvent(
      dom::KeyboardEvent::kDomKeyLocationStandard, dom::KeyboardEvent::kKeyDown,
      dom::KeyboardEvent::kNoModifier, kUp, 0, false));
  sample_events_.push_back(new dom::KeyboardEvent(
      dom::KeyboardEvent::kDomKeyLocationStandard, dom::KeyboardEvent::kKeyDown,
      dom::KeyboardEvent::kNoModifier, kDown, 0, false));
  sample_events_.push_back(new dom::KeyboardEvent(
      dom::KeyboardEvent::kDomKeyLocationStandard, dom::KeyboardEvent::kKeyDown,
      dom::KeyboardEvent::kNoModifier, kLeft, 0, false));
  sample_events_.push_back(new dom::KeyboardEvent(
      dom::KeyboardEvent::kDomKeyLocationStandard, dom::KeyboardEvent::kKeyDown,
      dom::KeyboardEvent::kNoModifier, kRight, 0, false));
  sample_events_.push_back(new dom::KeyboardEvent(
      dom::KeyboardEvent::kDomKeyLocationStandard, dom::KeyboardEvent::kKeyDown,
      dom::KeyboardEvent::kNoModifier, kReturn, 0, false));
  sample_events_.push_back(new dom::KeyboardEvent(
      dom::KeyboardEvent::kDomKeyLocationStandard, dom::KeyboardEvent::kKeyDown,
      dom::KeyboardEvent::kNoModifier, kEscape, 0, false));

  next_event_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(100),
                          base::Bind(&InputDeviceManagerFuzzer::OnNextEvent,
                                     base::Unretained(this)));
}

void InputDeviceManagerFuzzer::OnNextEvent() {
  // Uniformly sample from the list of events we can generate and then fire
  // an event based on that.
  int rand_key_event_index =
      static_cast<int>(base::RandGenerator(sample_events_.size()));
  keyboard_event_callback_.Run(sample_events_[rand_key_event_index]);
}

}  // namespace input
}  // namespace cobalt

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

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/rand_util.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/keycode.h"

namespace cobalt {
namespace input {

namespace {

// Initialize the set of key events we are able to produce.
const int kKeyCodes[] = {dom::keycode::kUp,     dom::keycode::kDown,
                         dom::keycode::kLeft,   dom::keycode::kRight,
                         dom::keycode::kReturn, dom::keycode::kEscape};

}  // namespace

InputDeviceManagerFuzzer::InputDeviceManagerFuzzer(
    KeyboardEventCallback keyboard_event_callback)
    : keyboard_event_callback_(keyboard_event_callback),
      next_event_timer_(true, true) {
  next_event_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(400),
                          base::Bind(&InputDeviceManagerFuzzer::OnNextEvent,
                                     base::Unretained(this)));
}

void InputDeviceManagerFuzzer::OnNextEvent() {
  // Uniformly sample from the list of events we can generate and then fire
  // an event based on that.
  int rand_key_event_index =
      static_cast<int>(base::RandGenerator(arraysize(kKeyCodes)));
  int key_code = kKeyCodes[rand_key_event_index];

  scoped_refptr<dom::KeyboardEvent> key_down_event(new dom::KeyboardEvent(
      base::Tokens::keydown(), dom::KeyboardEvent::kDomKeyLocationStandard,
      dom::KeyboardEvent::kNoModifier, key_code, 0, false));

  keyboard_event_callback_.Run(key_down_event);

  scoped_refptr<dom::KeyboardEvent> key_up_event(new dom::KeyboardEvent(
      base::Tokens::keyup(), dom::KeyboardEvent::kDomKeyLocationStandard,
      dom::KeyboardEvent::kNoModifier, key_code, 0, false));

  keyboard_event_callback_.Run(key_up_event);
}

}  // namespace input
}  // namespace cobalt

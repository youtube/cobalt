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

#include "cobalt/input/keypress_generator_filter.h"

#include "cobalt/base/tokens.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/dom/keycode.h"

namespace cobalt {
namespace input {

KeypressGeneratorFilter::KeypressGeneratorFilter(
    const KeyboardEventCallback& callback)
    : KeyEventHandler(callback) {}

KeypressGeneratorFilter::KeypressGeneratorFilter(KeyEventHandler* filter)
    : KeyEventHandler(filter) {}

void KeypressGeneratorFilter::HandleKeyboardEvent(
    base::Token type, const dom::KeyboardEventInit& keyboard_event) {
  // Handle the original event
  DispatchKeyboardEvent(type, keyboard_event);

  // If the event was a keydown, we may also generate a keypress event.
  ConditionallyGenerateKeypressEvent(type, keyboard_event);
}

bool KeypressGeneratorFilter::ConditionallyGenerateKeypressEvent(
    base::Token type, const dom::KeyboardEventInit& orig_event) {
  // Ignore everything but keydown events.
  if (type != base::Tokens::keydown()) {
    return false;
  }

  // Don't generate a keypress event if one of the modifier keys other than
  // Shift is held down.
  if (orig_event.alt_key() || orig_event.ctrl_key() || orig_event.meta_key()) {
    return false;
  }

  // Get the char_code corresponding to the key_code of the event.
  // Only generate the keypress event if there is a valid char_code.
  int char_code = dom::KeyboardEvent::ComputeCharCode(orig_event.key_code(),
                                                      orig_event.shift_key());

  if (char_code > 0) {
    dom::KeyboardEventInit event(orig_event);
    event.set_char_code(char_code);
    DispatchKeyboardEvent(base::Tokens::keypress(), event);
    return true;
  }

  return false;
}

}  // namespace input
}  // namespace cobalt

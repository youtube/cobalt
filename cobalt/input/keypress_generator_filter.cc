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

#include "cobalt/input/keypress_generator_filter.h"

#include "cobalt/dom/event_names.h"
#include "cobalt/dom/keyboard_code.h"

namespace cobalt {
namespace input {

KeypressGeneratorFilter::KeypressGeneratorFilter(
    const KeyboardEventCallback& callback)
    : KeyEventHandler(callback) {}

KeypressGeneratorFilter::KeypressGeneratorFilter(KeyEventHandler* filter)
    : KeyEventHandler(filter) {}

void KeypressGeneratorFilter::HandleKeyboardEvent(
    const scoped_refptr<dom::KeyboardEvent>& keyboard_event) {
  // Handle the original event
  DispatchKeyboardEvent(keyboard_event);

  // If the event was a keydown, we may also generate a keypress event.
  ConditionallyGenerateKeypressEvent(keyboard_event);
}

bool KeypressGeneratorFilter::ConditionallyGenerateKeypressEvent(
    const scoped_refptr<dom::KeyboardEvent>& orig_event) {
  // Ignore everything but keydown events.
  if (orig_event->type() != dom::EventNames::GetInstance()->keydown()) {
    return false;
  }

  // Don't generate a keypress event if one of the modifier keys other than
  // Shift is held down.
  if (orig_event->alt_key() || orig_event->ctrl_key() ||
      orig_event->meta_key()) {
    return false;
  }

  // Get the char_code corresponding to the key_code of the event.
  // Only generate the keypress event if there is a valid char_code.
  int key_code = orig_event->key_code();
  int char_code = 0;

  if (key_code == dom::kBack || key_code == dom::kReturn ||
      key_code == dom::kSpace) {
    // Treat these keys as special (mostly for debug console).
    char_code = key_code;
  } else {
    char_code = orig_event->ComputeCharCode();
  }

  if (char_code > 0) {
    scoped_refptr<dom::KeyboardEvent> keypress_event(new dom::KeyboardEvent(
        dom::EventNames::GetInstance()->keypress(),
        dom::KeyboardEvent::kDomKeyLocationStandard, orig_event->modifiers(),
        key_code, char_code, orig_event->repeat()));
    DispatchKeyboardEvent(keypress_event);
    return true;
  }

  return false;
}

}  // namespace input
}  // namespace cobalt

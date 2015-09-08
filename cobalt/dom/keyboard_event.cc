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

#include "cobalt/dom/keyboard_event.h"

#include "base/logging.h"
#include "cobalt/dom/event_names.h"

namespace cobalt {
namespace dom {

KeyboardEvent::KeyboardEvent(const std::string& type, KeyLocationCode location,
                             unsigned int modifiers, int key_code,
                             int char_code, bool is_repeat)
    : UIEventWithKeyState(type, modifiers),
      location_(location),
      key_code_(key_code),
      char_code_(char_code),
      repeat_(is_repeat) {}

// How to determine keycode:
//   http://www.w3.org/TR/DOM-Level-3-Events/#determine-keydown-keyup-keyCode
// Virtual key code for keyup/keydown, character code for keypress
int KeyboardEvent::key_code() const {
  if (type() == EventNames::GetInstance()->keydown() ||
      type() == EventNames::GetInstance()->keyup()) {
    return key_code_;
  }

  return char_code();
}

int KeyboardEvent::char_code() const {
  return type() == EventNames::GetInstance()->keypress() ? char_code_ : 0;
}

}  // namespace dom
}  // namespace cobalt

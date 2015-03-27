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

#ifndef DOM_KEYBOARD_EVENT_H_
#define DOM_KEYBOARD_EVENT_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/dom/ui_event_with_key_state.h"

namespace cobalt {
namespace dom {

// The KeyboardEvent provides specific contextual information associated with
// keyboard devices. Each keyboard event references a key using a value.
// Keyboard events are commonly directed at the element that has the focus.
//   http://www.w3.org/TR/DOM-Level-3-Events/#events-keyboardevents
class KeyboardEvent : public UIEventWithKeyState {
 public:
  // Web API: KeyboardEvent
  //
  enum KeyLocationCode {
    kDomKeyLocationStandard = 0x00,
    kDomKeyLocationLeft = 0x01,
    kDomKeyLocationRight = 0x02,
    kDomKeyLocationNumpad = 0x03,
  };

  KeyboardEvent(KeyLocationCode location, Type type, unsigned int modifiers,
                int key_code, int char_code, bool is_repeat);

  KeyLocationCode location() const { return location_; }
  bool repeat() const { return repeat_; }

  // Non-standard and deprecated.
  // key code for keydown and keyup, character for keypress
  //   http://www.w3.org/TR/DOM-Level-3-Events/#legacy-key-models
  int key_code() const;
  int char_code() const;

  // Custom, not in any spec.
  scoped_ptr<script::ScriptObjectHandle> CreateWrapper(
      EventWrapperCreator* creator) OVERRIDE {
    DCHECK(!get_wrapper_handle());
    return creator->CreateWrapper(make_scoped_refptr(this));
  }

 private:
  ~KeyboardEvent() OVERRIDE {}

  KeyLocationCode location_;
  int key_code_;
  int char_code_;
  bool repeat_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_KEYBOARD_EVENT_H_

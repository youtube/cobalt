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

#ifndef COBALT_DOM_KEYBOARD_EVENT_H_
#define COBALT_DOM_KEYBOARD_EVENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "cobalt/dom/ui_event_with_key_state.h"

namespace cobalt {
namespace dom {

// The KeyboardEvent provides specific contextual information associated with
// keyboard devices. Each keyboard event references a key using a value.
// Keyboard events are commonly directed at the element that has the focus.
//   https://www.w3.org/TR/DOM-Level-3-Events/#events-keyboardevents
class KeyboardEvent : public UIEventWithKeyState {
 public:
  // Non-standard, used to make creating KeyboardEvents via C++ easier.
  enum Type {
    kTypeKeyDown,
    kTypeKeyUp,
    kTypeKeyPress,
  };

  // Web API: KeyboardEvent
  //
  enum KeyLocationCode {
    kDomKeyLocationStandard = 0x00,
    kDomKeyLocationLeft = 0x01,
    kDomKeyLocationRight = 0x02,
    kDomKeyLocationNumpad = 0x03,
  };

  struct Data {
    Data()
        : type(kTypeKeyDown),
          key_location(kDomKeyLocationStandard),
          modifiers(0),
          key_code(0),
          char_code(0),
          repeat(false) {}

    Data(Type type, KeyLocationCode key_location, uint32_t modifiers,
         int32_t key_code, int32_t char_code, bool repeat)
        : type(type),
          key_location(key_location),
          modifiers(modifiers),
          key_code(key_code),
          char_code(char_code),
          repeat(repeat) {}

    Type type;
    KeyLocationCode key_location;
    uint32_t modifiers;
    int32_t key_code;
    int32_t char_code;
    bool repeat;
  };

  explicit KeyboardEvent(const std::string& type);
  explicit KeyboardEvent(const Data& data);
  KeyboardEvent(Type type, KeyLocationCode location, unsigned int modifiers,
                int key_code, int char_code, bool is_repeat);

  // Returns a string describing the key event, as defined here:
  //   https://www.w3.org/TR/DOM-Level-3-Events-key/
  std::string key() const;

  KeyLocationCode location() const { return data_.key_location; }
  bool repeat() const { return data_.repeat; }

  // Non-standard and deprecated.
  // key code for keydown and keyup, character for keypress
  //   https://www.w3.org/TR/DOM-Level-3-Events/#legacy-key-models
  int key_code() const;
  int char_code() const;
  KeyLocationCode key_location() const { return data_.key_location; }

  // keyIdentifier is deprecated and non-standard.
  // Here, we just map it to the standardized key() method, which matches some,
  // but not all browser implementations.
  std::string key_identifier() const { return key(); }

  // Custom, not in any spec.
  // Utility functions for keycode/charcode conversion.
  static int32 ComputeCharCode(int32 key_code, uint32 modifiers);
  static int KeyCodeToCharCodeWithShift(int key_code);
  static int KeyCodeToCharCodeNoShift(int key_code);
  static KeyLocationCode KeyCodeToKeyLocation(int key_code);

  DEFINE_WRAPPABLE_TYPE(KeyboardEvent);

 private:
  ~KeyboardEvent() OVERRIDE {}

  const Data data_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_KEYBOARD_EVENT_H_

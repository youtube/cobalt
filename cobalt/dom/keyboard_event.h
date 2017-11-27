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

#ifndef COBALT_DOM_KEYBOARD_EVENT_H_
#define COBALT_DOM_KEYBOARD_EVENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "cobalt/dom/keyboard_event_init.h"
#include "cobalt/dom/ui_event_with_key_state.h"

namespace cobalt {
namespace dom {

// The KeyboardEvent provides specific contextual information associated with
// keyboard devices. Each keyboard event references a key using a value.
// Keyboard events are commonly directed at the element that has the focus.
//   https://www.w3.org/TR/2016/WD-uievents-20160804/#events-keyboardevents
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
  explicit KeyboardEvent(const std::string& type);
  KeyboardEvent(const std::string& type, const KeyboardEventInit& init_dict);
  KeyboardEvent(base::Token type, const scoped_refptr<Window>& view,
                const KeyboardEventInit& init_dict);

  // Creates an event with its "initialized flag" unset.
  explicit KeyboardEvent(UninitializedFlag uninitialized_flag);

  void InitKeyboardEvent(const std::string& type, bool bubbles, bool cancelable,
                         const scoped_refptr<Window>& view,
                         const std::string& key, uint32 location,
                         const std::string& modifierslist, bool repeat);

  // Returns a string describing the key event, as defined here:
  //   https://www.w3.org/TR/DOM-Level-3-Events-key/
  std::string key() const;

  // Return a string describing the physical key pressed, not affected by
  // current keyboard layout or modifier state, as defined here:
  //   https://www.w3.org/TR/uievents-code/
  std::string code() const;

  KeyLocationCode location() const { return key_location_; }
  bool repeat() const { return repeat_; }
  bool is_composing() const { return false; }

  // Legacy Key Attributes.
  //   https://www.w3.org/TR/2016/WD-uievents-20160804/#legacy-key-attributes
  uint32 key_code() const;
  uint32 char_code() const;

  // Web API: UIEvent
  // This holds a system- and implementation-dependent numerical code signifying
  // the unmodified identifier associated with the key pressed. In most cases,
  // the value is identical to keyCode.
  //   https://www.w3.org/TR/2016/WD-uievents-20160804/#dom-keyboardevent-which
  uint32 which() const override;

  // keyLocation is deprecated and non-standard.
  //   https://www.w3.org/TR/2009/WD-DOM-Level-3-Events-20090908/#events-Events-KeyboardEvent-keylocation
  KeyLocationCode key_location() const { return key_location_; }

  // keyIdentifier is deprecated and non-standard.
  // Here, we just map it to the standardized key() method, which matches some,
  // but not all browser implementations.
  //   https://www.w3.org/TR/2009/WD-DOM-Level-3-Events-20090908/#events-Events-KeyboardEvent-keyIdentifier
  std::string key_identifier() const { return key(); }

  // Custom, not in any spec.
  // Utility functions for keycode/charcode conversion.
  static int32 ComputeCharCode(int32 key_code, bool shift_key);
  static int KeyCodeToCharCodeWithShift(int key_code);
  static int KeyCodeToCharCodeNoShift(int key_code);
  static KeyLocationCode KeyCodeToKeyLocation(int key_code);

  DEFINE_WRAPPABLE_TYPE(KeyboardEvent);

 private:
  ~KeyboardEvent() override {}
  std::string NonPrintableKey(int32_t key_code) const;

  KeyLocationCode key_location_;
  uint32_t key_code_;
  uint32_t char_code_;
  bool repeat_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_KEYBOARD_EVENT_H_

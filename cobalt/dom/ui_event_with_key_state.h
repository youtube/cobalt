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

#ifndef DOM_UI_EVENT_WITH_KEY_STATE_H_
#define DOM_UI_EVENT_WITH_KEY_STATE_H_

#include "cobalt/dom/ui_event.h"

namespace cobalt {
namespace dom {

// UIEventWithKeyState inherits UIEvent and has the key modifiers.
// Both KeyboardEvent and MouseEvent inherit UIEventWithKeyState.
class UIEventWithKeyState : public UIEvent {
 public:
  // Web API: KeyboardEvent, MouseEvent
  //
  static const unsigned int kNoModifier = 0;
  static const unsigned int kAltKey = 1 << 0;
  static const unsigned int kCtrlKey = 1 << 1;
  static const unsigned int kMetaKey = 1 << 2;
  static const unsigned int kShiftKey = 1 << 3;

  unsigned int modifiers() const { return modifiers_; }
  void set_modifiers(unsigned int modifiers) { modifiers_ = modifiers; }

  bool alt_key() const { return (modifiers_ & kAltKey) != 0; }
  bool ctrl_key() const { return (modifiers_ & kCtrlKey) != 0; }
  bool meta_key() const { return (modifiers_ & kMetaKey) != 0; }
  bool shift_key() const { return (modifiers_ & kShiftKey) != 0; }

  bool GetModifierState(const std::string& keyArg) const;

 protected:
  UIEventWithKeyState(const std::string& type, Bubbles bubbles,
                      Cancelable cancelable, unsigned int modifiers)
      : UIEvent(type, bubbles, cancelable), modifiers_(modifiers) {}

  ~UIEventWithKeyState() OVERRIDE {}

  unsigned int modifiers_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_UI_EVENT_WITH_KEY_STATE_H_

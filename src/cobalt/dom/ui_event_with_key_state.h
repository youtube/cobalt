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

#ifndef COBALT_DOM_UI_EVENT_WITH_KEY_STATE_H_
#define COBALT_DOM_UI_EVENT_WITH_KEY_STATE_H_

#include <string>

#include "cobalt/dom/event_modifier_init.h"
#include "cobalt/dom/ui_event.h"
#include "cobalt/dom/ui_event_init.h"

namespace cobalt {
namespace dom {

// UIEventWithKeyState inherits UIEvent and has the key modifiers.
// Both KeyboardEvent and MouseEvent inherit UIEventWithKeyState.
class UIEventWithKeyState : public UIEvent {
 public:
  // Web API: KeyboardEvent, MouseEvent
  //
  bool ctrl_key() const { return ctrl_key_; }
  bool shift_key() const { return shift_key_; }
  bool alt_key() const { return alt_key_; }
  bool meta_key() const { return meta_key_; }

  void set_ctrl_key(bool value) { ctrl_key_ = value; }
  void set_shift_key(bool value) { shift_key_ = value; }
  void set_alt_key(bool value) { alt_key_ = value; }
  void set_meta_key(bool value) { meta_key_ = value; }

  bool GetModifierState(const std::string& keyArg) const;

 protected:
  UIEventWithKeyState(base::Token type, Bubbles bubbles, Cancelable cancelable,
                      const scoped_refptr<Window>& view)
      : UIEvent(type, bubbles, cancelable, view),
        ctrl_key_(false),
        shift_key_(false),
        alt_key_(false),
        meta_key_(false) {}
  UIEventWithKeyState(base::Token type, Bubbles bubbles, Cancelable cancelable,
                      const scoped_refptr<Window>& view,
                      const EventModifierInit& init_dict)
      : UIEvent(type, bubbles, cancelable, view, init_dict) {
    ctrl_key_ = init_dict.ctrl_key();
    shift_key_ = init_dict.shift_key();
    alt_key_ = init_dict.alt_key();
    meta_key_ = init_dict.meta_key();
  }

  // Creates an event with its "initialized flag" unset.
  explicit UIEventWithKeyState(UninitializedFlag uninitialized_flag)
      : UIEvent(uninitialized_flag) {}

  void InitUIEventWithKeyState(const std::string& type, bool bubbles,
                               bool cancelable,
                               const scoped_refptr<Window>& view, int32 detail,
                               const std::string& modifierslist);
  void InitUIEventWithKeyState(const std::string& type, bool bubbles,
                               bool cancelable,
                               const scoped_refptr<Window>& view, int32 detail,
                               bool ctrl_key, bool alt_key, bool shift_key,
                               bool meta_key);
  ~UIEventWithKeyState() override {}

  bool ctrl_key_;
  bool shift_key_;
  bool alt_key_;
  bool meta_key_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_UI_EVENT_WITH_KEY_STATE_H_

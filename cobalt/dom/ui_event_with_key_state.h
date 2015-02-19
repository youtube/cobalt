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

class UIEventWithKeyState : public UIEvent {
 public:
  enum Modifiers {
    kAltKey = 1 << 0,
    kCtrlKey = 1 << 1,
    kMetaKey = 1 << 2,
    kShiftKey = 1 << 3,
  };

  bool alt_key() const { return modifiers_ & kAltKey; }
  bool ctrl_key() const { return modifiers_ & kCtrlKey; }
  bool meta_key() const { return modifiers_ & kMetaKey; }
  bool shift_key() const { return modifiers_ & kShiftKey; }

 protected:
  UIEventWithKeyState(Type type, Modifiers modifiers,
                      const scoped_refptr<Window>& view)
      : UIEvent(type, view), modifiers_(modifiers) {}

  ~UIEventWithKeyState() OVERRIDE {}

  unsigned int modifiers_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_UI_EVENT_WITH_KEY_STATE_H_

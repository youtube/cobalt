// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SYSTEM_WINDOW_INPUT_EVENT_H_
#define COBALT_SYSTEM_WINDOW_INPUT_EVENT_H_

#include "cobalt/base/event.h"
#include "cobalt/math/point_f.h"
#include "starboard/event.h"

namespace cobalt {
namespace system_window {

class InputEvent : public base::Event {
 public:
  enum Type {
    kKeyDown,
    kKeyUp,
    kKeyMove,
  };

  // Bit-mask of key modifiers. These correspond to the |SbKeyModifiers| values
  // defined in Starboard.
  enum Modifiers {
    kNoModifier = 0,
    kAltKey = 1 << 0,
    kCtrlKey = 1 << 1,
    kMetaKey = 1 << 2,
    kShiftKey = 1 << 3,
  };

  InputEvent(Type type, int key_code, uint32 modifiers, bool is_repeat,
             const math::PointF& position = math::PointF())
      : type_(type),
        key_code_(key_code),
        modifiers_(modifiers),
        is_repeat_(is_repeat),
        position_(position) {}

  Type type() const { return type_; }
  int key_code() const { return key_code_; }
  uint32 modifiers() const { return modifiers_; }
  bool is_repeat() const { return is_repeat_; }
  const math::PointF& position() const { return position_; }

  BASE_EVENT_SUBCLASS(InputEvent);

 private:
  Type type_;
  int key_code_;
  uint32 modifiers_;
  bool is_repeat_;
  math::PointF position_;
};

// The Starboard Event handler SbHandleEvent should call this function on
// unrecognized events. It will extract Input events and dispatch them to the
// appropriate SystemWindow for further routing.
void HandleInputEvent(const SbEvent* event);

}  // namespace system_window
}  // namespace cobalt

#endif  // COBALT_SYSTEM_WINDOW_INPUT_EVENT_H_

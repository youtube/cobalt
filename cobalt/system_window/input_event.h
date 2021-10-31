// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include <string>

#include "cobalt/base/event.h"
#include "cobalt/math/point_f.h"
#include "starboard/event.h"
#include "starboard/time.h"

namespace cobalt {
namespace system_window {

class InputEvent : public base::Event {
 public:
  enum Type {
    kKeyDown,
    kKeyUp,
    kKeyMove,
    kInput,
    kPointerDown,
    kPointerUp,
    kPointerMove,
    kTouchpadDown,
    kTouchpadUp,
    kTouchpadMove,
    kTouchscreenDown,
    kTouchscreenUp,
    kTouchscreenMove,
    kWheel,
  };

  // Bit-mask of key modifiers. These correspond to the |SbKeyModifiers| values
  // defined in Starboard.
  enum Modifiers {
    kNoModifier = 0,
    kAltKey = 1 << 0,
    kCtrlKey = 1 << 1,
    kMetaKey = 1 << 2,
    kShiftKey = 1 << 3,
    kLeftButton = 1 << 4,
    kRightButton = 1 << 5,
    kMiddleButton = 1 << 6,
    kBackButton = 1 << 7,
    kForwardButton = 1 << 8,
  };

  InputEvent(SbTimeMonotonic timestamp, Type type, int device_id, int key_code,
             uint32 modifiers, bool is_repeat,
             const math::PointF& position = math::PointF(),
             const math::PointF& delta = math::PointF(), float pressure = 0,
             const math::PointF& size = math::PointF(),
             const math::PointF& tilt = math::PointF()
#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
                 ,
             const std::string& input_text = "", bool is_composing = false
#endif  // SB_API_VERSION >= 12 ||
             // SB_HAS(ON_SCREEN_KEYBOARD)
             )
      : timestamp_(timestamp),
        type_(type),
        device_id_(device_id),
        key_code_(key_code),
        modifiers_(modifiers),
        is_repeat_(is_repeat),
        position_(position),
        delta_(delta),
        pressure_(pressure),
        size_(size),
        tilt_(tilt)
#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
        ,
        input_text_(input_text),
        is_composing_(is_composing)
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)
  {
  }

  ~InputEvent() {}

  SbTimeMonotonic timestamp() const { return timestamp_; }
  Type type() const { return type_; }
  int key_code() const { return key_code_; }
  int device_id() const { return device_id_; }
  uint32 modifiers() const { return modifiers_; }
  bool is_repeat() const { return is_repeat_; }
  const math::PointF& position() const { return position_; }
  const math::PointF& delta() const { return delta_; }
  float pressure() const { return pressure_; }
  const math::PointF& size() const { return size_; }
  const math::PointF& tilt() const { return tilt_; }
#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
  const std::string& input_text() const { return input_text_; }
  bool is_composing() const { return is_composing_; }
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)

  BASE_EVENT_SUBCLASS(InputEvent);

 private:
  SbTimeMonotonic timestamp_;
  Type type_;
  int device_id_;
  int key_code_;
  uint32 modifiers_;
  bool is_repeat_;
  math::PointF position_;
  math::PointF delta_;
  float pressure_;
  math::PointF size_;
  math::PointF tilt_;
#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
  std::string input_text_;
  bool is_composing_;
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)
};

// The Starboard Event handler SbHandleEvent should call this function on
// unrecognized events. It will extract Input events and dispatch them to the
// appropriate SystemWindow for further routing.
void HandleInputEvent(const SbEvent* event);

}  // namespace system_window
}  // namespace cobalt

#endif  // COBALT_SYSTEM_WINDOW_INPUT_EVENT_H_

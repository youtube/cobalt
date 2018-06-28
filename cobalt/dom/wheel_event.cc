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

#include "cobalt/dom/wheel_event.h"

#include <string>

#include "cobalt/dom/ui_event_with_key_state.h"

namespace cobalt {
namespace dom {

WheelEvent::WheelEvent(const std::string& type)
    : MouseEvent(type),
      delta_x_(0),
      delta_y_(0),
      delta_z_(0),
      delta_mode_(kDomDeltaPixel) {}
WheelEvent::WheelEvent(const std::string& type, const WheelEventInit& init_dict)
    : MouseEvent(type, init_dict),
      delta_x_(init_dict.delta_x()),
      delta_y_(init_dict.delta_y()),
      delta_z_(init_dict.delta_z()),
      delta_mode_(init_dict.delta_mode()) {}

WheelEvent::WheelEvent(base::Token type, const scoped_refptr<Window>& view,
                       const WheelEventInit& init_dict)
    : MouseEvent(type, kBubbles, kCancelable, view, init_dict),
      delta_x_(init_dict.delta_x()),
      delta_y_(init_dict.delta_y()),
      delta_z_(init_dict.delta_z()),
      delta_mode_(init_dict.delta_mode()) {}

WheelEvent::WheelEvent(UninitializedFlag uninitialized_flag)
    : MouseEvent(uninitialized_flag),
      delta_x_(0),
      delta_y_(0),
      delta_z_(0),
      delta_mode_(kDomDeltaPixel) {}

void WheelEvent::InitWheelEvent(
    const std::string& type, bool bubbles, bool cancelable,
    const scoped_refptr<Window>& view, int32 detail, int32 screen_x,
    int32 screen_y, int32 client_x, int32 client_y, uint16 button,
    const scoped_refptr<EventTarget>& related_target,
    const std::string& modifierslist, double delta_x, double delta_y,
    double delta_z, uint32 delta_mode) {
  InitMouseEvent(type, bubbles, cancelable, view, detail, screen_x, screen_y,
                 client_x, client_y, modifierslist, button, related_target);
  delta_x_ = delta_x;
  delta_y_ = delta_y;
  delta_z_ = delta_z;
  delta_mode_ = delta_mode;
}

}  // namespace dom
}  // namespace cobalt

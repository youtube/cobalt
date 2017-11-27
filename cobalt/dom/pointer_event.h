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

#ifndef COBALT_DOM_POINTER_EVENT_H_
#define COBALT_DOM_POINTER_EVENT_H_

#include <string>

#include "cobalt/base/token.h"
#include "cobalt/dom/mouse_event.h"
#include "cobalt/dom/pointer_event_init.h"

namespace cobalt {
namespace dom {

// The PointerEvent provides specific contextual information associated with
// pointer devices. A pointer is a hardware agnostic representation of input
// devices that can target a specific coordinate (or set of coordinates) on a
// screen.
//   https://www.w3.org/TR/2015/REC-pointerevents-20150224/
class PointerEvent : public MouseEvent {
 public:
  explicit PointerEvent(const std::string& type);
  PointerEvent(const std::string& type, const PointerEventInit& init_dict);
  PointerEvent(base::Token type, const scoped_refptr<Window>& view,
               const PointerEventInit& init_dict);
  PointerEvent(base::Token type, Bubbles bubbles, Cancelable cancelable,
               const scoped_refptr<Window>& view,
               const PointerEventInit& init_dict);

  int32_t pointer_id() const { return pointer_id_; }
  double width() const { return width_; }
  double height() const { return height_; }
  float pressure() const { return pressure_; }
  int32_t tilt_x() const { return tilt_x_; }
  int32_t tilt_y() const { return tilt_y_; }
  const std::string& pointer_type() const { return pointer_type_; }
  int32_t is_primary() const { return is_primary_; }

  DEFINE_WRAPPABLE_TYPE(PointerEvent);

 private:
  ~PointerEvent() override {}

  int32_t pointer_id_;
  double width_;
  double height_;
  float pressure_;
  int32_t tilt_x_;
  int32_t tilt_y_;
  std::string pointer_type_;
  bool is_primary_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_POINTER_EVENT_H_

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

#include "cobalt/dom/pointer_event.h"

#include <string>

namespace cobalt {
namespace dom {

PointerEvent::PointerEvent(const std::string& type)
    : MouseEvent(type),
      pointer_id_(0),
      width_(0),
      height_(0),
      pressure_(0),
      tilt_x_(0),
      tilt_y_(0),
      is_primary_(false) {}

PointerEvent::PointerEvent(const std::string& type,
                           const PointerEventInit& init_dict)
    : MouseEvent(type, init_dict),
      pointer_id_(init_dict.pointer_id()),
      width_(init_dict.width()),
      height_(init_dict.height()),
      pressure_(init_dict.pressure()),
      tilt_x_(init_dict.tilt_x()),
      tilt_y_(init_dict.tilt_y()),
      pointer_type_(init_dict.pointer_type()),
      is_primary_(init_dict.is_primary()) {}

PointerEvent::PointerEvent(base::Token type, const scoped_refptr<Window>& view,
                           const PointerEventInit& init_dict)
    : MouseEvent(type, view, init_dict),
      pointer_id_(init_dict.pointer_id()),
      width_(init_dict.width()),
      height_(init_dict.height()),
      pressure_(init_dict.pressure()),
      tilt_x_(init_dict.tilt_x()),
      tilt_y_(init_dict.tilt_y()),
      pointer_type_(init_dict.pointer_type()),
      is_primary_(init_dict.is_primary()) {}

PointerEvent::PointerEvent(base::Token type, Bubbles bubbles,
                           Cancelable cancelable,
                           const scoped_refptr<Window>& view,
                           const PointerEventInit& init_dict)
    : MouseEvent(type, bubbles, cancelable, view, init_dict),
      pointer_id_(init_dict.pointer_id()),
      width_(init_dict.width()),
      height_(init_dict.height()),
      pressure_(init_dict.pressure()),
      tilt_x_(init_dict.tilt_x()),
      tilt_y_(init_dict.tilt_y()),
      pointer_type_(init_dict.pointer_type()),
      is_primary_(init_dict.is_primary()) {}

}  // namespace dom
}  // namespace cobalt

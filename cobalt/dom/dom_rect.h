// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_DOM_RECT_H_
#define COBALT_DOM_DOM_RECT_H_

#include "cobalt/dom/dom_rect_read_only.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// Objects implementing the DOMRect interface represent a rectangle.
//   https://www.w3.org/TR/2014/CR-geometry-1-20141125/#DOMRect
class DOMRect : public DOMRectReadOnly {
 public:
  DOMRect() : DOMRectReadOnly(0.0f, 0.0f, 0.0f, 0.0f) {}
  explicit DOMRect(const math::RectF& rect) : DOMRectReadOnly(rect) {}
  explicit DOMRect(float x) : DOMRectReadOnly(x, 0.0f, 0.0f, 0.0f) {}
  DOMRect(float x, float y) : DOMRectReadOnly(x, y, 0.0f, 0.0f) {}
  DOMRect(float x, float y, float width) : DOMRectReadOnly(x, y, width, 0.0f) {}
  DOMRect(float x, float y, float width, float height)
      : DOMRectReadOnly(x, y, width, height) {}

  // Web API: DOMRect
  //
  void set_x(float x) { rect_.set_x(x); }
  void set_y(float y) { rect_.set_y(y); }
  void set_width(float width) { rect_.set_width(width); }
  void set_height(float height) { rect_.set_height(height); }

  DEFINE_WRAPPABLE_TYPE(DOMRect);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOM_RECT_H_

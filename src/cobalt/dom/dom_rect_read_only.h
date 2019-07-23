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

#ifndef COBALT_DOM_DOM_RECT_READ_ONLY_H_
#define COBALT_DOM_DOM_RECT_READ_ONLY_H_

#include <algorithm>

#include "cobalt/math/rect_f.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// Objects implementing the DOMRectReadOnly interface represent a rectangle.
//   https://www.w3.org/TR/2014/CR-geometry-1-20141125/#DOMRect
class DOMRectReadOnly : public script::Wrappable {
 public:
  DOMRectReadOnly(float x, float y, float width, float height)
      : rect_(x, y, width, height) {}
  explicit DOMRectReadOnly(const math::RectF& rect) : rect_(rect) {}

  // Web API: DOMRectReadOnly
  //
  float x() const { return rect_.x(); }
  float y() const { return rect_.y(); }
  float width() const { return rect_.width(); }
  float height() const { return rect_.height(); }

  float top() const { return std::min(rect_.y(), rect_.y() + rect_.height()); }
  float right() const { return std::max(rect_.x(), rect_.x() + rect_.width()); }
  float bottom() const {
    return std::max(rect_.y(), rect_.y() + rect_.height());
  }
  float left() const { return std::min(rect_.x(), rect_.x() + rect_.width()); }

  // Custom, not in any spec
  const math::RectF& rect() const { return rect_; }

  DEFINE_WRAPPABLE_TYPE(DOMRectReadOnly);

 protected:
  math::RectF rect_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOM_RECT_READ_ONLY_H_

// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/render_tree/border.h"

namespace cobalt {
namespace render_tree {

BorderSide::BorderSide(const BorderSide& that)
    : width(that.width), style(that.style), color(that.color) {}

BorderSide::BorderSide(float width, BorderStyle style, const ColorRGBA& color)
    : width(width), style(style), color(color) {}

Border::Border(const BorderSide& border_side)
    : left(border_side),
      right(border_side),
      top(border_side),
      bottom(border_side) {}

Border::Border(const BorderSide& left, const BorderSide& right,
               const BorderSide& top, const BorderSide& bottom)
    : left(left), right(right), top(top), bottom(bottom) {}

std::ostream& operator<<(std::ostream& out, const BorderSide& side) {
  out << "width: " << side.width << ", color: " << side.color
      << ", style: " << side.style;
  return out;
}

std::ostream& operator<<(std::ostream& out, const Border& border) {
  out << "Top [" << border.top << "] Left [" << border.left << "] Right ["
      << border.right << "] Bottom [" << border.bottom << "]";
  return out;
}

}  // namespace render_tree
}  // namespace cobalt

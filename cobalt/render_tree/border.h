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

#ifndef COBALT_RENDER_TREE_BORDER_H_
#define COBALT_RENDER_TREE_BORDER_H_

#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/brush.h"

namespace cobalt {
namespace render_tree {

// A style of a border segment.
enum BorderStyle {
  // A border segment along the given side should not be drawn.
  kBorderStyleNone,
  // A border segment along the given side should be drawn.
  kBorderStyleSolid
};

// Style properties for one of four sides of a border around a rectangle.
struct BorderSide {
  BorderSide(const BorderSide& that);
  BorderSide(float width, BorderStyle style, const ColorRGBA& color);

  bool operator==(const BorderSide& rhs) const {
    return style == rhs.style && color == rhs.color && width == rhs.width;
  }
  bool operator!=(const BorderSide& rhs) const {
    return !(*this == rhs);
  }

  float width;
  BorderStyle style;
  ColorRGBA color;
};

// A border around a rectangle.
struct Border {
  explicit Border(const BorderSide& border_side);
  Border(const BorderSide& left, const BorderSide& right, const BorderSide& top,
         const BorderSide& bottom);

  bool operator==(const Border& other) const {
    return left == other.left && right == other.right && top == other.top &&
           bottom == other.bottom;
  }

  BorderSide left;
  BorderSide right;
  BorderSide top;
  BorderSide bottom;
};

std::ostream& operator<<(std::ostream& out, const BorderSide& border_side);
std::ostream& operator<<(std::ostream& out, const Border& border);

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_BORDER_H_

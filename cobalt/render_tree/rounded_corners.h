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

#ifndef COBALT_RENDER_TREE_ROUNDED_CORNERS_H_
#define COBALT_RENDER_TREE_ROUNDED_CORNERS_H_

#include <algorithm>

#include "cobalt/math/insets_f.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace render_tree {

// RoundedCorner represents one of the corners of an rectangle. It contains the
// lengths of the semi-major axis and the semi-minor axis of an ellipse.
struct RoundedCorner {
  RoundedCorner() : horizontal(0.0f), vertical(0.0f) {}

  RoundedCorner(float horizontal, float vertical)
      : horizontal(horizontal), vertical(vertical) {}

  //  If either length is zero-ish, the corner is square, not rounded.
  bool IsSquare() const;

  RoundedCorner Inset(float x, float y) const {
    return RoundedCorner(std::max(0.0f, horizontal - x),
                         std::max(0.0f, vertical - y));
  }

  bool operator==(const RoundedCorner& other) const {
    return horizontal == other.horizontal && vertical == other.vertical;
  }
  bool operator>=(const RoundedCorner& other) const {
    return horizontal >= other.horizontal && vertical >= other.vertical;
  }

  // |horizontal| and |vertical| represent the horizontal radius and vertical
  // radius of a corner.
  float horizontal;
  float vertical;
};

// RoundedCorners represents 4 rounded corners of an rectangle. Top left, top
// right, bottom right and bottom left.
struct RoundedCorners {
  explicit RoundedCorners(const RoundedCorner& corner)
      : top_left(corner),
        top_right(corner),
        bottom_right(corner),
        bottom_left(corner) {}

  RoundedCorners(float horizontal, float vertical)
      : top_left(horizontal, vertical),
        top_right(horizontal, vertical),
        bottom_right(horizontal, vertical),
        bottom_left(horizontal, vertical) {}

  RoundedCorners(const RoundedCorners& radiuses)
      : top_left(radiuses.top_left),
        top_right(radiuses.top_right),
        bottom_right(radiuses.bottom_right),
        bottom_left(radiuses.bottom_left) {}

  RoundedCorners(const RoundedCorner& top_left, const RoundedCorner& top_right,
                 const RoundedCorner& bottom_right,
                 const RoundedCorner& bottom_left)
      : top_left(top_left),
        top_right(top_right),
        bottom_right(bottom_right),
        bottom_left(bottom_left) {}

  bool operator==(const RoundedCorners& other) const {
    return top_left == other.top_left && top_right == other.top_right &&
           bottom_right == other.bottom_right &&
           bottom_left == other.bottom_left;
  }

  RoundedCorners Inset(float left, float top, float right, float bottom) const {
    return RoundedCorners(
        top_left.Inset(left, top), top_right.Inset(right, top),
        bottom_right.Inset(right, bottom), bottom_left.Inset(left, bottom));
  }

  RoundedCorners Inset(const math::InsetsF& insets) const {
    return Inset(insets.left(), insets.top(), insets.right(), insets.bottom());
  }

  RoundedCorners Scale(float sx, float sy) const;

  // Ensure the rounded corners' radii do not exceed the length of the
  // corresponding edge of the given rect.
  RoundedCorners Normalize(const math::RectF& rect) const;
  bool IsNormalized(const math::RectF& rect) const;

  bool AreSquares() const {
    return top_left.IsSquare() && top_right.IsSquare() &&
           bottom_right.IsSquare() && bottom_left.IsSquare();
  }

  // Returns true if all corners have the same value as the input.
  bool AllCornersEqual(const RoundedCorner& rhs) const {
    return top_left == rhs && top_right == rhs && bottom_right == rhs &&
           bottom_left == rhs;
  }

  // Returns true if all corners' radii are greater than or equal to the
  // corresponding radii of the input.
  bool AllCornersGE(const RoundedCorner& rhs) const {
    return top_left >= rhs && top_right >= rhs && bottom_right >= rhs &&
           bottom_left >= rhs;
  }

  RoundedCorner top_left;
  RoundedCorner top_right;
  RoundedCorner bottom_right;
  RoundedCorner bottom_left;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_ROUNDED_CORNERS_H_

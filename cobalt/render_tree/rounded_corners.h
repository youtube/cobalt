/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_RENDER_TREE_ROUNDED_CORNERS_H_
#define COBALT_RENDER_TREE_ROUNDED_CORNERS_H_

namespace cobalt {
namespace render_tree {

// RoundedCorner represents one of the corners of an rectangle. It contains the
// lengths of the semi-major axis and the semi-minor axis of an ellipse.
struct RoundedCorner {
  RoundedCorner() : horizontal(0.0f), vertical(0.0f) {}

  RoundedCorner(float horizontal, float vertical)
      : horizontal(horizontal), vertical(vertical) {}

  //  If either length is zero, the corner is square, not rounded.
  bool IsSquare() const { return horizontal == 0.0f || vertical == 0.0f; }

  // |horizontal| and |vertial| represent the horizontal radius and vertical
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

  bool AreSquares() const {
    return top_left.IsSquare() && top_right.IsSquare() &&
           bottom_right.IsSquare() && bottom_left.IsSquare();
  }

  RoundedCorner top_left;
  RoundedCorner top_right;
  RoundedCorner bottom_right;
  RoundedCorner bottom_left;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_ROUNDED_CORNERS_H_

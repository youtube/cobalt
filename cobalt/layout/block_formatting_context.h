/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef LAYOUT_BLOCK_FORMATTING_CONTEXT_H_
#define LAYOUT_BLOCK_FORMATTING_CONTEXT_H_

#include "base/optional.h"
#include "cobalt/math/point_f.h"

namespace cobalt {
namespace layout {

class Box;

// In a block formatting context, boxes are laid out one after the other,
// vertically, beginning at the top of a containing block.
//   http://www.w3.org/TR/CSS21/visuren.html#block-formatting
//
// A block formatting context is a short-lived object that is constructed
// and destroyed during the layout. The block formatting context does not own
// child boxes nor triggers their layout - it is a responsibility of the box
// that establishes this formatting context. This class merely knows how
// to update the position of the subsequent children passed to it.
class BlockFormattingContext {
 public:
  BlockFormattingContext();

  // Calculates the used position of the given child box and updates
  // the internal state in the preparation for the next child.
  void UpdateUsedPosition(Box* child_box);

  // Used to calculate the "auto" width of the box that establishes this
  // formatting context.
  float shrink_to_fit_width() const { return shrink_to_fit_width_; }

  // A vertical offset of the baseline of the last child box that has one,
  // relatively to the origin of the block container box. Disengaged, if none
  // of the child boxes have a baseline.
  base::optional<float> height_above_baseline() const {
    return height_above_baseline_;
  }

  // Used to calculate the "auto" height of the box that establishes this
  // formatting context.
  //
  // TODO(***REMOVED***): Fix when a margin collapsing is implemented.
  float last_child_box_used_bottom() const {
    return next_child_box_used_position_.y();
  }

 private:
  // In a block formatting context, each box's left outer edge touches
  // the left edge of the containing block.
  //   http://www.w3.org/TR/CSS21/visuren.html#block-formatting
  //
  // According to the above, we default-construct the position of the first
  // child at (0, 0).
  math::PointF next_child_box_used_position_;

  float shrink_to_fit_width_;

  base::optional<float> height_above_baseline_;

  DISALLOW_COPY_AND_ASSIGN(BlockFormattingContext);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_BLOCK_FORMATTING_CONTEXT_H_

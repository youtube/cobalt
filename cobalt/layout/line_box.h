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

#ifndef LAYOUT_LINE_BOX_H_
#define LAYOUT_LINE_BOX_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/optional.h"

namespace cobalt {
namespace layout {

class Box;

// The rectangular area that contains the boxes that form a line is called
// a line box.
//   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
//
// Note that the line box is not an actual box. To maintain consistency, we
// follow the nomenclature of CSS 2.1 specification. But we do not derive
// |LineBox| from |Box| because, despite the name, a line box is much closer
// to block and inline formatting contexts than to a box.
//
// A line box is a short-lived object that is constructed and destroyed during
// the layout. It does not own child boxes nor trigger their layout, which it is
// a responsibility of the box that establishes this formatting context. This
// class merely knows how to update the position of the subsequent children
// passed to it.
//
// Due to the horizontal and vertical alignment, used values of "left" and "top"
// can only be calculated when the line ends. To ensure that the line box has
// completed all calculations, |UpdateUsedTops| must be called after
// |TryUpdateUsedLeftAndMaybeSplit| or |UpdateUsedLeftAndMaybeOverflow| have
// been called for every child box.
class LineBox {
 public:
  LineBox(float used_top, float containing_block_width);

  float used_top() const { return used_top_; }

  // When used outside of an inline formatting context (for example, to lay out
  // an inline container box), the line box may be configured to not trim
  // the leading and trailing white space.
  void set_should_trim_white_space(bool should_trim_white_space) {
    should_trim_white_space_ = should_trim_white_space;
  }

  // Attempts to calculate the used values of "left" and "top" for the given
  // child box if the box or a part of it fits on the line. Some parts of
  // calculation are asynchronous, so the child's position is in undefined state
  // until |EndQueries| is called.
  //
  // Returns false if the box does not fit on the line, even if split.
  //
  // If the child box had to be split in order to fit on the line, the part
  // of the box after the split is stored in |child_box_after_split|. The box
  // that establishes this formatting context must re-insert the returned part
  // right after the original child box.
  bool TryQueryUsedPositionAndMaybeSplit(
      Box* child_box, scoped_ptr<Box>* child_box_after_split);
  // Asynchronously calculates the used values of "left" and "top" for the given
  // child box, ignoring the possible overflow. The child's position is
  // in undefined state until |EndQueries| is called.
  void QueryUsedPositionAndMaybeOverflow(Box* child_box);
  // Ensures that the calculation of used values of "left" and "top" for all
  // previously seen child boxes is completed.
  void EndQueries();

  // WARNING: All public methods below may be called only after |EndQueries|.

  // Line boxes that contain no text, no preserved white space, no inline
  // elements with non-zero margins, padding, or borders, and no other in-flow
  // content must be treated as zero-height line boxes for the purposes
  // of determining the positions of any elements inside of them, and must be
  // treated as not existing for any other purpose.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  bool line_exists() const { return line_exists_; }

  // Used to calculate the width of an inline container box.
  float GetShrinkToFitWidth() const;

  // Used to calculate the "auto" height of the box that establishes this
  // formatting context.
  float used_height() const { return used_height_; }

  // Returns the vertical offset of the baseline. May return non-zero values
  // even for empty line boxes, because of the strut.
  //   http://www.w3.org/TR/CSS21/visudet.html#strut
  float height_above_baseline() const { return height_above_baseline_; }

 private:
  void CollapseTrailingWhiteSpace() const;

  const float used_top_;
  const float containing_block_width_;
  bool should_trim_white_space_;

  bool line_exists_;

  // Non-owned list of child boxes.
  //
  // Used to calculate used values of "top" which can only be determined when
  // all child boxes are known, due to a vertical alignment.
  typedef std::vector<Box*> ChildBoxes;
  ChildBoxes child_boxes_;

  base::optional<size_t> last_non_collapsed_child_box_index_;

  float used_height_;
  float height_above_baseline_;

  DISALLOW_COPY_AND_ASSIGN(LineBox);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_LINE_BOX_H_

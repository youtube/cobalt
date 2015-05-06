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

#include "cobalt/layout/line_box.h"

#include "cobalt/layout/box.h"

namespace cobalt {
namespace layout {

// The left edge of a line box touches the left edge of its containing block.
//   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
LineBox::LineBox(float used_top, float containing_block_width,
                 ShouldTrimWhiteSpace should_trim_white_space)
    : used_top_(used_top),
      containing_block_width_(containing_block_width),
      should_trim_white_space_(should_trim_white_space),
      line_exists_(false),
      used_height_(0),
      height_above_baseline_(0) {}

bool LineBox::TryQueryUsedPositionAndMaybeSplit(
    Box* child_box, scoped_ptr<Box>* child_box_after_split) {
  DCHECK_EQ(static_cast<Box*>(NULL), child_box_after_split->get());

  // Horizontal margins, borders, and padding are respected between boxes.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  // TODO(***REMOVED***): Implement the above.
  float available_width = containing_block_width_ - GetShrinkToFitWidth();
  if (child_box->used_frame().width() <= available_width) {
    QueryUsedPositionAndMaybeOverflow(child_box);
    return true;
  }

  // When an inline box exceeds the width of a line box, it is split.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  *child_box_after_split = child_box->TrySplitAt(available_width);
  if (*child_box_after_split) {
    DCHECK_LE(child_box->used_frame().width(), available_width);
    QueryUsedPositionAndMaybeOverflow(child_box);
    return true;
  }

  return false;
}

void LineBox::QueryUsedPositionAndMaybeOverflow(Box* child_box) {
  // A sequence of collapsible spaces at the beginning of a line is removed.
  //   http://www.w3.org/TR/css3-text/#white-space-phase-2
  if (should_trim_white_space_ && !last_non_collapsed_child_box_index_) {
    child_box->CollapseLeadingWhiteSpace();
  }

  // Any space immediately following another collapsible space - even one
  // outside the boundary of the inline containing that space, provided they
  // are both within the same inline formatting context - is collapsed.
  //   http://www.w3.org/TR/css3-text/#white-space-phase-1
  if (last_non_collapsed_child_box_index_ &&
      child_boxes_[*last_non_collapsed_child_box_index_]
          ->HasTrailingWhiteSpace()) {
    child_box->CollapseLeadingWhiteSpace();
  }

  // Horizontal margins, borders, and padding are respected between boxes.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  // TODO(***REMOVED***): Implement the above.
  child_box->used_frame().set_x(GetShrinkToFitWidth());

  if (!child_box->IsCollapsed()) {
    last_non_collapsed_child_box_index_ = child_boxes_.size();
  }

  child_boxes_.push_back(child_box);
}

void LineBox::EndQueries() {
  // A sequence of collapsible spaces at the end of a line is removed.
  //   http://www.w3.org/TR/css3-text/#white-space-phase-2
  if (should_trim_white_space_) {
    CollapseTrailingWhiteSpace();
  }

  // TODO(***REMOVED***): Align child boxes vertically according to "line-height"
  //               and "vertical-align":
  //               http://www.w3.org/TR/CSS21/visudet.html#line-height
  NOTIMPLEMENTED();

  line_exists_ = false;

  // TODO(***REMOVED***): The minimum height consists of a minimum height above
  //               the baseline and a minimum depth below it, exactly as if
  //               each line box starts with a zero-width inline box with
  //               the element's font and line height properties.
  //               We call that imaginary box a "strut."
  //               http://www.w3.org/TR/CSS21/visudet.html#strut
  height_above_baseline_ = 0;
  float height_below_baseline = 0;
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    line_exists_ = line_exists_ || child_box->JustifiesLineExistence();

    float child_height_above_baseline = child_box->GetHeightAboveBaseline();
    height_above_baseline_ =
        std::max(height_above_baseline_, child_height_above_baseline);

    float child_height_below_baseline =
        child_box->used_frame().height() - child_height_above_baseline;
    height_below_baseline =
        std::max(height_below_baseline, child_height_below_baseline);
  }
  // The line box height is the distance between the uppermost box top and the
  // lowermost box bottom.
  //   http://www.w3.org/TR/CSS21/visudet.html#line-height
  used_height_ = height_above_baseline_ + height_below_baseline;

  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->used_frame().set_y(used_top_ + height_above_baseline_ -
                                  child_box->GetHeightAboveBaseline());
  }
}

float LineBox::GetShrinkToFitWidth() const {
  return child_boxes_.empty() ? 0 : child_boxes_.back()->used_frame().right();
}

void LineBox::CollapseTrailingWhiteSpace() const {
  if (!last_non_collapsed_child_box_index_) {
    return;
  }

  ChildBoxes::const_iterator child_box_iterator =
      child_boxes_.begin() + static_cast<ChildBoxes::difference_type>(
                                 *last_non_collapsed_child_box_index_);
  Box* child_box = *child_box_iterator;
  if (!child_box->HasTrailingWhiteSpace()) {
    return;
  }

  // Collapse the trailing white space.
  float child_box_pre_collapse_width = child_box->used_frame().width();
  child_box->CollapseTrailingWhiteSpace();
  float collapsed_white_space_width =
      child_box_pre_collapse_width - child_box->used_frame().width();
  DCHECK_GT(collapsed_white_space_width, 0);

  // Adjust the positions of subsequent child boxes.
  for (++child_box_iterator; child_box_iterator != child_boxes_.end();
       ++child_box_iterator) {
    child_box = *child_box_iterator;
    child_box->used_frame().Offset(-collapsed_white_space_width, 0);
  }
}

}  // namespace layout
}  // namespace cobalt

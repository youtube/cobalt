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

#include <algorithm>

#include "cobalt/layout/box.h"
#include "cobalt/cssom/keyword_value.h"

namespace cobalt {
namespace layout {

// The left edge of a line box touches the left edge of its containing block.
//   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
LineBox::LineBox(float used_top, float x_height,
                 ShouldTrimWhiteSpace should_trim_white_space,
                 const scoped_refptr<cssom::PropertyValue>& text_align,
                 const LayoutParams& layout_params)
    : used_top_(used_top),
      x_height_(x_height),
      should_trim_white_space_(should_trim_white_space),
      text_align_(text_align),
      layout_params_(layout_params),
      line_exists_(false),
      line_has_bidi_reversed_children_(false),
      used_height_(0),
      baseline_offset_from_top_(0) {}

bool LineBox::TryQueryUsedRectAndMaybeSplit(
    Box* child_box, scoped_ptr<Box>* child_box_after_split) {
  DCHECK(!*child_box_after_split);

  // TODO(***REMOVED***): A position of "fixed" is also out-of-flow and should not
  //               affect the block formatting context's state.
  if (child_box->computed_style()->position() ==
          cssom::KeywordValue::GetAbsolute()) {
    // TODO(***REMOVED***): Reimplement this crude approximation of static position
    //               in order to take horizontal and vertical alignments
    //               into consideration.
    child_box->set_left(GetShrinkToFitWidth());
    child_box->set_top(0);
    return true;
  }

  child_box->UpdateSize(layout_params_);

  // Horizontal margins, borders, and padding are respected between boxes.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  // TODO(***REMOVED***): Implement the above.
  float available_width =
      layout_params_.containing_block_size.width() - GetShrinkToFitWidth();

  // Allow overflow if the line box has no non-collapsed child boxes. The line
  // box allows overflow in the case where no smaller split is available until
  // one non-collapsed box is added (according to the CSS 2.1 specification, an
  // anonymous inline box is not generated when the whitespace content is
  // collapsed away--this case is represented by collapsed text boxes in our
  // implementation).
  //  http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  //  http://www.w3.org/TR/CSS21/visuren.html#anonymous
  bool allow_overflow = !last_non_collapsed_child_box_index_;

  if (child_box->width() <= available_width) {
    QueryUsedRectAndMaybeOverflow(child_box);
    return true;
  }

  // When an inline box exceeds the width of a line box, it is split.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  *child_box_after_split =
      child_box->TrySplitAt(available_width, allow_overflow);
  if (*child_box_after_split) {
    QueryUsedRectAndMaybeOverflow(child_box);
    if (!allow_overflow) {
      DCHECK_LE(child_box->width(), available_width);
    }
    return true;
  }

  return false;
}

void LineBox::QueryUsedRectAndMaybeOverflow(Box* child_box) {
  // TODO(***REMOVED***): A position of "fixed" is also out-of-flow and should not
  //               affect the block formatting context's state.
  if (child_box->computed_style()->position() ==
          cssom::KeywordValue::GetAbsolute()) {
    // TODO(***REMOVED***): Reimplement this crude approximation of static position
    //               in order to take horizontal and vertical alignments
    //               into consideration.
    child_box->set_left(GetShrinkToFitWidth());
    child_box->set_top(0);
    return;
  }

  child_box->UpdateSize(layout_params_);

  bool should_collapse_leading_white_space =
      last_non_collapsed_child_box_index_
          // Any space immediately following another collapsible space - even
          // one outside the boundary of the inline containing that space,
          // provided they are both within the same inline formatting context -
          // is collapsed.
          //   http://www.w3.org/TR/css3-text/#white-space-phase-1
          ? child_boxes_[*last_non_collapsed_child_box_index_]
                ->HasTrailingWhiteSpace()
          // A sequence of collapsible spaces at the beginning of a line is
          // removed.
          //   http://www.w3.org/TR/css3-text/#white-space-phase-2
          : should_trim_white_space_ == kShouldTrimWhiteSpace;

  if (should_collapse_leading_white_space) {
    child_box->CollapseLeadingWhiteSpace();
    child_box->UpdateSize(layout_params_);
  }

  // Horizontal margins, borders, and padding are respected between boxes.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  // TODO(***REMOVED***): Implement the above.
  child_box->set_left(GetShrinkToFitWidth());

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
  //               http://www.w3.org/TR/CSS21/visudet.html#line-height
  NOTIMPLEMENTED();

  ReverseChildBoxesByBidiLevels();
  SetLineBoxHeightFromChildBoxes();
  SetChildBoxTopPositions();
  SetChildBoxLeftPositions();
}

float LineBox::GetShrinkToFitWidth() const {
  return child_boxes_.empty()
             ? 0
             : child_boxes_.back()
                   ->GetRightMarginEdgeOffsetFromContainingBlock();
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
  float child_box_pre_collapse_width = child_box->width();
  child_box->CollapseTrailingWhiteSpace();
  child_box->UpdateSize(layout_params_);
  float collapsed_white_space_width =
      child_box_pre_collapse_width - child_box->width();
  DCHECK_GT(collapsed_white_space_width, 0);

  // Adjust the positions of subsequent child boxes.
  for (++child_box_iterator; child_box_iterator != child_boxes_.end();
       ++child_box_iterator) {
    child_box = *child_box_iterator;
    child_box->set_left(child_box->left() - collapsed_white_space_width);
  }
}

// Returns the height of half the given box above the 'middle' of the line box.
float LineBox::GetHeightAboveMiddleAlignmentPoint(Box* box) {
  return (box->height() + x_height_) / 2;
}

void LineBox::ReverseChildBoxesByBidiLevels() {
  // From the highest level found in the text to the lowest odd level on each
  // line, including intermediate levels not actually present in the text,
  // reverse any contiguous sequence of characters that are at that level or
  // higher.
  //  http://unicode.org/reports/tr9/#L2
  const int kInvalidLevel = -1;
  int max_level = 0;
  int min_level = std::numeric_limits<int>::max();

  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;

    int child_level = child_box->GetBidiLevel().value_or(kInvalidLevel);
    if (child_level != kInvalidLevel) {
      if (child_level > max_level) {
        max_level = child_level;
      }
      if (child_level < min_level) {
        min_level = child_level;
      }
    }
  }

  // Reversals only occur down to the lowest odd level.
  if (min_level % 2 == 0) {
    min_level += 1;
  }

  for (int i = max_level; i >= min_level; --i) {
    ReverseChildBoxesMeetingBidiLevelThreshold(i);
  }
}

void LineBox::ReverseChildBoxesMeetingBidiLevelThreshold(int level) {
  // Walk all of the boxes in the line, looking for runs of boxes that have a
  // bidi level greater than or equal to the passed in level. Every run of two
  // or more boxes is reversed.
  int run_count = 0;
  ChildBoxes::iterator run_start;
  int child_level = 0;

  for (ChildBoxes::iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end();) {
    ChildBoxes::iterator current_iterator = child_box_iterator++;
    Box* child_box = *current_iterator;

    child_level = child_box->GetBidiLevel().value_or(child_level);

    // The child's level is greater than or equal to the required level, so it
    // qualifies for reversal.
    if (child_level >= level) {
      if (run_count == 0) {
        run_start = current_iterator;
      }
      ++run_count;
      // The child's level didn't qualify it for reversal. If there was an
      // active run, it has ended, so reverse it.
    } else {
      if (run_count > 1) {
        std::reverse(run_start, current_iterator);
        line_has_bidi_reversed_children_ = true;
      }
      run_count = 0;
    }
  }

  // A qualifying run was found that ran through the end of the children.
  // Reverse it.
  if (run_count > 1) {
    std::reverse(run_start, child_boxes_.end());
    line_has_bidi_reversed_children_ = true;
  }
}

// Loop over the child boxes and set the height_above_baseline_ and used_height_
// such that all child boxes fit. Also updates line_exists_.
void LineBox::SetLineBoxHeightFromChildBoxes() {
  // TODO(***REMOVED***): The minimum height consists of a minimum height above
  //               the baseline and a minimum depth below it, exactly as if
  //               each line box starts with a zero-width inline box with
  //               the element's font and line height properties.
  //               We call that imaginary box a "strut."
  //               http://www.w3.org/TR/CSS21/visudet.html#strut
  line_exists_ = false;
  baseline_offset_from_top_ = 0;
  float height_below_baseline = 0;
  float max_top_used_height = 0;
  // During this loop, the line box height above and below the baseline is
  // established.
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    line_exists_ = line_exists_ || child_box->JustifiesLineExistence();

    // The child box influence on the line box depends on the vertical-align
    // property.
    //   http://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
    const scoped_refptr<cssom::PropertyValue>& vertical_align =
        child_box->computed_style()->vertical_align();
    float child_height_above_line_box_baseline;
    bool update_box_height = false;
    if (vertical_align == cssom::KeywordValue::GetMiddle()) {
      // Align the vertical midpoint of the box with the baseline of the parent
      // box plus half the x-height (height of the 'x' glyph) of the parent.
      child_height_above_line_box_baseline =
          GetHeightAboveMiddleAlignmentPoint(child_box);
      update_box_height = true;
    } else if (vertical_align == cssom::KeywordValue::GetTop()) {
      // Align the top of the aligned subtree with the top of the line box.
      // That means it will never affect the height above the baseline, but it
      // may affect the height below the baseline if this is the tallest child
      // box. We measure the tallest top-aligned box to implement that after
      // this loop.
      max_top_used_height = std::max(max_top_used_height, child_box->height());
    } else if (vertical_align == cssom::KeywordValue::GetBaseline()) {
      // Align the baseline of the box with the baseline of the parent box.
      child_height_above_line_box_baseline =
          child_box->GetBaselineOffsetFromTopMarginEdge();
      update_box_height = true;
    } else {
      NOTREACHED() << "Unsupported vertical_align property value";
    }

    if (update_box_height) {
      baseline_offset_from_top_ = std::max(
          baseline_offset_from_top_, child_height_above_line_box_baseline);

      float child_height_below_baseline =
          child_box->height() - child_height_above_line_box_baseline;
      height_below_baseline =
          std::max(height_below_baseline, child_height_below_baseline);
    }
  }
  // The line box height is the distance between the uppermost box top and the
  // lowermost box bottom.
  //   http://www.w3.org/TR/CSS21/visudet.html#line-height
  used_height_ = baseline_offset_from_top_ + height_below_baseline;
  if (max_top_used_height > used_height_) {
    // Increase the line box height below the baseline to make the largest
    // top-aligned child box fit.
    float additional_height_for_top_box = max_top_used_height - used_height_;
    height_below_baseline += additional_height_for_top_box;
    used_height_ = baseline_offset_from_top_ + height_below_baseline;
  }
}

void LineBox::SetChildBoxTopPositions() {
  // During this loop, the vertical positions of the child boxes are
  // established.
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;

    // The child box top position depends on the vertical-align property.
    //   http://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
    const scoped_refptr<cssom::PropertyValue>& vertical_align =
        child_box->computed_style()->vertical_align();
    float child_top;
    if (vertical_align == cssom::KeywordValue::GetMiddle()) {
      // Align the vertical midpoint of the box with the baseline of the parent
      //  box plus half the x-height (height of the 'x' glyph) of the parent.
      child_top = baseline_offset_from_top_ -
                  GetHeightAboveMiddleAlignmentPoint(child_box);
    } else if (vertical_align == cssom::KeywordValue::GetTop()) {
      // Align the top of the aligned subtree with the top of the line box.
      child_top = 0;
    } else if (vertical_align == cssom::KeywordValue::GetBaseline()) {
      // Align the baseline of the box with the baseline of the parent box.
      child_top = baseline_offset_from_top_ -
                  child_box->GetBaselineOffsetFromTopMarginEdge();
    } else {
      child_top = 0;
      NOTREACHED() << "Unsupported vertical_align property value";
    }
    child_box->set_top(used_top_ + child_top);
  }
}

void LineBox::SetChildBoxLeftPositions() {
  // If any children were reversed as a result of bidi analysis, then the used
  // left positions need to be re-generated.
  if (line_has_bidi_reversed_children_) {
    float used_left = 0;
    for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
         child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
      Box* child_box = *child_box_iterator;
      child_box->set_left(used_left);
      used_left = child_box->GetRightMarginEdgeOffsetFromContainingBlock();
    }
  }

  // Set used left of the child boxes according to the value of text-align.
  //   http://www.w3.org/TR/CSS21/text.html#propdef-text-align
  //

  // When the total width of the inline-level boxes on a line is less than the
  // width of the line box containing them, their horizontal distribution within
  // the line box is determined by the 'text-align' property.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  // So do not shift child boxes when the inline-level boxes already overflow,
  // also do not shift if text-align is 'left'.
  if (text_align_ == cssom::KeywordValue::GetLeft() ||
      layout_params_.containing_block_size.width() < GetShrinkToFitWidth()) {
    return;
  }

  // Calculate the amount to be shifted to the right.
  float additional_left = 0;
  if (text_align_ == cssom::KeywordValue::GetCenter()) {
    additional_left =
        (layout_params_.containing_block_size.width() - GetShrinkToFitWidth()) /
        2;
  } else if (text_align_ == cssom::KeywordValue::GetRight()) {
    additional_left =
        layout_params_.containing_block_size.width() - GetShrinkToFitWidth();
  }

  // Shift all child boxes to the right the calculated amount.
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->set_left(child_box->left() + additional_left);
  }
}

}  // namespace layout
}  // namespace cobalt

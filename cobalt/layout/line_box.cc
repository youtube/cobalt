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
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

// The left edge of a line box touches the left edge of its containing block.
//   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
LineBox::LineBox(float top,
                 const scoped_refptr<cssom::PropertyValue>& line_height,
                 const render_tree::FontMetrics& font_metrics,
                 bool should_collapse_leading_white_space,
                 bool should_collapse_trailing_white_space,
                 const LayoutParams& layout_params,
                 const scoped_refptr<cssom::PropertyValue>& text_align)
    : top_(top),
      line_height_(line_height),
      font_metrics_(font_metrics),
      should_collapse_leading_white_space_(should_collapse_leading_white_space),
      should_collapse_trailing_white_space_(
          should_collapse_trailing_white_space),
      layout_params_(layout_params),
      text_align_(text_align),
      at_end_(false),
      line_exists_(false),
      shrink_to_fit_width_(0),
      height_(0),
      baseline_offset_from_top_(0) {}

bool LineBox::TryBeginUpdateRectAndMaybeSplit(
    Box* child_box, scoped_ptr<Box>* child_box_after_split) {
  DCHECK(!*child_box_after_split);
  DCHECK(!child_box->IsAbsolutelyPositioned());

  if (at_end_) {
    return false;
  }

  UpdateSizePreservingTrailingWhiteSpace(child_box);
  float available_width = GetAvailableWidth();

  // Horizontal margins, borders, and padding are respected between boxes.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  if (child_box->GetMarginBoxWidth() <= available_width) {
    BeginUpdatePosition(child_box);
    return true;
  }

  // We must have reached the end of the line.
  at_end_ = true;

  if (should_collapse_trailing_white_space_) {
    child_box->SetShouldCollapseTrailingWhiteSpace(true);
    child_box->UpdateSize(layout_params_);

    // A sequence of collapsible spaces at the end of a line is removed.
    //   http://www.w3.org/TR/css3-text/#white-space-phase-2
    if (child_box->IsCollapsed()) {
      CollapseTrailingWhiteSpace();
    }

    // Try to place the child again, as the white space collapsing may have
    // freed up some space.
    available_width = GetAvailableWidth();
    if (child_box->GetMarginBoxWidth() <= available_width) {
      BeginUpdatePosition(child_box);
      return true;
    }
  }

  // If an inline box cannot be split (e.g., if the inline box contains
  // a single character, or language specific word breaking rules disallow
  // a break within the inline box), then the inline box overflows the line
  // box.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  //
  // Allow the overflow if the line box has no children that justify line
  // existence. This prevents perpetual rejection of child boxes that cannot
  // be broken in a way that makes them fit the line.
  bool allow_overflow = !line_exists_;

  // When an inline box exceeds the width of a line box, it is split.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  *child_box_after_split =
      child_box->TrySplitAt(available_width, allow_overflow);

  // A split occurred, need to re-measure the child box.
  if (*child_box_after_split != NULL) {
    BeginUpdateRectAndMaybeOverflow(child_box);
    DCHECK(child_box->GetMarginBoxWidth() <= available_width || allow_overflow);
    return true;
  }

  if (allow_overflow) {
    BeginUpdatePosition(child_box);
    return true;
  }

  return false;
}

void LineBox::BeginUpdateRectAndMaybeOverflow(Box* child_box) {
  DCHECK(!child_box->IsAbsolutelyPositioned());

  UpdateSizePreservingTrailingWhiteSpace(child_box);
  BeginUpdatePosition(child_box);
}

void LineBox::BeginEstimateStaticPosition(Box* child_box) {
  DCHECK(child_box->IsAbsolutelyPositioned());

  // TODO(***REMOVED***): Reimplement this crude approximation of static position
  //               in order to take horizontal and vertical alignments
  //               into consideration (for inline-level children) and line
  //               height (for block-level children).
  child_box->set_left(shrink_to_fit_width_);
  child_box->set_top(0);
}

void LineBox::EndUpdates() {
  at_end_ = true;

  // A sequence of collapsible spaces at the end of a line is removed.
  //   http://www.w3.org/TR/css3-text/#white-space-phase-2
  if (should_collapse_trailing_white_space_) {
    CollapseTrailingWhiteSpace();
  }

  ReverseChildBoxesByBidiLevels();
  UpdateChildBoxLeftPositions();
  SetLineBoxHeightFromChildBoxes();
  UpdateChildBoxTopPositions();
}

bool LineBox::HasLeadingWhiteSpace() const {
  return first_non_collapsed_child_box_index_ &&
         child_boxes_[*first_non_collapsed_child_box_index_]
             ->HasLeadingWhiteSpace();
}

bool LineBox::HasTrailingWhiteSpace() const {
  return last_non_collapsed_child_box_index_ &&
         child_boxes_[*last_non_collapsed_child_box_index_]
             ->HasTrailingWhiteSpace();
}

bool LineBox::IsCollapsed() const {
  return !first_non_collapsed_child_box_index_;
}

float LineBox::GetAvailableWidth() {
  return layout_params_.containing_block_size.width() - shrink_to_fit_width_;
}

void LineBox::UpdateSizePreservingTrailingWhiteSpace(Box* child_box) {
  child_box->SetShouldCollapseLeadingWhiteSpace(
      ShouldCollapseLeadingWhiteSpaceInNextChildBox());
  child_box->SetShouldCollapseTrailingWhiteSpace(false);
  child_box->UpdateSize(layout_params_);
}

bool LineBox::ShouldCollapseLeadingWhiteSpaceInNextChildBox() const {
  return last_non_collapsed_child_box_index_
             // Any space immediately following another collapsible space - even
             // one outside the boundary of the inline containing that space,
             // provided they are both within the same inline formatting context
             // - is collapsed.
             //   http://www.w3.org/TR/css3-text/#white-space-phase-1
             ? child_boxes_[*last_non_collapsed_child_box_index_]
                   ->HasTrailingWhiteSpace()
             // A sequence of collapsible spaces at the beginning of a line is
             // removed.
             //   http://www.w3.org/TR/css3-text/#white-space-phase-2
             : should_collapse_leading_white_space_;
}

void LineBox::CollapseTrailingWhiteSpace() {
  DCHECK(at_end_);

  if (!HasTrailingWhiteSpace()) {
    return;
  }

  // A white space between child boxes is already collapsed as a result
  // of calling |UpdateSizePreservingTrailingWhiteSpace|. Now that we know that
  // the end of the line is reached, we are ready to collapse the trailing white
  // space in the last non-collapsed child box (all fully collapsed child boxes
  // at the end of the line are treated as non-existent for the purposes
  // of collapsing).
  //
  // TODO(***REMOVED***): This should be a loop that goes backward. This is because
  //               the trailing white space collapsing may turn the last
  //               non-collapsed child into a fully collapsed child.
  Box* last_non_collapsed_child_box =
      child_boxes_[*last_non_collapsed_child_box_index_];
  float child_box_pre_collapse_width = last_non_collapsed_child_box->width();
  last_non_collapsed_child_box->SetShouldCollapseTrailingWhiteSpace(true);
  last_non_collapsed_child_box->UpdateSize(layout_params_);
  float collapsed_white_space_width =
      child_box_pre_collapse_width - last_non_collapsed_child_box->width();
  DCHECK_GT(collapsed_white_space_width, 0);

  shrink_to_fit_width_ -= collapsed_white_space_width;
}

void LineBox::BeginUpdatePosition(Box* child_box) {
  if (!child_box->IsCollapsed()) {
    if (!first_non_collapsed_child_box_index_) {
      first_non_collapsed_child_box_index_ = child_boxes_.size();
    }
    last_non_collapsed_child_box_index_ = child_boxes_.size();
  }

  // Horizontal margins, borders, and padding are respected between boxes.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  shrink_to_fit_width_ += child_box->GetMarginBoxWidth();

  line_exists_ = line_exists_ || child_box->JustifiesLineExistence();

  child_boxes_.push_back(child_box);
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
      }
      run_count = 0;
    }
  }

  // A qualifying run was found that ran through the end of the children.
  // Reverse it.
  if (run_count > 1) {
    std::reverse(run_start, child_boxes_.end());
  }
}

void LineBox::UpdateChildBoxLeftPositions() {
  float horizontal_offset_due_alignment = 0;
  if (text_align_ == cssom::KeywordValue::GetLeft() ||
      layout_params_.containing_block_size.width() < shrink_to_fit_width_) {
    // When the total width of the inline-level boxes on a line is less than
    // the width of the line box containing them, their horizontal distribution
    // within the line box is determined by the "text-align" property.
    //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
    //
    // So do not shift child boxes when the inline-level boxes already overflow,
    // also do not shift if text-align is "left".
  } else if (text_align_ == cssom::KeywordValue::GetCenter()) {
    horizontal_offset_due_alignment =
        (layout_params_.containing_block_size.width() - shrink_to_fit_width_) /
        2;
  } else if (text_align_ == cssom::KeywordValue::GetRight()) {
    horizontal_offset_due_alignment =
        layout_params_.containing_block_size.width() - shrink_to_fit_width_;
  } else {
    NOTREACHED() << "Unknown value of \"text-align\".";
  }

  // Set used value of "left" of the child boxes according to the value
  // of "text-align", vaguely specified by
  // http://www.w3.org/TR/CSS21/text.html#propdef-text-align.
  float child_box_left = 0;
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->set_left(child_box_left + horizontal_offset_due_alignment);
    child_box_left = child_box->GetRightMarginEdgeOffsetFromContainingBlock();
  }
}

// Loops over the child boxes and sets the |baseline_offset_from_top_|
// and |height_| such that all child boxes fit.
void LineBox::SetLineBoxHeightFromChildBoxes() {
  // The minimum height consists of a minimum height above the baseline and
  // a minimum depth below it, exactly as if each line box starts with
  // a zero-width inline box with the element's font and line height properties.
  // We call that imaginary box a "strut."
  //   http://www.w3.org/TR/CSS21/visudet.html#strut
  UsedLineHeightProvider used_line_height_provider(font_metrics_);
  line_height_->Accept(&used_line_height_provider);
  baseline_offset_from_top_ =
      used_line_height_provider.baseline_offset_from_top();
  float baseline_offset_from_bottom =
      used_line_height_provider.baseline_offset_from_bottom();

  float child_max_margin_box_height = 0;

  // During this loop, the line box height above and below the baseline is
  // established.
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;

    // The child box influence on the line box depends on the vertical-align
    // property.
    //   http://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
    const scoped_refptr<cssom::PropertyValue>& vertical_align =
        child_box->computed_style()->vertical_align();
    float baseline_offset_from_child_top_margin_edge;
    bool update_height = false;
    if (vertical_align == cssom::KeywordValue::GetMiddle()) {
      // Align the vertical midpoint of the box with the baseline of the parent
      // box plus half the x-height (height of the 'x' glyph) of the parent.
      baseline_offset_from_child_top_margin_edge =
          GetHeightAboveMiddleAlignmentPoint(child_box);
      update_height = true;
    } else if (vertical_align == cssom::KeywordValue::GetTop()) {
      // Align the top of the aligned subtree with the top of the line box.
      // That means it will never affect the height above the baseline, but it
      // may affect the height below the baseline if this is the tallest child
      // box. We measure the tallest top-aligned box to implement that after
      // this loop.
      child_max_margin_box_height = std::max(child_max_margin_box_height,
                                             child_box->GetMarginBoxHeight());
    } else if (vertical_align == cssom::KeywordValue::GetBaseline()) {
      // Align the baseline of the box with the baseline of the parent box.
      baseline_offset_from_child_top_margin_edge =
          child_box->GetBaselineOffsetFromTopMarginEdge();
      update_height = true;
    } else {
      NOTREACHED() << "Unknown value of \"vertical-align\".";
    }

    if (update_height) {
      baseline_offset_from_top_ =
          std::max(baseline_offset_from_top_,
                   baseline_offset_from_child_top_margin_edge);

      float baseline_offset_from_child_bottom_margin_edge =
          child_box->GetMarginBoxHeight() -
          baseline_offset_from_child_top_margin_edge;
      baseline_offset_from_bottom =
          std::max(baseline_offset_from_bottom,
                   baseline_offset_from_child_bottom_margin_edge);
    }
  }
  // The line box height is the distance between the uppermost box top and the
  // lowermost box bottom.
  //   http://www.w3.org/TR/CSS21/visudet.html#line-height
  height_ = baseline_offset_from_top_ + baseline_offset_from_bottom;
  if (child_max_margin_box_height > height_) {
    // Increase the line box height below the baseline to make the largest
    // top-aligned child box fit.
    baseline_offset_from_bottom += child_max_margin_box_height - height_;
    height_ = baseline_offset_from_top_ + baseline_offset_from_bottom;
  }
}

void LineBox::UpdateChildBoxTopPositions() {
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
    child_box->set_top(top_ + child_top);
  }
}

// Returns the height of half the given box above the 'middle' of the line box.
float LineBox::GetHeightAboveMiddleAlignmentPoint(Box* child_box) {
  return (child_box->height() + font_metrics_.x_height) / 2;
}

}  // namespace layout
}  // namespace cobalt

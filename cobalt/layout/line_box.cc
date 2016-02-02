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
#include <limits>

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/math.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

// The left edge of a line box touches the left edge of its containing block.
//   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
LineBox::LineBox(float top, bool position_children_relative_to_baseline,
                 const scoped_refptr<cssom::PropertyValue>& line_height,
                 const render_tree::FontMetrics& font_metrics,
                 bool should_collapse_leading_white_space,
                 bool should_collapse_trailing_white_space,
                 const LayoutParams& layout_params,
                 const scoped_refptr<cssom::PropertyValue>& text_align,
                 const scoped_refptr<cssom::PropertyValue>& white_space,
                 float indent_offset, float ellipsis_width)
    : top_(top),
      position_children_relative_to_baseline_(
          position_children_relative_to_baseline),
      line_height_(line_height),
      font_metrics_(font_metrics),
      should_collapse_leading_white_space_(should_collapse_leading_white_space),
      should_collapse_trailing_white_space_(
          should_collapse_trailing_white_space),
      layout_params_(layout_params),
      text_align_(text_align),
      is_text_wrapping_disabled_(white_space ==
                                     cssom::KeywordValue::GetNoWrap() ||
                                 white_space == cssom::KeywordValue::GetPre()),
      indent_offset_(indent_offset),
      ellipsis_width_(ellipsis_width),
      at_end_(false),
      line_exists_(false),
      shrink_to_fit_width_(indent_offset_),
      height_(0),
      baseline_offset_from_top_(0),
      is_ellipsis_placed_(false),
      placed_ellipsis_offset_(0) {}

bool LineBox::TryBeginUpdateRectAndMaybeSplit(
    Box* child_box, scoped_refptr<Box>* child_box_after_split) {
  DCHECK(!*child_box_after_split);
  DCHECK(!child_box->IsAbsolutelyPositioned());

  if (at_end_) {
    return false;
  }

  // If the white-space style of the line box disables text wrapping then always
  // allow overflow.
  // NOTE: While this works properly for typical use cases, it does not allow
  // child boxes to enable or disable wrapping, so this is not a robust
  // long-term solution. However, it handles ***REMOVED***'s current needs.
  // TODO(***REMOVED***): Implement the full line breaking algorithm (tracked as
  // b/25504189).
  if (is_text_wrapping_disabled_) {
    BeginUpdateRectAndMaybeOverflow(child_box);
    return true;
  }

  UpdateSizePreservingTrailingWhiteSpace(child_box);

  float available_width = GetAvailableWidth();

  // Horizontal margins, borders, and padding are respected between boxes.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
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
    //   https://www.w3.org/TR/css3-text/#white-space-phase-2
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
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  //
  // Allow the overflow if the line box has no children that justify line
  // existence. This prevents perpetual rejection of child boxes that cannot
  // be broken in a way that makes them fit the line.
  bool allow_overflow = !line_exists_;

  // When an inline box exceeds the width of a line box, it is split.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
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

  // The term "static position" (of an element) refers, roughly, to the position
  // an element would have had in the normal flow. More precisely:
  //
  // The static-position containing block is the containing block of a
  // hypothetical box that would have been the first box of the element if its
  // specified 'position' value had been 'static'.
  //
  // The static position for 'left' is the distance from the left edge of the
  // containing block to the left margin edge of a hypothetical box that would
  // have been the first box of the element if its 'position' property had been
  // 'static' and 'float' had been 'none'. The value is negative if the
  // hypothetical box is to the left of the containing block.
  //   https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width

  // For the purposes of this section and the next, the term "static position"
  // (of an element) refers, roughly, to the position an element would have had
  // in the normal flow. More precisely, the static position for 'top' is the
  // distance from the top edge of the containing block to the top margin edge
  // of a hypothetical box that would have been the first box of the element if
  // its specified 'position' value had been 'static'.
  //   https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-height

  switch (child_box->GetLevel()) {
    case Box::kInlineLevel:
      child_box->set_left(shrink_to_fit_width_);
      break;
    case Box::kBlockLevel:
      child_box->set_left(0);
      break;
    default:
      NOTREACHED();
      break;
  }
  child_box->set_top(0);
}

void LineBox::EndUpdates() {
  at_end_ = true;

  // A sequence of collapsible spaces at the end of a line is removed.
  //   https://www.w3.org/TR/css3-text/#white-space-phase-2
  if (should_collapse_trailing_white_space_) {
    CollapseTrailingWhiteSpace();
  }

  ReverseChildBoxesByBidiLevels();
  UpdateChildBoxLeftPositions();
  SetLineBoxHeightFromChildBoxes();
  UpdateChildBoxTopPositions();
  MaybePlaceEllipsis();
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

bool LineBox::IsEllipsisPlaced() const { return is_ellipsis_placed_; }

math::Vector2dF LineBox::GetEllipsisCoordinates() const {
  return math::Vector2dF(placed_ellipsis_offset_,
                         top_ + baseline_offset_from_top_);
}

float LineBox::GetAvailableWidth() const {
  return RoundToFixedPointPrecision(
      layout_params_.containing_block_size.width() - shrink_to_fit_width_);
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
             //   https://www.w3.org/TR/css3-text/#white-space-phase-1
             ? child_boxes_[*last_non_collapsed_child_box_index_]
                   ->HasTrailingWhiteSpace()
             // A sequence of collapsible spaces at the beginning of a line is
             // removed.
             //   https://www.w3.org/TR/css3-text/#white-space-phase-2
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

  // If this child has a trailing line break, then we've reached the end of this
  // line. Nothing more can be added to it.
  if (child_box->HasTrailingLineBreak()) {
    at_end_ = true;
  }

  // Horizontal margins, borders, and padding are respected between boxes.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
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
    //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
    //
    // So do not shift child boxes when the inline-level boxes already overflow,
    // also do not shift if text-align is "left".
  } else if (text_align_ == cssom::KeywordValue::GetCenter()) {
    horizontal_offset_due_alignment = GetAvailableWidth() / 2;
  } else if (text_align_ == cssom::KeywordValue::GetRight()) {
    horizontal_offset_due_alignment = GetAvailableWidth();
  } else {
    NOTREACHED() << "Unknown value of \"text-align\".";
  }

  // Set the first child box position to the indent offset, which is treated as
  // a margin applied to the start edge of the line box, specified by
  // https://www.w3.org/TR/CSS21/text.html#propdef-text-indent,
  // and offset this position by the the value calculated from "text-align",
  // which is vaguely specified by
  // https://www.w3.org/TR/CSS21/text.html#propdef-text-align.
  float child_box_left = indent_offset_ + horizontal_offset_due_alignment;
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->set_left(child_box_left);
    child_box_left =
        child_box->GetMarginBoxRightEdgeOffsetFromContainingBlock();
  }
}

// Loops over the child boxes and sets the |baseline_offset_from_top_|
// and |height_| such that all child boxes fit.
void LineBox::SetLineBoxHeightFromChildBoxes() {
  // The minimum height consists of a minimum height above the baseline and
  // a minimum depth below it, exactly as if each line box starts with
  // a zero-width inline box with the element's font and line height properties.
  // We call that imaginary box a "strut."
  //   https://www.w3.org/TR/CSS21/visudet.html#strut
  UsedLineHeightProvider used_line_height_provider(font_metrics_);
  line_height_->Accept(&used_line_height_provider);

  baseline_offset_from_top_ =
      used_line_height_provider.baseline_offset_from_top();
  float baseline_offset_from_bottom =
      used_line_height_provider.baseline_offset_from_bottom();

  float max_top_aligned_height = 0;
  float max_bottom_aligned_height = 0;

  // During this loop, the line box height above and below the baseline is
  // established.
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;

    // The child box influence on the line box depends on the vertical-align
    // property.
    //   https://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
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
      float child_height = child_box->GetInlineLevelBoxHeight();
      // If there previously was a taller bottom-aligned box, then this box does
      // not influence the line box height or baseline.
      if (child_height > max_bottom_aligned_height) {
        max_top_aligned_height = std::max(max_top_aligned_height, child_height);
      }
    } else if (vertical_align == cssom::KeywordValue::GetBottom()) {
      // Align the bottom of the aligned subtree with the bottom of the line
      // box.
      float child_height = child_box->GetInlineLevelBoxHeight();
      // If there previously was a taller top-aligned box, then this box does
      // not influence the line box height or baseline.
      if (child_height > max_top_aligned_height) {
        max_bottom_aligned_height =
            std::max(max_bottom_aligned_height, child_height);
      }
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
          child_box->GetInlineLevelBoxHeight() -
          baseline_offset_from_child_top_margin_edge;
      baseline_offset_from_bottom =
          std::max(baseline_offset_from_bottom,
                   baseline_offset_from_child_bottom_margin_edge);
    }
  }
  // The line box height is the distance between the uppermost box top and the
  // lowermost box bottom.
  //   https://www.w3.org/TR/CSS21/visudet.html#line-height
  height_ = baseline_offset_from_top_ + baseline_offset_from_bottom;

  // In case they are aligned 'top' or 'bottom', they must be aligned so as to
  // minimize the line box height. If such boxes are tall enough, there are
  // multiple solutions and CSS 2.1 does not define the position of the line
  // box's baseline.
  //   https://www.w3.org/TR/CSS21/visudet.html#line-height
  // For the cases where CSS 2.1 does not specify the baseline position, the
  // code below matches the behavior or WebKit and Blink.
  if (max_top_aligned_height < max_bottom_aligned_height) {
    if (max_top_aligned_height > height_) {
      // The bottom aligned box is tallest, but there should also be enough
      // space below the baseline for the shorter top aligned box.
      baseline_offset_from_bottom =
          max_top_aligned_height - baseline_offset_from_top_;
    }
    if (max_bottom_aligned_height > height_) {
      // Increase the line box height above the baseline to make the largest
      // bottom-aligned child box fit.
      height_ = max_bottom_aligned_height;
      baseline_offset_from_top_ = height_ - baseline_offset_from_bottom;
    }
  } else {
    if (max_bottom_aligned_height > height_) {
      // The top aligned box is tallest, but there should also be enough
      // space above the baseline for the shorter bottom aligned box.
      baseline_offset_from_top_ =
          max_bottom_aligned_height - baseline_offset_from_bottom;
    }
    if (max_top_aligned_height > height_) {
      // Increase the line box height below the baseline to make the largest
      // top-aligned child box fit.
      height_ = max_top_aligned_height;
      baseline_offset_from_bottom = height_ - baseline_offset_from_top_;
    }
  }
}

void LineBox::UpdateChildBoxTopPositions() {
  float top_offset = top_;
  if (position_children_relative_to_baseline_) {
    // For InlineContainerBoxes, the children have to be aligned to the baseline
    // so that the vertical positioning can be consistent with the box position
    // with line-height and different font sizes.
    top_offset -= baseline_offset_from_top_;
  }
  // During this loop, the vertical positions of the child boxes are
  // established.
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;

    // The child box top position depends on the vertical-align property.
    //   https://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
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
    } else if (vertical_align == cssom::KeywordValue::GetBottom()) {
      // Align the bottom of the aligned subtree with the bottom of the line
      // box.
      child_top = height_ - child_box->GetInlineLevelBoxHeight();
    } else if (vertical_align == cssom::KeywordValue::GetBaseline()) {
      // Align the baseline of the box with the baseline of the parent box.
      child_top = baseline_offset_from_top_ -
                  child_box->GetBaselineOffsetFromTopMarginEdge();
    } else {
      child_top = 0;
      NOTREACHED() << "Unsupported vertical_align property value";
    }
    child_box->set_top(top_offset + child_top +
                       child_box->GetInlineLevelTopMargin());
  }
}

void LineBox::MaybePlaceEllipsis() {
  // Check to see if an ellipsis should be placed, which is only the case when a
  // non-zero width ellipsis is available and the line box has overflowed the
  // containing block.
  if (ellipsis_width_ > 0 &&
      shrink_to_fit_width_ > layout_params_.containing_block_size.width()) {
    // Determine the preferred offset for the ellipsis:
    // - The preferred offset defaults to the end line box edge minus the width
    //   of the ellipsis, which ensures that the full ellipsis appears within
    //   the unclipped portion of the line.
    // - However, if there is insufficient space for the ellipsis, the preferred
    //   offset is set to 0, rather than a negative value, thereby ensuring that
    //   the ellipsis itself is clipped at the end line box edge.
    // https://www.w3.org/TR/css3-ui/#propdef-text-overflow
    float preferred_offset = std::max<float>(
        layout_params_.containing_block_size.width() - ellipsis_width_, 0);

    // Whether or not a character or atomic inline-level element has been
    // encountered within the boxes already checked on the line. The ellipsis
    // cannot be placed at an offset that precedes the first character or atomic
    // inline-level element on a line.
    // https://www.w3.org/TR/css3-ui/#propdef-text-overflow
    bool is_placement_requirement_met = false;

    // Walk each box within the line in order attempting to place the ellipsis
    // and update the box's ellipsis state. Even after the ellipsis is placed,
    // subsequent boxes must still be processed, as their state my change as
    // a result of having an ellipsis preceding them on the line.
    for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
         child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
      Box* child_box = *child_box_iterator;
      child_box->TryPlaceEllipsisOrProcessPlacedEllipsis(
          preferred_offset, &is_placement_requirement_met, &is_ellipsis_placed_,
          &placed_ellipsis_offset_);
    }
  }
}

// Returns the height of half the given box above the 'middle' of the line box.
float LineBox::GetHeightAboveMiddleAlignmentPoint(Box* child_box) {
  return (child_box->height() + font_metrics_.x_height()) / 2.0f;
}

}  // namespace layout
}  // namespace cobalt

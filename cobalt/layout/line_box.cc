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

#include "cobalt/base/math.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

// The left edge of a line box touches the left edge of its containing block.
//   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
LineBox::LineBox(LayoutUnit top, bool position_children_relative_to_baseline,
                 const scoped_refptr<cssom::PropertyValue>& line_height,
                 const render_tree::FontMetrics& font_metrics,
                 bool should_collapse_leading_white_space,
                 bool should_collapse_trailing_white_space,
                 const LayoutParams& layout_params,
                 BaseDirection base_direction,
                 const scoped_refptr<cssom::PropertyValue>& text_align,
                 const scoped_refptr<cssom::PropertyValue>& white_space,
                 const scoped_refptr<cssom::PropertyValue>& font_size,
                 LayoutUnit indent_offset, LayoutUnit ellipsis_width)
    : top_(top),
      position_children_relative_to_baseline_(
          position_children_relative_to_baseline),
      line_height_(line_height),
      font_metrics_(font_metrics),
      should_collapse_leading_white_space_(should_collapse_leading_white_space),
      should_collapse_trailing_white_space_(
          should_collapse_trailing_white_space),
      layout_params_(layout_params),
      base_direction_(base_direction),
      text_align_(text_align),
      font_size_(font_size),
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

  LayoutUnit available_width = GetAvailableWidth();

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
  *child_box_after_split = child_box->TrySplitAt(
      available_width, should_collapse_trailing_white_space_, allow_overflow);

  // A split occurred, need to re-measure the child box.
  if (*child_box_after_split != NULL) {
    BeginUpdateRectAndMaybeOverflow(child_box);

    // If trailing white space is being collapsed, then the child box can exceed
    // the available width prior to white space being collapsed. So the DCHECK
    // is only valid if the box's whitespace is collapsed prior to it.
    // if (should_collapse_trailing_white_space_) {
    //  CollapseTrailingWhiteSpace();
    //}
    // TODO(***REMOVED***): Re-enable the DCHECK() below after implementing b/27134223.
    // DCHECK(child_box->GetMarginBoxWidth() <= available_width ||
    //        allow_overflow);
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
      child_box->set_left(LayoutUnit());
      break;
    default:
      NOTREACHED();
      break;
  }
  child_box->set_top(LayoutUnit());
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
  return math::Vector2dF(placed_ellipsis_offset_.toFloat(),
                         (top_ + baseline_offset_from_top_).toFloat());
}

LayoutUnit LineBox::GetAvailableWidth() const {
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
  LayoutUnit child_box_pre_collapse_width =
      last_non_collapsed_child_box->width();
  last_non_collapsed_child_box->SetShouldCollapseTrailingWhiteSpace(true);
  last_non_collapsed_child_box->UpdateSize(layout_params_);
  LayoutUnit collapsed_white_space_width =
      child_box_pre_collapse_width - last_non_collapsed_child_box->width();
  DCHECK_GT(collapsed_white_space_width, LayoutUnit());

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
  LayoutUnit horizontal_offset;

  // Determine the horizontal offset to apply to the child boxes from the
  // horizontal alignment.
  HorizontalAlignment horizontal_alignment = ComputeHorizontalAlignment();
  switch (horizontal_alignment) {
    case kLeftHorizontalAlignment:
      // The horizontal alignment is to the left, so no offset needs to be
      // applied based on the alignment. This places all of the available space
      // to the right of the boxes.
      break;
    case kCenterHorizontalAlignment:
      // The horizontal alignment is to the center, so offset by half of the
      // available width. This places half of the available width on each side
      // of the boxes.
      horizontal_offset = GetAvailableWidth() / 2;
      break;
    case kRightHorizontalAlignment:
      // The horizontal alignment is to the right, so offset by the full
      // available width. This places all of the available space to the left of
      // the boxes.
      horizontal_offset = GetAvailableWidth();
      break;
  }

  // Determine the horizontal offset to add to the child boxes from the indent
  // offset (https://www.w3.org/TR/CSS21/text.html#propdef-text-indent), which
  // is treated as a margin applied to the start edge of the line box. In the
  // case where the start edge is on the right, there is no offset to add, as
  // it was already included from the GetAvailableWidth() logic above.
  //
  // To add to this, the indent offset was added to |shrink_to_fit_width_| when
  // the line box was created. The above logic serves to subtract half of the
  // indent offset when the alignment is centered, and the full indent offset
  // when the alignment is to the right. Re-adding the indent offset in the case
  // where the base direction is LTR causes the indent to shift the boxes to the
  // right. Not adding it in the case where the base direction is RTL causes the
  // indent to shift the boxes to the left.
  //
  // Here are the 6 cases and the final indent offset they produce:
  // Left Align   + LTR => indent_offset
  // Center Align + LTR => indent_offset / 2
  // Right Align  + LTR => 0
  // Left Align   + RTL => 0
  // Center Align + RTL => -indent_offset / 2
  // Right Align  + RTL => -indent_offset
  if (base_direction_ != kRightToLeftBaseDirection) {
    horizontal_offset += indent_offset_;
  }

  // Set the first child box left position to the horizontal offset. This
  // results in all boxes being shifted by that offset.
  LayoutUnit child_box_left(horizontal_offset);
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
  UsedLineHeightProvider used_line_height_provider(font_metrics_, font_size_);
  line_height_->Accept(&used_line_height_provider);

  baseline_offset_from_top_ =
      used_line_height_provider.baseline_offset_from_top();
  LayoutUnit baseline_offset_from_bottom =
      used_line_height_provider.baseline_offset_from_bottom();

  LayoutUnit max_top_aligned_height;
  LayoutUnit max_bottom_aligned_height;

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
    LayoutUnit baseline_offset_from_child_top_margin_edge;
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
      LayoutUnit child_height = child_box->GetInlineLevelBoxHeight();
      // If there previously was a taller bottom-aligned box, then this box does
      // not influence the line box height or baseline.
      if (child_height > max_bottom_aligned_height) {
        max_top_aligned_height = std::max(max_top_aligned_height, child_height);
      }
    } else if (vertical_align == cssom::KeywordValue::GetBottom()) {
      // Align the bottom of the aligned subtree with the bottom of the line
      // box.
      LayoutUnit child_height = child_box->GetInlineLevelBoxHeight();
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

      LayoutUnit baseline_offset_from_child_bottom_margin_edge =
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
  LayoutUnit top_offset = top_;
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
    LayoutUnit child_top;
    if (vertical_align == cssom::KeywordValue::GetMiddle()) {
      // Align the vertical midpoint of the box with the baseline of the parent
      //  box plus half the x-height (height of the 'x' glyph) of the parent.
      child_top = baseline_offset_from_top_ -
                  GetHeightAboveMiddleAlignmentPoint(child_box);
    } else if (vertical_align == cssom::KeywordValue::GetTop()) {
      // Align the top of the aligned subtree with the top of the line box.
      // Nothing to do child_top is already zero
    } else if (vertical_align == cssom::KeywordValue::GetBottom()) {
      // Align the bottom of the aligned subtree with the bottom of the line
      // box.
      child_top = height_ - child_box->GetInlineLevelBoxHeight();
    } else if (vertical_align == cssom::KeywordValue::GetBaseline()) {
      // Align the baseline of the box with the baseline of the parent box.
      child_top = baseline_offset_from_top_ -
                  child_box->GetBaselineOffsetFromTopMarginEdge();
    } else {
      NOTREACHED() << "Unsupported vertical_align property value";
    }
    child_box->set_top(top_offset + child_top +
                       child_box->GetInlineLevelTopMargin());
  }
}

void LineBox::MaybePlaceEllipsis() {
  // Check to see if an ellipsis should be placed, which only occurs when the
  // ellipsis has a positive width and the content has overflowed the line.
  if (ellipsis_width_ <= LayoutUnit() ||
      shrink_to_fit_width_ <= layout_params_.containing_block_size.width()) {
    return;
  }

  // Determine the preferred start edge offset for the ellipsis, which is the
  // offset at which the ellipsis begins clipping content on the line.
  // - If the ellipsis fully fits on the line, then the preferred end edge for
  //   the ellipsis is the line's end edge. Therefore the preferred ellipsis
  //   start edge is simply the end edge offset by the ellipsis's width.
  // - However, if there is insufficient space for the ellipsis to fully fit on
  //   the line, then the ellipsis should overflow the line's end edge, rather
  //   than its start edge. As a result, the preferred ellipsis start edge
  //   offset is simply the line's start edge.
  // https://www.w3.org/TR/css3-ui/#propdef-text-overflow
  LayoutUnit preferred_start_edge_offset;
  if (ellipsis_width_ <= layout_params_.containing_block_size.width()) {
    preferred_start_edge_offset =
        base_direction_ == kRightToLeftBaseDirection
            ? ellipsis_width_
            : layout_params_.containing_block_size.width() - ellipsis_width_;
  } else {
    preferred_start_edge_offset =
        base_direction_ == kRightToLeftBaseDirection
            ? layout_params_.containing_block_size.width()
            : LayoutUnit();
  }

  // Whether or not a character or atomic inline-level element has been
  // encountered within the boxes already checked on the line. The ellipsis
  // cannot be placed at an offset that precedes the first character or atomic
  // inline-level element on a line.
  // https://www.w3.org/TR/css3-ui/#propdef-text-overflow
  bool is_placement_requirement_met = false;

  // The start edge offset at which the ellipsis was eventually placed. This
  // will be set by TryPlaceEllipsisOrProcessPlacedEllipsis() within one of the
  // child boxes.
  // NOTE: While this is is guaranteed to be set later, initializing it here
  // keeps compilers from complaining about it being an uninitialized variable
  // below.
  LayoutUnit placed_start_edge_offset;

  // Walk each box within the line in base direction order attempting to place
  // the ellipsis and update the box's ellipsis state. Even after the ellipsis
  // is placed, subsequent boxes must still be processed, as their state may
  // change as a result of having an ellipsis preceding them on the line.
  if (base_direction_ == kRightToLeftBaseDirection) {
    for (ChildBoxes::reverse_iterator child_box_iterator =
             child_boxes_.rbegin();
         child_box_iterator != child_boxes_.rend(); ++child_box_iterator) {
      Box* child_box = *child_box_iterator;
      child_box->TryPlaceEllipsisOrProcessPlacedEllipsis(
          base_direction_, preferred_start_edge_offset,
          &is_placement_requirement_met, &is_ellipsis_placed_,
          &placed_start_edge_offset);
    }
  } else {
    for (ChildBoxes::iterator child_box_iterator = child_boxes_.begin();
         child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
      Box* child_box = *child_box_iterator;
      child_box->TryPlaceEllipsisOrProcessPlacedEllipsis(
          base_direction_, preferred_start_edge_offset,
          &is_placement_requirement_met, &is_ellipsis_placed_,
          &placed_start_edge_offset);
    }
  }

  // Set |placed_ellipsis_offset_|. This is the offset at which an ellipsis will
  // be rendered and represents the left edge of the placed ellipsis.
  // In the case where the line's base direction is right-to-left and the start
  // edge is the right edge of the ellipsis, the width of the ellipsis must be
  // subtracted to produce the left edge of the ellipsis.
  placed_ellipsis_offset_ = base_direction_ == kRightToLeftBaseDirection
                                ? placed_start_edge_offset - ellipsis_width_
                                : placed_start_edge_offset;
}

// Returns the height of half the given box above the 'middle' of the line box.
LayoutUnit LineBox::GetHeightAboveMiddleAlignmentPoint(Box* child_box) {
  return (child_box->GetInlineLevelBoxHeight() +
          LayoutUnit(font_metrics_.x_height())) /
         2.0f;
}

LineBox::HorizontalAlignment LineBox::ComputeHorizontalAlignment() const {
  // When the total width of the inline-level boxes on a line is less than
  // the width of the line box containing them, their horizontal distribution
  // within the line box is determined by the "text-align" property.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting.
  // text-align is vaguely specified by
  // https://www.w3.org/TR/css-text-3/#text-align.

  HorizontalAlignment horizontal_alignment;
  if (layout_params_.containing_block_size.width() < shrink_to_fit_width_) {
    // If the content has overflowed the line, then do not base horizontal
    // alignment on the value of text-align. Instead, simply rely upon the base
    // direction of the line, so that inline-level content begins at the
    // starting edge of the line.
    horizontal_alignment = base_direction_ == kRightToLeftBaseDirection
                               ? kRightHorizontalAlignment
                               : kLeftHorizontalAlignment;
  } else if (text_align_ == cssom::KeywordValue::GetStart()) {
    // If the value of text-align is start, then inline-level content is aligned
    // to the start edge of the line box.
    horizontal_alignment = base_direction_ == kRightToLeftBaseDirection
                               ? kRightHorizontalAlignment
                               : kLeftHorizontalAlignment;
  } else if (text_align_ == cssom::KeywordValue::GetEnd()) {
    // If the value of text-align is end, then inline-level content is aligned
    // to the end edge of the line box.
    horizontal_alignment = base_direction_ == kRightToLeftBaseDirection
                               ? kLeftHorizontalAlignment
                               : kRightHorizontalAlignment;
  } else if (text_align_ == cssom::KeywordValue::GetLeft()) {
    // If the value of text-align is left, then inline-level content is aligned
    // to the left line edge.
    horizontal_alignment = kLeftHorizontalAlignment;
  } else if (text_align_ == cssom::KeywordValue::GetRight()) {
    // If the value of text-align is right, then inline-level content is aligned
    // to the right line edge.
    horizontal_alignment = kRightHorizontalAlignment;
  } else if (text_align_ == cssom::KeywordValue::GetCenter()) {
    // If the value of text-align is center, then inline-content is centered
    // within the line box.
    horizontal_alignment = kCenterHorizontalAlignment;
  } else {
    NOTREACHED() << "Unknown value of \"text-align\".";
    horizontal_alignment = kLeftHorizontalAlignment;
  }
  return horizontal_alignment;
}

}  // namespace layout
}  // namespace cobalt

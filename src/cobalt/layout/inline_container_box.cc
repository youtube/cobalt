// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/layout/inline_container_box.h"

#include <limits>

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/line_box.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

InlineContainerBox::InlineContainerBox(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        css_computed_style_declaration,
    UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker)
    : ContainerBox(css_computed_style_declaration, used_style_provider,
                   layout_stat_tracker),
      should_collapse_leading_white_space_(false),
      should_collapse_trailing_white_space_(false),
      has_leading_white_space_(false),
      has_trailing_white_space_(false),
      is_collapsed_(false),
      justifies_line_existence_(false),
      first_box_justifying_line_existence_index_(0),
      used_font_(used_style_provider->GetUsedFontList(
          css_computed_style_declaration->data()->font_family(),
          css_computed_style_declaration->data()->font_size(),
          css_computed_style_declaration->data()->font_style(),
          css_computed_style_declaration->data()->font_weight())) {}

InlineContainerBox::~InlineContainerBox() {}

Box::Level InlineContainerBox::GetLevel() const { return kInlineLevel; }

bool InlineContainerBox::TryAddChild(const scoped_refptr<Box>& child_box) {
  switch (child_box->GetLevel()) {
    case kBlockLevel:
      if (!child_box->IsAbsolutelyPositioned()) {
        // Only inline-level boxes are allowed as in-flow children of an inline
        // container box.
        return false;
      }
      // Fall through if out-of-flow.

    case kInlineLevel:
      // If the inline container box already contains a line break, then no
      // additional children can be added to it.
      if (HasTrailingLineBreak()) {
        return false;
      }

      PushBackDirectChild(child_box);
      return true;

    default:
      NOTREACHED();
      return false;
  }
}

scoped_refptr<ContainerBox> InlineContainerBox::TrySplitAtEnd() {
  scoped_refptr<InlineContainerBox> box_after_split(
      new InlineContainerBox(css_computed_style_declaration(),
                             used_style_provider(), layout_stat_tracker()));
  // When an inline box is split, margins, borders, and padding have no visual
  // effect where the split occurs.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  // TODO: Implement the above comment.

  return box_after_split;
}

LayoutUnit InlineContainerBox::GetInlineLevelBoxHeight() const {
  return line_height_;
}

LayoutUnit InlineContainerBox::GetInlineLevelTopMargin() const {
  return inline_top_margin_;
}

void InlineContainerBox::UpdateContentSizeAndMargins(
    const LayoutParams& layout_params) {
  // Lay out child boxes as one line without width constraints and white space
  // trimming.
  const render_tree::FontMetrics& font_metrics = used_font_->GetFontMetrics();
  LayoutUnit box_top_height = LayoutUnit(font_metrics.ascent());
  LineBox line_box(box_top_height, true, computed_style()->line_height(),
                   font_metrics, should_collapse_leading_white_space_,
                   should_collapse_trailing_white_space_, layout_params,
                   kLeftToRightBaseDirection, cssom::KeywordValue::GetLeft(),
                   computed_style()->font_size(), LayoutUnit(), LayoutUnit());

  for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    line_box.BeginAddChildAndMaybeOverflow(*child_box_iterator);
  }
  line_box.EndUpdates();

  // Although the spec says:
  //
  // The "width" property does not apply.
  //   https://www.w3.org/TR/CSS21/visudet.html#inline-width
  //
  // ...it is not the entire truth. It merely means that we have to ignore
  // the computed value of "width". Instead we use the shrink-to-fit width of
  // a hypothetical line box that contains all children. Later on this allow
  // to apply the following rule:
  //
  // When an inline box exceeds the width of a line box, it is split into
  // several boxes.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  set_width(line_box.shrink_to_fit_width());

  base::optional<LayoutUnit> maybe_margin_left = GetUsedMarginLeftIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_margin_right = GetUsedMarginRightIfNotAuto(
      computed_style(), layout_params.containing_block_size);

  // A computed value of "auto" for "margin-left" or "margin-right" becomes
  // a used value of "0".
  //   https://www.w3.org/TR/CSS21/visudet.html#inline-width
  set_margin_left(maybe_margin_left.value_or(LayoutUnit()));
  set_margin_right(maybe_margin_right.value_or(LayoutUnit()));

  // The "height" property does not apply. The height of the content area should
  // be based on the font, but this specification does not specify how. [...]
  // However, we suggest that the height is chosen such that the content area
  // is just high enough for [...] the maximum ascenders and descenders, of all
  // the fonts in the element.
  //   https://www.w3.org/TR/CSS21/visudet.html#inline-non-replaced
  //
  // Above definition of used height matches the height of hypothetical line box
  // that contains all children.
  set_height(LayoutUnit(font_metrics.em_box_height()));

  // On a non-replaced inline element, 'line-height' specifies the height that
  // is used in the calculation of the line box height.
  //   https://www.w3.org/TR/CSS21/visudet.html#propdef-line-height
  line_height_ = line_box.height();

  // Vertical margins will not have any effect on non-replaced inline elements.
  //   https://www.w3.org/TR/CSS21/box.html#margin-properties
  set_margin_top(LayoutUnit());
  set_margin_bottom(LayoutUnit());
  inline_top_margin_ = line_box.baseline_offset_from_top() - line_box.top() -
                       border_top_width() - padding_top();

  has_leading_white_space_ = line_box.HasLeadingWhiteSpace();
  has_trailing_white_space_ = line_box.HasTrailingWhiteSpace();
  is_collapsed_ = line_box.IsCollapsed();
  justifies_line_existence_ =
      line_box.LineExists() || HasNonZeroMarginOrBorderOrPadding();
  first_box_justifying_line_existence_index_ =
      line_box.GetFirstBoxJustifyingLineExistenceIndex();
  baseline_offset_from_margin_box_top_ = line_box.baseline_offset_from_top();
}

WrapResult InlineContainerBox::TryWrapAt(
    WrapAtPolicy wrap_at_policy, WrapOpportunityPolicy wrap_opportunity_policy,
    bool is_line_existence_justified, LayoutUnit available_width,
    bool should_collapse_trailing_white_space) {
  DCHECK(!IsAbsolutelyPositioned());
  DCHECK(is_line_existence_justified || justifies_line_existence_);

  switch (wrap_at_policy) {
    case kWrapAtPolicyBefore:
      return TryWrapAtBefore(wrap_opportunity_policy,
                             is_line_existence_justified, available_width,
                             should_collapse_trailing_white_space);
    case kWrapAtPolicyLastOpportunityWithinWidth:
      return TryWrapAtLastOpportunityWithinWidth(
          wrap_opportunity_policy, is_line_existence_justified, available_width,
          should_collapse_trailing_white_space);
    case kWrapAtPolicyLastOpportunity:
      return TryWrapAtLastOpportunityBeforeIndex(
          child_boxes().size(), wrap_opportunity_policy,
          is_line_existence_justified, available_width,
          should_collapse_trailing_white_space);
    case kWrapAtPolicyFirstOpportunity:
      return TryWrapAtFirstOpportunity(
          wrap_opportunity_policy, is_line_existence_justified, available_width,
          should_collapse_trailing_white_space);
    default:
      NOTREACHED();
      return kWrapResultNoWrap;
  }
}

Box* InlineContainerBox::GetSplitSibling() const { return split_sibling_; }

bool InlineContainerBox::DoesFulfillEllipsisPlacementRequirement() const {
  for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    if (child_box->DoesFulfillEllipsisPlacementRequirement()) {
      return true;
    }
  }

  return false;
}

void InlineContainerBox::DoPreEllipsisPlacementProcessing() {
  for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    (*child_box_iterator)->DoPreEllipsisPlacementProcessing();
  }
}

void InlineContainerBox::DoPostEllipsisPlacementProcessing() {
  for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    (*child_box_iterator)->DoPostEllipsisPlacementProcessing();
  }
}

bool InlineContainerBox::TrySplitAtSecondBidiLevelRun() {
  const int kInvalidLevel = -1;
  int last_level = kInvalidLevel;

  Boxes::const_iterator child_box_iterator = child_boxes().begin();
  while (child_box_iterator != child_boxes().end()) {
    Box* child_box = *child_box_iterator;
    int current_level = child_box->GetBidiLevel().value_or(last_level);

    // If the last level isn't equal to the current level, then check on whether
    // or not the last level is kInvalidLevel. If it is, then no initial value
    // has been set yet. Otherwise, the intersection of two bidi levels has been
    // found where a split should occur.
    if (last_level != current_level) {
      if (last_level == kInvalidLevel) {
        last_level = current_level;
      } else {
        break;
      }
    }

    // Try to split the child box's internals.
    if (child_box->TrySplitAtSecondBidiLevelRun()) {
      child_box_iterator = InsertSplitSiblingOfDirectChild(child_box_iterator);
      break;
    }

    ++child_box_iterator;
  }

  // If the iterator reached the end, then no split was found.
  if (child_box_iterator == child_boxes().end()) {
    return false;
  }

  SplitAtIterator(child_box_iterator);
  return true;
}

base::optional<int> InlineContainerBox::GetBidiLevel() const {
  if (!child_boxes().empty()) {
    return child_boxes().front()->GetBidiLevel();
  }

  return base::nullopt;
}

void InlineContainerBox::SetShouldCollapseLeadingWhiteSpace(
    bool should_collapse_leading_white_space) {
  if (should_collapse_leading_white_space_ !=
      should_collapse_leading_white_space) {
    should_collapse_leading_white_space_ = should_collapse_leading_white_space;
    InvalidateUpdateSizeInputs();
  }
}

void InlineContainerBox::SetShouldCollapseTrailingWhiteSpace(
    bool should_collapse_trailing_white_space) {
  if (should_collapse_trailing_white_space_ !=
      should_collapse_trailing_white_space) {
    should_collapse_trailing_white_space_ =
        should_collapse_trailing_white_space;
    InvalidateUpdateSizeInputs();
  }
}

bool InlineContainerBox::HasLeadingWhiteSpace() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  DCHECK_EQ(width(), width());  // Width should not be NaN.

  return has_leading_white_space_;
}

bool InlineContainerBox::HasTrailingWhiteSpace() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  DCHECK_EQ(width(), width());  // Width should not be NaN.

  return has_trailing_white_space_;
}

bool InlineContainerBox::IsCollapsed() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  DCHECK_EQ(width(), width());  // Width should not be NaN.

  return is_collapsed_;
}

bool InlineContainerBox::JustifiesLineExistence() const {
  return justifies_line_existence_;
}

bool InlineContainerBox::HasTrailingLineBreak() const {
  return !child_boxes().empty() && child_boxes().back()->HasTrailingLineBreak();
}

bool InlineContainerBox::AffectsBaselineInBlockFormattingContext() const {
  NOTREACHED() << "Should only be called in a block formatting context.";
  return true;
}

LayoutUnit InlineContainerBox::GetBaselineOffsetFromTopMarginEdge() const {
  return baseline_offset_from_margin_box_top_;
}

bool InlineContainerBox::IsTransformable() const { return false; }

#ifdef COBALT_BOX_DUMP_ENABLED

void InlineContainerBox::DumpClassName(std::ostream* stream) const {
  *stream << "InlineContainerBox ";
}

void InlineContainerBox::DumpProperties(std::ostream* stream) const {
  ContainerBox::DumpProperties(stream);

  *stream << std::boolalpha << "line_height=" << line_height_.toFloat() << " "
          << "inline_top_margin=" << inline_top_margin_.toFloat() << " "
          << "has_leading_white_space=" << has_leading_white_space_ << " "
          << "has_trailing_white_space=" << has_trailing_white_space_ << " "
          << "is_collapsed=" << is_collapsed_ << " "
          << "justifies_line_existence=" << justifies_line_existence_ << " "
          << std::noboolalpha;
}

#endif  // COBALT_BOX_DUMP_ENABLED

void InlineContainerBox::DoPlaceEllipsisOrProcessPlacedEllipsis(
    BaseDirection base_direction, LayoutUnit desired_offset,
    bool* is_placement_requirement_met, bool* is_placed,
    LayoutUnit* placed_offset) {
  // If the ellipsis hasn't been previously placed, but the placement
  // requirement is met and its desired offset comes before the content box's
  // start edge, then place the ellipsis at its desired position. This can occur
  // when the desired position falls between the start edge of the margin box
  // and the start edge of its content box.
  if (!*is_placed && *is_placement_requirement_met) {
    LayoutUnit content_box_start_edge_offset =
        GetContentBoxStartEdgeOffsetFromContainingBlock(base_direction);
    if ((base_direction == kRightToLeftBaseDirection &&
         desired_offset >= content_box_start_edge_offset) ||
        (base_direction != kRightToLeftBaseDirection &&
         desired_offset <= content_box_start_edge_offset)) {
      *is_placed = true;
      *placed_offset = desired_offset;
    }
  }

  bool was_placed_before_children = *is_placed;

  // Subtract the content box offset from the desired offset. This box's
  // children will treat its content box left edge as the base ellipsis offset
  // position, and the content box offset will be re-added after the ellipsis
  // is placed. This allows its children to only focus on their offset from
  // their containing block, and not worry about nested containing blocks.
  LayoutUnit content_box_offset =
      GetContentBoxLeftEdgeOffsetFromContainingBlock();
  desired_offset -= content_box_offset;

  // Walk each child box in base direction order attempting to place the
  // ellipsis and update the box's ellipsis state. Even after the ellipsis is
  // placed, subsequent boxes must still be processed, as their state my change
  // as a result of having an ellipsis preceding them on the line.
  if (base_direction == kRightToLeftBaseDirection) {
    for (Boxes::const_reverse_iterator child_box_iterator =
             child_boxes().rbegin();
         child_box_iterator != child_boxes().rend(); ++child_box_iterator) {
      Box* child_box = *child_box_iterator;
      // Out-of-flow boxes are not impacted by ellipses.
      if (child_box->IsAbsolutelyPositioned()) {
        continue;
      }
      child_box->TryPlaceEllipsisOrProcessPlacedEllipsis(
          base_direction, desired_offset, is_placement_requirement_met,
          is_placed, placed_offset);
    }
  } else {
    for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
         child_box_iterator != child_boxes().end(); ++child_box_iterator) {
      Box* child_box = *child_box_iterator;
      // Out-of-flow boxes are not impacted by ellipses.
      if (child_box->IsAbsolutelyPositioned()) {
        continue;
      }
      child_box->TryPlaceEllipsisOrProcessPlacedEllipsis(
          base_direction, desired_offset, is_placement_requirement_met,
          is_placed, placed_offset);
    }
  }

  // If the ellipsis was not placed prior to processing the child boxes, then it
  // is guaranteed that the placement location comes after the start edge of the
  // content box. The content box's offset needs to be added back to the placed
  // offset, so that the offset again references this box's containing block.
  // Additionally, in the event that the ellipsis was not placed within a child
  // box, it will now be placed.
  if (!was_placed_before_children) {
    // If the ellipsis hasn't been placed yet, then place the ellipsis at its
    // desired position. This case can occur when the desired position falls
    // between the end edge of the box's content and the end edge of the box's
    // margin. In this case, |is_placement_requirement_met| does not need to be
    // checked, as it is guaranteed that one of the child boxes met the
    // requirement.
    if (!(*is_placed)) {
      *is_placed = true;
      *placed_offset = desired_offset;
    }

    *placed_offset += content_box_offset;
  }
}

WrapResult InlineContainerBox::TryWrapAtBefore(
    WrapOpportunityPolicy wrap_opportunity_policy, bool is_line_requirement_met,
    LayoutUnit available_width, bool should_collapse_trailing_white_space) {
  // If there are no boxes within the inline container box, then there is no
  // first box to try to wrap before. This box does not provide a wrappable
  // point on its own. Additionally, if the line requirement has not been met
  // before this box, then no wrap is available.
  if (child_boxes().size() == 0 || !is_line_requirement_met) {
    return kWrapResultNoWrap;
  } else {
    // Otherwise, attempt to wrap before the first child box.
    return TryWrapAtIndex(0, kWrapAtPolicyBefore, wrap_opportunity_policy,
                          is_line_requirement_met, available_width,
                          should_collapse_trailing_white_space);
  }
}

WrapResult InlineContainerBox::TryWrapAtLastOpportunityWithinWidth(
    WrapOpportunityPolicy wrap_opportunity_policy,
    bool is_line_existence_justified, LayoutUnit available_width,
    bool should_collapse_trailing_white_space) {
  DCHECK(GetMarginBoxWidth().GreaterThanOrNaN(available_width));

  // Calculate the available width where the content begins. If the content
  // does not begin within the available width, then the wrap can only occur
  // before the inline container box.
  available_width -= GetContentBoxLeftEdgeOffsetFromMarginBox();
  if (available_width < LayoutUnit()) {
    return TryWrapAtBefore(wrap_opportunity_policy, is_line_existence_justified,
                           available_width,
                           should_collapse_trailing_white_space);
  }

  // Determine the child box where the overflow occurs. If the overflow does not
  // occur until after the end of the content, then the overflow index will be
  // set to the number of child boxes.
  size_t overflow_index = 0;
  while (overflow_index < child_boxes().size()) {
    Box* child_box = child_boxes()[overflow_index];
    // Absolutely positioned boxes are not included in width calculations.
    if (child_box->IsAbsolutelyPositioned()) {
      continue;
    }

    LayoutUnit child_width = child_box->GetMarginBoxWidth();
    if (child_width > available_width) {
      break;
    }
    available_width -= child_width;
    ++overflow_index;
  }

  // If the overflow occurs before the line is justified, then no wrap is
  // available.
  if (!is_line_existence_justified &&
      overflow_index < first_box_justifying_line_existence_index_) {
    return kWrapResultNoWrap;
  }

  // If the overflow occurred within the content and not after, then attempt to
  // wrap within the box that overflowed and return the result if the wrap is
  // successful.
  if (overflow_index < child_boxes().size()) {
    WrapResult wrap_result =
        TryWrapAtIndex(overflow_index, kWrapAtPolicyLastOpportunityWithinWidth,
                       wrap_opportunity_policy, is_line_existence_justified,
                       available_width, should_collapse_trailing_white_space);
    if (wrap_result != kWrapResultNoWrap) {
      return wrap_result;
    }
  }

  // If no wrap was found within the box that overflowed, then attempt to wrap
  // within an earlier child box.
  return TryWrapAtLastOpportunityBeforeIndex(
      overflow_index, wrap_opportunity_policy, is_line_existence_justified,
      available_width, should_collapse_trailing_white_space);
}

WrapResult InlineContainerBox::TryWrapAtLastOpportunityBeforeIndex(
    size_t index, WrapOpportunityPolicy wrap_opportunity_policy,
    bool is_line_existence_justified, LayoutUnit available_width,
    bool should_collapse_trailing_white_space) {
  WrapResult wrap_result = kWrapResultNoWrap;

  // If the line is already justified, then any child before the index is
  // potentially wrappable. Otherwise, children preceding the first box that
  // justifies the line's existence do not need to be checked, as they can
  // never be wrappable.
  size_t first_wrappable_index =
      is_line_existence_justified ? 0
                                  : first_box_justifying_line_existence_index_;

  // Iterate backwards through the children attempting to wrap until a wrap is
  // successful or the first wrappable index is reached.
  while (wrap_result == kWrapResultNoWrap && index > first_wrappable_index) {
    --index;
    wrap_result =
        TryWrapAtIndex(index, kWrapAtPolicyLastOpportunity,
                       wrap_opportunity_policy, is_line_existence_justified,
                       available_width, should_collapse_trailing_white_space);
  }

  return wrap_result;
}

WrapResult InlineContainerBox::TryWrapAtFirstOpportunity(
    WrapOpportunityPolicy wrap_opportunity_policy,
    bool is_line_existence_justified, LayoutUnit available_width,
    bool should_collapse_trailing_white_space) {
  WrapResult wrap_result = kWrapResultNoWrap;

  // If the line is already justified, then any child is potentially wrappable.
  // Otherwise, children preceding the first box that justifies the line's
  // existence do not need to be checked, as they can never be wrappable.
  size_t check_index = is_line_existence_justified
                           ? 0
                           : first_box_justifying_line_existence_index_;

  // Iterate forward through the children attempting to wrap until a wrap is
  // successful or all of the children have been attempted.
  for (; wrap_result == kWrapResultNoWrap && check_index < child_boxes().size();
       ++check_index) {
    wrap_result =
        TryWrapAtIndex(check_index, kWrapAtPolicyFirstOpportunity,
                       wrap_opportunity_policy, is_line_existence_justified,
                       available_width, should_collapse_trailing_white_space);
  }

  return wrap_result;
}

WrapResult InlineContainerBox::TryWrapAtIndex(
    size_t wrap_index, WrapAtPolicy wrap_at_policy,
    WrapOpportunityPolicy wrap_opportunity_policy,
    bool is_line_existence_justified, LayoutUnit available_width,
    bool should_collapse_trailing_white_space) {
  Box* child_box = child_boxes()[wrap_index];
  // Absolutely positioned boxes are not wrappable.
  if (child_box->IsAbsolutelyPositioned()) {
    return kWrapResultNoWrap;
  }

  // Check for whether the line is justified before this child. If it is not,
  // then verify that the line is justified within this child. This function
  // should not be called for unjustified indices.
  bool is_line_existence_justified_before_index =
      is_line_existence_justified ||
      wrap_index > first_box_justifying_line_existence_index_;
  DCHECK(is_line_existence_justified_before_index ||
         wrap_index == first_box_justifying_line_existence_index_);

  WrapResult wrap_result = child_box->TryWrapAt(
      wrap_at_policy, wrap_opportunity_policy,
      is_line_existence_justified_before_index, available_width,
      should_collapse_trailing_white_space);
  // If the no wrap was found, then simply return out. There's nothing to do.
  if (wrap_result == kWrapResultNoWrap) {
    return kWrapResultNoWrap;
    // Otherwise, if the wrap is before the first child, then the wrap is
    // happening before the full inline container box and no split is
    // occurring.
    // When breaks happen before the first or the last character of a box,
    // the break occurs immediately before/after the box (at its margin edge)
    // rather than breaking the box between its content edge and the content.
    //   https://www.w3.org/TR/css-text-3/#line-breaking
  } else if (wrap_result == kWrapResultWrapBefore && wrap_index == 0) {
    return kWrapResultWrapBefore;
    // In all other cases, the inline container box is being split as a result
    // of the wrap.
  } else {
    Boxes::const_iterator wrap_iterator =
        child_boxes().begin() + static_cast<int>(wrap_index);
    // If the child was split during its wrap, then the split sibling that was
    // produced by the split needs to be added to the container's children.
    if (wrap_result == kWrapResultSplitWrap) {
      wrap_iterator = InsertSplitSiblingOfDirectChild(wrap_iterator);
    }

    SplitAtIterator(wrap_iterator);
    return kWrapResultSplitWrap;
  }
}

void InlineContainerBox::SplitAtIterator(
    Boxes::const_iterator child_split_iterator) {
  // TODO: When an inline box is split, margins, borders, and padding
  //       have no visual effect where the split occurs.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting

  // Move the children after the split into a new box.
  scoped_refptr<InlineContainerBox> box_after_split(
      new InlineContainerBox(css_computed_style_declaration(),
                             used_style_provider(), layout_stat_tracker()));

  // Update the split sibling links.
  box_after_split->split_sibling_ = split_sibling_;
  split_sibling_ = box_after_split;

  // Now move the children, starting at the iterator, from this container to the
  // split sibling.
  MoveDirectChildrenToSplitSibling(child_split_iterator);

#ifdef _DEBUG
  // Make sure that the |UpdateContentSizeAndMargins| is called afterwards.

  set_width(LayoutUnit());
  set_height(LayoutUnit());

  set_margin_left(LayoutUnit());
  set_margin_top(LayoutUnit());
  set_margin_right(LayoutUnit());
  set_margin_bottom(LayoutUnit());
#endif  // _DEBUG
}

}  // namespace layout
}  // namespace cobalt

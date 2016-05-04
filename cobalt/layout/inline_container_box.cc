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
    UsedStyleProvider* used_style_provider)
    : ContainerBox(css_computed_style_declaration, used_style_provider),
      should_collapse_leading_white_space_(false),
      should_collapse_trailing_white_space_(false),
      has_leading_white_space_(false),
      has_trailing_white_space_(false),
      is_collapsed_(false),
      justifies_line_existence_(false),
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
  scoped_refptr<InlineContainerBox> box_after_split(new InlineContainerBox(
      css_computed_style_declaration(), used_style_provider()));
  // When an inline box is split, margins, borders, and padding have no visual
  // effect where the split occurs.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  // TODO(***REMOVED***): Implement the above comment.

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
                   cssom::KeywordValue::GetNormal(),
                   computed_style()->font_size(), LayoutUnit(), LayoutUnit());

  for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    if (child_box->IsAbsolutelyPositioned()) {
      line_box.BeginEstimateStaticPosition(child_box);
    } else {
      line_box.BeginUpdateRectAndMaybeOverflow(child_box);
    }
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
      line_box.line_exists() || HasNonZeroMarginOrBorderOrPadding();
  baseline_offset_from_margin_box_top_ = line_box.baseline_offset_from_top();
}

scoped_refptr<Box> InlineContainerBox::TrySplitAt(
    LayoutUnit available_width, bool should_collapse_trailing_white_space,
    bool allow_overflow) {
  DCHECK_GT(GetMarginBoxWidth(), available_width);

  available_width -= GetContentBoxLeftEdgeOffsetFromMarginBox();
  if (!allow_overflow && available_width < LayoutUnit()) {
    return scoped_refptr<Box>();
  }

  // Leave first N children that fit completely in the available width in this
  // box. The first child that does not fit within the width may also be split
  // and partially left in this box. Additionally, if |allow_overflow| is true,
  // then overflows past the available width are allowed until a child with a
  // used width greater than 0 has been added.
  Boxes::const_iterator child_box_iterator;
  for (child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;

    LayoutUnit margin_box_width_of_child_box = child_box->GetMarginBoxWidth();

    // Split the first child that overflows the available width.
    // Leave its part before the split in this box.
    if (available_width < margin_box_width_of_child_box) {
      scoped_refptr<Box> child_box_after_split = child_box->TrySplitAt(
          available_width, should_collapse_trailing_white_space,
          allow_overflow);
      if (child_box_after_split) {
        ++child_box_iterator;
        child_box_iterator =
            InsertDirectChild(child_box_iterator, child_box_after_split);
      } else if (allow_overflow) {
        // Unable to split the child, but overflow is allowed, so increment
        // |child_box_iterator| because the whole first child box is being left
        // in this box.
        ++child_box_iterator;
      }

      break;
    }

    available_width -= margin_box_width_of_child_box;

    // Only continue allowing overflow if the box that was added
    // does not contribute to the line box.
    allow_overflow = allow_overflow && !child_box->JustifiesLineExistence();
  }

  // The first child cannot be split, so this box cannot be split either.
  if (child_box_iterator == child_boxes().begin()) {
    return scoped_refptr<Box>();
  }
  // Either:
  //   - All children fit but the right edge overflows.
  //   - The last child overflows, and happens to be the first one that
  //     justifies line existence and the overflow is allowed.
  // Anyway, this box cannot be split.
  //
  // TODO(***REMOVED***): If all children fit but the right edge overflows,
  //               go backwards and try splitting children just before their
  //               right edge.
  if (child_box_iterator == child_boxes().end()) {
    DCHECK(padding_right() + border_right_width() + margin_right() >
               available_width ||
           (allow_overflow &&
            child_boxes().back()->GetMarginBoxWidth() > available_width));
    return scoped_refptr<Box>();
  }

  return SplitAtIterator(child_box_iterator);
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

void InlineContainerBox::ResetEllipses() {
  for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    (*child_box_iterator)->ResetEllipses();
  }
}

scoped_refptr<Box> InlineContainerBox::TrySplitAtSecondBidiLevelRun() {
  const int kInvalidLevel = -1;
  int last_level = kInvalidLevel;

  Boxes::const_iterator child_box_iterator;
  for (child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
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
    scoped_refptr<Box> child_box_after_split =
        child_box->TrySplitAtSecondBidiLevelRun();
    if (child_box_after_split) {
      ++child_box_iterator;
      child_box_iterator =
          InsertDirectChild(child_box_iterator, child_box_after_split);
      break;
    }
  }

  // If the iterator reached the end, then no split was found.
  if (child_box_iterator == child_boxes().end()) {
    return scoped_refptr<Box>();
  }

  return SplitAtIterator(child_box_iterator);
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
      child_box->TryPlaceEllipsisOrProcessPlacedEllipsis(
          base_direction, desired_offset, is_placement_requirement_met,
          is_placed, placed_offset);
    }
  } else {
    for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
         child_box_iterator != child_boxes().end(); ++child_box_iterator) {
      Box* child_box = *child_box_iterator;
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

scoped_refptr<Box> InlineContainerBox::SplitAtIterator(
    Boxes::const_iterator child_split_iterator) {
  // TODO(***REMOVED***): When an inline box is split, margins, borders, and padding
  //              have no visual effect where the split occurs.
  //              Tracked in b/27134223.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting

  // Move the children after the split into a new box.
  scoped_refptr<InlineContainerBox> box_after_split(new InlineContainerBox(
      css_computed_style_declaration(), used_style_provider()));

  // Update the split sibling links.
  box_after_split->split_sibling_ = split_sibling_;
  split_sibling_ = box_after_split;

  box_after_split->MoveChildrenFrom(box_after_split->child_boxes().end(), this,
                                    child_split_iterator, child_boxes().end());

#ifdef _DEBUG
  // Make sure that the |UpdateContentSizeAndMargins| is called afterwards.

  set_width(LayoutUnit());
  set_height(LayoutUnit());

  set_margin_left(LayoutUnit());
  set_margin_top(LayoutUnit());
  set_margin_right(LayoutUnit());
  set_margin_bottom(LayoutUnit());
#endif  // _DEBUG

  return box_after_split;
}

}  // namespace layout
}  // namespace cobalt

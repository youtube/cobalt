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

#include "cobalt/layout/block_container_box.h"

#include "cobalt/layout/formatting_context.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

BlockContainerBox::BlockContainerBox(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        css_computed_style_declaration,
    BaseDirection base_direction, UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker)
    : ContainerBox(css_computed_style_declaration, used_style_provider,
                   layout_stat_tracker),
      base_direction_(base_direction) {}

BlockContainerBox::~BlockContainerBox() {}

// Updates used values of "width" and "margin" properties based on
// https://www.w3.org/TR/CSS21/visudet.html#Computing_widths_and_margins.
void BlockContainerBox::UpdateContentWidthAndMargins(
    LayoutUnit containing_block_width, bool shrink_to_fit_width_forced,
    bool width_depends_on_containing_block,
    const base::optional<LayoutUnit>& maybe_left,
    const base::optional<LayoutUnit>& maybe_right,
    const base::optional<LayoutUnit>& maybe_margin_left,
    const base::optional<LayoutUnit>& maybe_margin_right,
    const base::optional<LayoutUnit>& maybe_width,
    const base::optional<LayoutUnit>& maybe_height) {
  if (IsAbsolutelyPositioned()) {
    UpdateWidthAssumingAbsolutelyPositionedBox(
        containing_block_width, maybe_left, maybe_right, maybe_width,
        maybe_margin_left, maybe_margin_right, maybe_height);
  } else {
    base::optional<LayoutUnit> maybe_nulled_width = maybe_width;
    Level forced_level = GetLevel();
    if (shrink_to_fit_width_forced) {
      forced_level = kInlineLevel;
      // Break circular dependency if needed.
      if (width_depends_on_containing_block) {
        maybe_nulled_width = base::nullopt;
      }
    }

    switch (forced_level) {
      case kBlockLevel:
        UpdateWidthAssumingBlockLevelInFlowBox(
            containing_block_width, maybe_nulled_width, maybe_margin_left,
            maybe_margin_right);
        break;
      case kInlineLevel:
        UpdateWidthAssumingInlineLevelInFlowBox(
            containing_block_width, maybe_nulled_width, maybe_margin_left,
            maybe_margin_right, maybe_height);
        break;
    }
  }
}

// Updates used values of "height" and "margin" properties based on
// https://www.w3.org/TR/CSS21/visudet.html#Computing_heights_and_margins.
void BlockContainerBox::UpdateContentHeightAndMargins(
    const SizeLayoutUnit& containing_block_size,
    const base::optional<LayoutUnit>& maybe_top,
    const base::optional<LayoutUnit>& maybe_bottom,
    const base::optional<LayoutUnit>& maybe_margin_top,
    const base::optional<LayoutUnit>& maybe_margin_bottom,
    const base::optional<LayoutUnit>& maybe_height) {
  LayoutParams child_layout_params;
  LayoutParams absolute_child_layout_params;
  if (AsAnonymousBlockBox()) {
    // Anonymous block boxes are ignored when resolving percentage values
    // that would refer to it: the closest non-anonymous ancestor box is used
    // instead.
    //   https://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
    child_layout_params.containing_block_size = containing_block_size;
  } else {
    // If the element's position is "relative" or "static", the containing block
    // is formed by the content edge of the nearest block container ancestor
    // box.
    //   https://www.w3.org/TR/CSS21/visudet.html#containing-block-details
    child_layout_params.containing_block_size.set_width(width());
    // If the element has 'position: absolute', ...
    // the containing block is formed by the padding edge of the ancestor.
    //   http://www.w3.org/TR/CSS21/visudet.html#containing-block-details
    absolute_child_layout_params.containing_block_size.set_width(
        GetPaddingBoxWidth());
    // The "auto" height is not known yet but it shouldn't matter for in-flow
    // children, as per:
    //
    // If the height of the containing block is not specified explicitly (i.e.,
    // it depends on content height), and this element is not absolutely
    // positioned, the value [of "height"] computes to "auto".
    //   https://www.w3.org/TR/CSS21/visudet.html#the-height-property
    if (maybe_height) {
      child_layout_params.containing_block_size.set_height(*maybe_height);
    } else if (maybe_top && maybe_bottom) {
      child_layout_params.containing_block_size.set_height(
          containing_block_size.height() - *maybe_top - *maybe_bottom);
    } else {
      child_layout_params.containing_block_size.set_height(LayoutUnit());
    }
  }
  scoped_ptr<FormattingContext> formatting_context =
      UpdateRectOfInFlowChildBoxes(child_layout_params);

  if (IsAbsolutelyPositioned()) {
    UpdateHeightAssumingAbsolutelyPositionedBox(
        containing_block_size.height(), maybe_top, maybe_bottom, maybe_height,
        maybe_margin_top, maybe_margin_bottom, *formatting_context);
  } else {
    UpdateHeightAssumingInFlowBox(maybe_height, maybe_margin_top,
                                  maybe_margin_bottom, *formatting_context);
  }

  // Positioned children are laid out at the end as their position and size
  // depends on the size of the containing block as well as possibly their
  // previously calculated in-flow position.
  child_layout_params.containing_block_size.set_height(height());
  absolute_child_layout_params.containing_block_size.set_height(
      GetPaddingBoxHeight());
  UpdateRectOfPositionedChildBoxes(child_layout_params,
                                   absolute_child_layout_params);

  if (formatting_context->maybe_baseline_offset_from_top_content_edge()) {
    maybe_baseline_offset_from_top_margin_edge_ =
        margin_top() + border_top_width() + padding_top() +
        *formatting_context->maybe_baseline_offset_from_top_content_edge();
  } else {
    maybe_baseline_offset_from_top_margin_edge_ = base::nullopt;
  }
}

void BlockContainerBox::UpdateContentSizeAndMargins(
    const LayoutParams& layout_params) {
  base::optional<LayoutUnit> maybe_height = GetUsedHeightIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  bool width_depends_on_containing_block;
  base::optional<LayoutUnit> maybe_width = GetUsedWidthIfNotAuto(
      computed_style(), layout_params.containing_block_size,
      &width_depends_on_containing_block);
  base::optional<LayoutUnit> maybe_margin_left = GetUsedMarginLeftIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_margin_right = GetUsedMarginRightIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_left = GetUsedLeftIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_right = GetUsedRightIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_top = GetUsedTopIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_bottom = GetUsedBottomIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_margin_top = GetUsedMarginTopIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_margin_bottom = GetUsedMarginBottomIfNotAuto(
      computed_style(), layout_params.containing_block_size);

  UpdateContentWidthAndMargins(layout_params.containing_block_size.width(),
                               layout_params.shrink_to_fit_width_forced,
                               width_depends_on_containing_block, maybe_left,
                               maybe_right, maybe_margin_left,
                               maybe_margin_right, maybe_width, maybe_height);

  // If the tentative used width is greater than 'max-width', the rules above
  // are applied again, but this time using the computed value of 'max-width' as
  // the computed value for 'width'.
  //   https://www.w3.org/TR/CSS21/visudet.html#min-max-widths
  bool max_width_depends_on_containing_block;
  base::optional<LayoutUnit> maybe_max_width = GetUsedMaxWidthIfNotNone(
      computed_style(), layout_params.containing_block_size,
      &max_width_depends_on_containing_block);
  if (maybe_max_width && width() > maybe_max_width.value()) {
    UpdateContentWidthAndMargins(
        layout_params.containing_block_size.width(),
        layout_params.shrink_to_fit_width_forced,
        max_width_depends_on_containing_block, maybe_left, maybe_right,
        maybe_margin_left, maybe_margin_right, maybe_max_width, maybe_height);
  }

  // If the resulting width is smaller than 'min-width', the rules above are
  // applied again, but this time using the value of 'min-width' as the computed
  // value for 'width'.
  //   https://www.w3.org/TR/CSS21/visudet.html#min-max-widths
  bool min_width_depends_on_containing_block;
  base::optional<LayoutUnit> min_width =
      GetUsedMinWidth(computed_style(), layout_params.containing_block_size,
                      &min_width_depends_on_containing_block);
  if (width() < min_width.value()) {
    UpdateContentWidthAndMargins(layout_params.containing_block_size.width(),
                                 layout_params.shrink_to_fit_width_forced,
                                 min_width_depends_on_containing_block,
                                 maybe_left, maybe_right, maybe_margin_left,
                                 maybe_margin_right, min_width, maybe_height);
  }

  UpdateContentHeightAndMargins(layout_params.containing_block_size, maybe_top,
                                maybe_bottom, maybe_margin_top,
                                maybe_margin_bottom, maybe_height);

  // If the tentative height is greater than 'max-height', the rules above are
  // applied again, but this time using the value of 'max-height' as the
  // computed value for 'height'.
  //   https://www.w3.org/TR/CSS21/visudet.html#min-max-heights
  bool max_height_depends_on_containing_block;
  base::optional<LayoutUnit> maybe_max_height = GetUsedMaxHeightIfNotNone(
      computed_style(), layout_params.containing_block_size,
      &max_height_depends_on_containing_block);
  if (maybe_max_height && height() > maybe_max_height.value()) {
    UpdateContentHeightAndMargins(layout_params.containing_block_size,
                                  maybe_top, maybe_bottom, maybe_margin_top,
                                  maybe_margin_bottom, maybe_max_height);
  }

  // If the resulting height is smaller than 'min-height', the rules above are
  // applied again, but this time using the value of 'min-height' as the
  // computed value for 'height'.
  //   https://www.w3.org/TR/CSS21/visudet.html#min-max-heights
  bool min_height_depends_on_containing_block;
  base::optional<LayoutUnit> min_height =
      GetUsedMinHeight(computed_style(), layout_params.containing_block_size,
                       &min_height_depends_on_containing_block);
  if (height() < min_height.value()) {
    UpdateContentHeightAndMargins(layout_params.containing_block_size,
                                  maybe_top, maybe_bottom, maybe_margin_top,
                                  maybe_margin_bottom, min_height);
  }
}

WrapResult BlockContainerBox::TryWrapAt(
    WrapAtPolicy /*wrap_at_policy*/,
    WrapOpportunityPolicy /*wrap_opportunity_policy*/,
    bool /*is_line_existence_justified*/, LayoutUnit /*available_width*/,
    bool /*should_collapse_trailing_white_space*/) {
  DCHECK(!IsAbsolutelyPositioned());
  DCHECK_EQ(kInlineLevel, GetLevel());
  return kWrapResultNoWrap;
}

bool BlockContainerBox::TrySplitAtSecondBidiLevelRun() { return false; }

base::optional<int> BlockContainerBox::GetBidiLevel() const {
  return base::optional<int>();
}

void BlockContainerBox::SetShouldCollapseLeadingWhiteSpace(
    bool /*should_collapse_leading_white_space*/) {
  DCHECK_EQ(kInlineLevel, GetLevel());
  // Do nothing.
}

void BlockContainerBox::SetShouldCollapseTrailingWhiteSpace(
    bool /*should_collapse_trailing_white_space*/) {
  DCHECK_EQ(kInlineLevel, GetLevel());
  // Do nothing.
}

bool BlockContainerBox::HasLeadingWhiteSpace() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  return false;
}

bool BlockContainerBox::HasTrailingWhiteSpace() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  return false;
}

bool BlockContainerBox::IsCollapsed() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  return false;
}

bool BlockContainerBox::JustifiesLineExistence() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  return true;
}

bool BlockContainerBox::AffectsBaselineInBlockFormattingContext() const {
  return static_cast<bool>(maybe_baseline_offset_from_top_margin_edge_);
}

LayoutUnit BlockContainerBox::GetBaselineOffsetFromTopMarginEdge() const {
  return maybe_baseline_offset_from_top_margin_edge_.value_or(
      GetMarginBoxHeight());
}

scoped_refptr<ContainerBox> BlockContainerBox::TrySplitAtEnd() {
  return scoped_refptr<ContainerBox>();
}

BaseDirection BlockContainerBox::GetBaseDirection() const {
  return base_direction_;
}

bool BlockContainerBox::IsTransformable() const { return true; }

#ifdef COBALT_BOX_DUMP_ENABLED

void BlockContainerBox::DumpProperties(std::ostream* stream) const {
  ContainerBox::DumpProperties(stream);

  *stream << std::boolalpha << "affects_baseline_in_block_formatting_context="
          << AffectsBaselineInBlockFormattingContext() << " "
          << std::noboolalpha;
}

#endif  // COBALT_BOX_DUMP_ENABLED

// Based on https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width.
//
// The constraint that determines the used values for these elements is:
//       "left" + "margin-left" + "border-left-width" + "padding-left"
//     + "width"
//     + "padding-right" + "border-right-width" + "margin-right" + "right"
//     = width of containing block
void BlockContainerBox::UpdateWidthAssumingAbsolutelyPositionedBox(
    LayoutUnit containing_block_width,
    const base::optional<LayoutUnit>& maybe_left,
    const base::optional<LayoutUnit>& maybe_right,
    const base::optional<LayoutUnit>& maybe_width,
    const base::optional<LayoutUnit>& maybe_margin_left,
    const base::optional<LayoutUnit>& maybe_margin_right,
    const base::optional<LayoutUnit>& maybe_height) {
  // If all three of "left", "width", and "right" are "auto":
  if (!maybe_left && !maybe_width && !maybe_right) {
    // First set any "auto" values for "margin-left" and "margin-right" to 0.
    set_margin_left(maybe_margin_left.value_or(LayoutUnit()));
    set_margin_right(maybe_margin_right.value_or(LayoutUnit()));

    // Then set "left" to the static position...
    set_left(GetStaticPositionLeft());

    // ...and apply rule number three (the width is shrink-to-fit).
    set_width(GetShrinkToFitWidth(containing_block_width, maybe_height));
    return;
  }

  // If none of the three is "auto":
  if (maybe_left && maybe_width && maybe_right) {
    set_left(*maybe_left);
    set_width(*maybe_width);

    LayoutUnit horizontal_margin_sum = containing_block_width - *maybe_left -
                                       GetBorderBoxWidth() - *maybe_right;

    if (!maybe_margin_left && !maybe_margin_right) {
      // If both "margin-left" and "margin-right" are "auto", solve the equation
      // under the extra constraint that the two margins get equal values...
      LayoutUnit horizontal_margin = horizontal_margin_sum / 2;
      if (horizontal_margin < LayoutUnit()) {
        // ...unless this would make them negative, in which case set
        // "margin-left" to zero and solve for "margin-right".
        set_margin_left(LayoutUnit());
        set_margin_right(horizontal_margin_sum);
      } else {
        set_margin_left(horizontal_margin);
        set_margin_right(horizontal_margin);
      }
    } else if (!maybe_margin_left) {
      // If one of "margin-left" or "margin-right" is "auto", solve the equation
      // for that value.
      set_margin_left(horizontal_margin_sum - *maybe_margin_right);
      set_margin_right(*maybe_margin_right);
    } else if (!maybe_margin_right) {
      // If one of "margin-left" or "margin-right" is "auto", solve the equation
      // for that value.
      set_margin_left(*maybe_margin_left);
      set_margin_right(horizontal_margin_sum - *maybe_margin_left);
    } else {
      // If the values are over-constrained, ignore the value for "right".
      set_margin_left(*maybe_margin_left);
      set_margin_right(*maybe_margin_right);
    }
    return;
  }

  // Otherwise, set "auto" values for "margin-left" and "margin-right" to 0...
  set_margin_left(maybe_margin_left.value_or(LayoutUnit()));
  set_margin_right(maybe_margin_right.value_or(LayoutUnit()));

  // ...and pick the one of the following six rules that applies.

  // 1. "left" and "width" are "auto" and "right" is not "auto"...
  if (!maybe_left && !maybe_width && maybe_right) {
    // ...then the width is shrink-to-fit.
    set_width(GetShrinkToFitWidth(containing_block_width, maybe_height));
    // Then solve for "left".
    set_left(containing_block_width - GetMarginBoxWidth() - *maybe_right);
    return;
  }

  // 2. "left" and "right" are "auto" and "width" is not "auto"...
  if (!maybe_left && !maybe_right && maybe_width) {
    set_width(*maybe_width);
    // ...then set "left" to the static position.
    set_left(GetStaticPositionLeft());
    DCHECK_EQ(left(), left());  // Check for NaN.
    return;
  }

  // 3. "width" and "right" are "auto" and "left" is not "auto"...
  if (!maybe_width && !maybe_right && maybe_left) {
    set_left(*maybe_left);
    // ...then the width is shrink-to-fit.
    set_width(GetShrinkToFitWidth(containing_block_width, maybe_height));
    return;
  }

  // 4. "left" is "auto", "width" and "right" are not "auto"...
  if (!maybe_left && maybe_width && maybe_right) {
    set_width(*maybe_width);
    // ...then solve for "left".
    set_left(containing_block_width - GetMarginBoxWidth() - *maybe_right);
    return;
  }

  // 5. "width" is "auto", "left" and "right" are not "auto"...
  if (!maybe_width && maybe_left && maybe_right) {
    set_left(*maybe_left);
    // ...then solve for "width".
    set_width(containing_block_width - *maybe_left - margin_left() -
              border_left_width() - padding_left() - padding_right() -
              border_right_width() - margin_right() - *maybe_right);
    return;
  }

  // 6. "right" is "auto", "left" and "width" are not "auto".
  if (!maybe_right && maybe_left && maybe_width) {
    set_left(*maybe_left);
    set_width(*maybe_width);
    return;
  }
}

// Based on https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-height.
//
// The constraint that determines the used values for these elements is:
//       "top" + "margin-top" + "border-top-width" + "padding-top"
//     + "height"
//     + "padding-bottom" + "border-bottom-width" + "margin-bottom" + "bottom"
//     = height of containing block
void BlockContainerBox::UpdateHeightAssumingAbsolutelyPositionedBox(
    LayoutUnit containing_block_height,
    const base::optional<LayoutUnit>& maybe_top,
    const base::optional<LayoutUnit>& maybe_bottom,
    const base::optional<LayoutUnit>& maybe_height,
    const base::optional<LayoutUnit>& maybe_margin_top,
    const base::optional<LayoutUnit>& maybe_margin_bottom,
    const FormattingContext& formatting_context) {
  // If all three of "top", "height", and "bottom" are "auto":
  if (!maybe_top && !maybe_height && !maybe_bottom) {
    // First set any "auto" values for "margin-top" and "margin-bottom" to 0.
    set_margin_top(maybe_margin_top.value_or(LayoutUnit()));
    set_margin_bottom(maybe_margin_bottom.value_or(LayoutUnit()));

    // Then set "top" to the static position...
    set_top(GetStaticPositionTop());
    DCHECK_EQ(top(), top());  // Check for NaN.

    // ...and apply rule number three (the height is based on the content).
    set_height(formatting_context.auto_height());
    return;
  }

  // If none of the three is "auto":
  if (maybe_top && maybe_height && maybe_bottom) {
    set_top(*maybe_top);
    set_height(*maybe_height);

    LayoutUnit vertical_margin_sum = containing_block_height - *maybe_top -
                                     GetBorderBoxHeight() - *maybe_bottom;

    if (!maybe_margin_top && !maybe_margin_bottom) {
      // If both "margin-top" and "margin-bottom" are "auto", solve the equation
      // under the extra constraint that the two margins get equal values...
      LayoutUnit vertical_margin = vertical_margin_sum / 2;
      set_margin_top(vertical_margin);
      set_margin_bottom(vertical_margin);
    } else if (!maybe_margin_top) {
      // If one of "margin-top" or "margin-bottom" is "auto", solve the equation
      // for that value.
      set_margin_top(vertical_margin_sum - *maybe_margin_bottom);
      set_margin_bottom(*maybe_margin_bottom);
    } else if (!maybe_margin_bottom) {
      // If one of "margin-top" or "margin-bottom" is "auto", solve the equation
      // for that value.
      set_margin_top(*maybe_margin_top);
      set_margin_bottom(vertical_margin_sum - *maybe_margin_top);
    } else {
      // If the values are over-constrained, ignore the value for "bottom".
      set_margin_top(*maybe_margin_top);
      set_margin_bottom(*maybe_margin_bottom);
    }
    return;
  }

  // Otherwise, set "auto" values for "margin-top" and "margin-bottom" to 0...
  set_margin_top(maybe_margin_top.value_or(LayoutUnit()));
  set_margin_bottom(maybe_margin_bottom.value_or(LayoutUnit()));

  // ...and pick the one of the following six rules that applies.

  // 1. "top" and "height" are "auto" and "bottom" is not "auto"...
  if (!maybe_top && !maybe_height && maybe_bottom) {
    // ...then the height is based on the content.
    set_height(formatting_context.auto_height());
    // Then solve for "top".
    set_top(containing_block_height - GetMarginBoxHeight() - *maybe_bottom);
    return;
  }

  // 2. "top" and "bottom" are "auto" and "height" is not "auto"...
  if (!maybe_top && !maybe_bottom && maybe_height) {
    set_height(*maybe_height);
    // ...then set "top" to the static position.
    set_top(GetStaticPositionTop());
    DCHECK_EQ(top(), top());  // Check for NaN.
    return;
  }

  // 3. "height" and "bottom" are "auto" and "top" is not "auto"...
  if (!maybe_height && !maybe_bottom && maybe_top) {
    set_top(*maybe_top);
    // ...then the height is based on the content.
    set_height(formatting_context.auto_height());
    return;
  }

  // 4. "top" is "auto", "height" and "bottom" are not "auto"...
  if (!maybe_top && maybe_height && maybe_bottom) {
    set_height(*maybe_height);
    // ...then solve for "top".
    set_top(containing_block_height - GetMarginBoxHeight() - *maybe_bottom);
    return;
  }

  // 5. "height" is "auto", "top" and "bottom" are not "auto"...
  if (!maybe_height && maybe_top && maybe_bottom) {
    set_top(*maybe_top);
    // ...then solve for "height".
    set_height(containing_block_height - *maybe_top - margin_top() -
               border_top_width() - padding_top() - padding_bottom() -
               border_bottom_width() - margin_bottom() - *maybe_bottom);
    return;
  }

  // 6. "bottom" is "auto", "top" and "height" are not "auto".
  if (!maybe_bottom && maybe_top && maybe_height) {
    set_top(*maybe_top);
    set_height(*maybe_height);
    return;
  }
}

// Based on https://www.w3.org/TR/CSS21/visudet.html#blockwidth.
//
// The following constraints must hold among the used values of the other
// properties:
//       "margin-left" + "border-left-width" + "padding-left"
//     + "width"
//     + "padding-right" + "border-right-width" + "margin-right"
//     = width of containing block
void BlockContainerBox::UpdateWidthAssumingBlockLevelInFlowBox(
    LayoutUnit containing_block_width,
    const base::optional<LayoutUnit>& maybe_width,
    const base::optional<LayoutUnit>& possibly_overconstrained_margin_left,
    const base::optional<LayoutUnit>& possibly_overconstrained_margin_right) {
  base::optional<LayoutUnit> maybe_margin_left =
      possibly_overconstrained_margin_left;
  base::optional<LayoutUnit> maybe_margin_right =
      possibly_overconstrained_margin_right;

  if (maybe_width) {
    set_width(*maybe_width);
    UpdateHorizontalMarginsAssumingBlockLevelInFlowBox(
        containing_block_width, GetBorderBoxWidth(), maybe_margin_left,
        maybe_margin_right);
  } else {
    // If "width" is set to "auto", any other "auto" values become "0" and
    // "width" follows from the resulting equality.
    set_margin_left(maybe_margin_left.value_or(LayoutUnit()));
    set_margin_right(maybe_margin_right.value_or(LayoutUnit()));

    set_width(containing_block_width - margin_left() - border_left_width() -
              padding_left() - padding_right() - border_right_width() -
              margin_right());
  }
}

void BlockContainerBox::UpdateWidthAssumingInlineLevelInFlowBox(
    LayoutUnit containing_block_width,
    const base::optional<LayoutUnit>& maybe_width,
    const base::optional<LayoutUnit>& maybe_margin_left,
    const base::optional<LayoutUnit>& maybe_margin_right,
    const base::optional<LayoutUnit>& maybe_height) {
  // A computed value of "auto" for "margin-left" or "margin-right" becomes
  // a used value of "0".
  //   https://www.w3.org/TR/CSS21/visudet.html#inlineblock-width
  set_margin_left(maybe_margin_left.value_or(LayoutUnit()));
  set_margin_right(maybe_margin_right.value_or(LayoutUnit()));

  if (!maybe_width) {
    // If "width" is "auto", the used value is the shrink-to-fit width.
    //   https://www.w3.org/TR/CSS21/visudet.html#inlineblock-width
    set_width(GetShrinkToFitWidth(containing_block_width, maybe_height));
  } else {
    set_width(*maybe_width);
  }
}

LayoutUnit BlockContainerBox::GetShrinkToFitWidth(
    LayoutUnit containing_block_width,
    const base::optional<LayoutUnit>& maybe_height) {
  LayoutParams child_layout_params;
  // The available width is the width of the containing block minus
  // the used values of "margin-left", "border-left-width", "padding-left",
  // "padding-right", "border-right-width", "margin-right".
  //   https://www.w3.org/TR/CSS21/visudet.html#shrink-to-fit-float
  child_layout_params.containing_block_size.set_width(
      containing_block_width - margin_left() - border_left_width() -
      padding_left() - padding_right() - border_right_width() - margin_right());
  // The "auto" height is not known yet but it shouldn't matter for in-flow
  // children, as per:
  //
  // If the height of the containing block is not specified explicitly (i.e.,
  // it depends on content height), and this element is not absolutely
  // positioned, the value [of "height"] computes to "auto".
  //   https://www.w3.org/TR/CSS21/visudet.html#the-height-property
  child_layout_params.containing_block_size.set_height(
      maybe_height.value_or(LayoutUnit()));
  // Although the spec does not mention it explicitly, Chromium operates under
  // the assumption that child block-level boxes must shrink instead of
  // expanding when calculating shrink-to-fit width of the parent box.
  child_layout_params.shrink_to_fit_width_forced = true;

  // Do a preliminary layout using the available width as a containing block
  // width. See |InlineFormattingContext::EndUpdates()| for details.
  //
  // TODO: Laying out the children twice has an exponential worst-case
  //       complexity (because every child could lay out itself twice as
  //       well). Figure out if there is a better way.
  scoped_ptr<FormattingContext> formatting_context =
      UpdateRectOfInFlowChildBoxes(child_layout_params);

  return formatting_context->shrink_to_fit_width();
}

// Based on https://www.w3.org/TR/CSS21/visudet.html#normal-block.
//
// TODO: Implement https://www.w3.org/TR/CSS21/visudet.html#block-root-margin
// when the margin collapsing is supported.
void BlockContainerBox::UpdateHeightAssumingInFlowBox(
    const base::optional<LayoutUnit>& maybe_height,
    const base::optional<LayoutUnit>& maybe_margin_top,
    const base::optional<LayoutUnit>& maybe_margin_bottom,
    const FormattingContext& formatting_context) {
  // If "margin-top", or "margin-bottom" are "auto", their used value is 0.
  set_margin_top(maybe_margin_top.value_or(LayoutUnit()));
  set_margin_bottom(maybe_margin_bottom.value_or(LayoutUnit()));

  // If "height" is "auto", the used value is the distance from box's top
  // content edge to the first applicable of the following:
  //     1. the bottom edge of the last line box, if the box establishes
  //        an inline formatting context with one or more lines;
  //     2. the bottom edge of the bottom margin of its last in-flow child.
  set_height(maybe_height.value_or(formatting_context.auto_height()));
}

}  // namespace layout
}  // namespace cobalt

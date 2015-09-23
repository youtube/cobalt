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

#include "cobalt/layout/block_container_box.h"

#include "cobalt/layout/formatting_context.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

BlockContainerBox::BlockContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider)
    : ContainerBox(computed_style, transitions, used_style_provider) {}

BlockContainerBox::~BlockContainerBox() {}

void BlockContainerBox::UpdateContentSizeAndMargins(
    const LayoutParams& layout_params) {
  bool width_depends_on_containing_block;
  base::optional<float> maybe_width = GetUsedWidthIfNotAuto(
      computed_style(), layout_params.containing_block_size,
      &width_depends_on_containing_block);
  base::optional<float> maybe_margin_left = GetUsedMarginLeftIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<float> maybe_margin_right = GetUsedMarginRightIfNotAuto(
      computed_style(), layout_params.containing_block_size);

  base::optional<float> maybe_height = GetUsedHeightIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<float> maybe_margin_top = GetUsedMarginTopIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<float> maybe_margin_bottom = GetUsedMarginBottomIfNotAuto(
      computed_style(), layout_params.containing_block_size);

  if (IsAbsolutelyPositioned()) {
    base::optional<float> maybe_left = GetUsedLeftIfNotAuto(
        computed_style(), layout_params.containing_block_size);
    base::optional<float> maybe_right = GetUsedRightIfNotAuto(
        computed_style(), layout_params.containing_block_size);

    UpdateWidthAssumingAbsolutelyPositionedBox(
        layout_params.containing_block_size.width(), maybe_left, maybe_right,
        maybe_width, maybe_margin_left, maybe_margin_right, maybe_height);
  } else {
    Level forced_level = GetLevel();
    if (layout_params.shrink_to_fit_width_forced) {
      forced_level = kInlineLevel;
      // Break circular dependency if needed.
      if (width_depends_on_containing_block) {
        maybe_width = base::nullopt;
      }
    }

    switch (forced_level) {
      case kBlockLevel:
        UpdateWidthAssumingBlockLevelInFlowBox(
            layout_params.containing_block_size.width(), maybe_width,
            maybe_margin_left, maybe_margin_right);
        break;
      case kInlineLevel:
        UpdateWidthAssumingInlineLevelInFlowBox(
            layout_params.containing_block_size.width(), maybe_width,
            maybe_margin_left, maybe_margin_right, maybe_height);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  LayoutParams child_layout_params;
  if (AsAnonymousBlockBox()) {
    // Anonymous block boxes are ignored when resolving percentage values
    // that would refer to it: the closest non-anonymous ancestor box is used
    // instead.
    //   http://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
    child_layout_params.containing_block_size =
        layout_params.containing_block_size;
  } else {
    // If the element's position is "relative" or "static", the containing block
    // is formed by the content edge of the nearest block container ancestor
    // box.
    //   http://www.w3.org/TR/CSS21/visudet.html#containing-block-details
    child_layout_params.containing_block_size.set_width(width());
    // The "auto" height is not known yet but it shouldn't matter for in-flow
    // children, as per:
    //
    // If the height of the containing block is not specified explicitly (i.e.,
    // it depends on content height), and this element is not absolutely
    // positioned, the value [of "height"] computes to "auto".
    //   http://www.w3.org/TR/CSS21/visudet.html#the-height-property
    child_layout_params.containing_block_size.set_height(
        maybe_height.value_or(0.0f));
  }
  scoped_ptr<FormattingContext> formatting_context =
      UpdateRectOfInFlowChildBoxes(child_layout_params);

  if (IsAbsolutelyPositioned()) {
    base::optional<float> maybe_top = GetUsedTopIfNotAuto(
        computed_style(), layout_params.containing_block_size);
    base::optional<float> maybe_bottom = GetUsedBottomIfNotAuto(
        computed_style(), layout_params.containing_block_size);

    UpdateHeightAssumingAbsolutelyPositionedBox(
        layout_params.containing_block_size.height(), maybe_top, maybe_bottom,
        maybe_height, maybe_margin_top, maybe_margin_bottom,
        *formatting_context);
  } else {
    UpdateHeightAssumingInFlowBox(maybe_height, maybe_margin_top,
                                  maybe_margin_bottom, *formatting_context);
  }

  // Positioned children are laid out at the end as their position and size
  // depends on the size of the containing block.
  child_layout_params.containing_block_size.set_height(height());
  UpdateRectOfPositionedChildBoxes(child_layout_params);

  if (formatting_context->maybe_baseline_offset_from_top_content_edge()) {
    maybe_baseline_offset_from_top_margin_edge_ =
        margin_top() + border_top_width() + padding_top() +
        *formatting_context->maybe_baseline_offset_from_top_content_edge();
  }
}

scoped_ptr<Box> BlockContainerBox::TrySplitAt(float /*available_width*/,
                                              bool /*allow_overflow*/) {
  return scoped_ptr<Box>();
}

scoped_ptr<Box> BlockContainerBox::TrySplitAtSecondBidiLevelRun() {
  return scoped_ptr<Box>();
}

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

bool BlockContainerBox::DoesTriggerLineBreak() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  return false;
}

bool BlockContainerBox::AffectsBaselineInBlockFormattingContext() const {
  return static_cast<bool>(maybe_baseline_offset_from_top_margin_edge_);
}

float BlockContainerBox::GetBaselineOffsetFromTopMarginEdge() const {
  return maybe_baseline_offset_from_top_margin_edge_.value_or(
      GetMarginBoxHeight());
}

scoped_ptr<ContainerBox> BlockContainerBox::TrySplitAtEnd() {
  return scoped_ptr<ContainerBox>();
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

// Based on http://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width.
//
// The constraint that determines the used values for these elements is:
//       "left" + "margin-left" + "border-left-width" + "padding-left"
//     + "width"
//     + "padding-right" + "border-right-width" + "margin-right" + "right"
//     = width of containing block
void BlockContainerBox::UpdateWidthAssumingAbsolutelyPositionedBox(
    float containing_block_width, const base::optional<float>& maybe_left,
    const base::optional<float>& maybe_right,
    const base::optional<float>& maybe_width,
    const base::optional<float>& maybe_margin_left,
    const base::optional<float>& maybe_margin_right,
    const base::optional<float>& maybe_height) {
  // If all three of "left", "width", and "right" are "auto":
  if (!maybe_left && !maybe_width && !maybe_right) {
    // First set any "auto" values for "margin-left" and "margin-right" to 0.
    set_margin_left(maybe_margin_left.value_or(0.0f));
    set_margin_right(maybe_margin_right.value_or(0.0f));

    // Then set "left" to the static position...
    // Nothing to do, "left" was set by the parent box.
    DCHECK_EQ(left(), left());  // Check for NaN.

    // ...and apply rule number three (the width is shrink-to-fit).
    set_width(GetShrinkToFitWidth(containing_block_width, maybe_height));
    return;
  }

  // If none of the three is "auto":
  if (maybe_left && maybe_width && maybe_right) {
    set_left(*maybe_left);
    set_width(*maybe_width);

    float horizontal_margin_sum = containing_block_width - *maybe_left -
                                  GetBorderBoxWidth() - *maybe_right;

    if (!maybe_margin_left && !maybe_margin_right) {
      // If both "margin-left" and "margin-right" are "auto", solve the equation
      // under the extra constraint that the two margins get equal values...
      float horizontal_margin = horizontal_margin_sum / 2;
      if (horizontal_margin < 0) {
        // ...unless this would make them negative, in which case set
        // "margin-left" to zero and solve for "margin-right".
        set_margin_left(0);
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
  set_margin_left(maybe_margin_left.value_or(0.0f));
  set_margin_right(maybe_margin_right.value_or(0.0f));

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
    // Nothing to do, "left" was set by the parent box.
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

// Based on http://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-height.
//
// The constraint that determines the used values for these elements is:
//       "top" + "margin-top" + "border-top-width" + "padding-top"
//     + "height"
//     + "padding-bottom" + "border-bottom-width" + "margin-bottom" + "bottom"
//     = height of containing block
void BlockContainerBox::UpdateHeightAssumingAbsolutelyPositionedBox(
    float containing_block_height, const base::optional<float>& maybe_top,
    const base::optional<float>& maybe_bottom,
    const base::optional<float>& maybe_height,
    const base::optional<float>& maybe_margin_top,
    const base::optional<float>& maybe_margin_bottom,
    const FormattingContext& formatting_context) {
  // If all three of "top", "height", and "bottom" are "auto":
  if (!maybe_top && !maybe_height && !maybe_bottom) {
    // First set any "auto" values for "margin-top" and "margin-bottom" to 0.
    set_margin_top(maybe_margin_top.value_or(0.0f));
    set_margin_bottom(maybe_margin_bottom.value_or(0.0f));

    // Then set "top" to the static position...
    // Nothing to do, "top" was set by the parent box.
    DCHECK_EQ(top(), top());  // Check for NaN.

    // ...and apply rule number three (the height is based on the content).
    set_height(formatting_context.auto_height());
    return;
  }

  // If none of the three is "auto":
  if (maybe_top && maybe_height && maybe_bottom) {
    set_top(*maybe_top);
    set_height(*maybe_height);

    float vertical_margin_sum = containing_block_height - *maybe_top -
                                GetBorderBoxHeight() - *maybe_bottom;

    if (!maybe_margin_top && !maybe_margin_bottom) {
      // If both "margin-top" and "margin-bottom" are "auto", solve the equation
      // under the extra constraint that the two margins get equal values...
      float vertical_margin = vertical_margin_sum / 2;
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
  set_margin_top(maybe_margin_top.value_or(0.0f));
  set_margin_bottom(maybe_margin_bottom.value_or(0.0f));

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
    // Nothing to do, "top" was set by the parent box.
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

// Based on http://www.w3.org/TR/CSS21/visudet.html#blockwidth.
//
// The following constraints must hold among the used values of the other
// properties:
//       "margin-left" + "border-left-width" + "padding-left"
//     + "width"
//     + "padding-right" + "border-right-width" + "margin-right"
//     = width of containing block
void BlockContainerBox::UpdateWidthAssumingBlockLevelInFlowBox(
    float containing_block_width, const base::optional<float>& maybe_width,
    const base::optional<float>& possibly_overconstrained_margin_left,
    const base::optional<float>& possibly_overconstrained_margin_right) {
  base::optional<float> maybe_margin_left =
      possibly_overconstrained_margin_left;
  base::optional<float> maybe_margin_right =
      possibly_overconstrained_margin_right;

  if (maybe_width) {
    set_width(*maybe_width);

    float border_box_width = GetBorderBoxWidth();

    // If "width" is not "auto" and "border-left-width" + "padding-left" +
    // "width" + "padding-right" + "border-right-width" (plus any of
    // "margin-left" or "margin-right" that are not "auto") is larger than
    // the width of the containing block, then any "auto" values for
    // "margin-left" or "margin-right" are, for the following rules, treated
    // as zero.
    if (maybe_margin_left.value_or(0.0f) + border_box_width +
            maybe_margin_right.value_or(0.0f) >
        containing_block_width) {
      maybe_margin_left = maybe_margin_left.value_or(0.0f);
      maybe_margin_right = maybe_margin_right.value_or(0.0f);
    }

    if (maybe_margin_left) {
      // If all of the above have a computed value other than "auto", the values
      // are said to be "over-constrained" and the specified value of
      // "margin-right" is ignored and the value is calculated so as to make
      // the equality true.
      //
      // If there is exactly one value specified as "auto", its used value
      // follows from the equality.
      set_margin_left(*maybe_margin_left);
      set_margin_right(containing_block_width - *maybe_margin_left -
                       border_box_width);
    } else if (maybe_margin_right) {
      // If there is exactly one value specified as "auto", its used value
      // follows from the equality.
      set_margin_left(containing_block_width - border_box_width -
                      *maybe_margin_right);
      set_margin_right(*maybe_margin_right);
    } else {
      // If both "margin-left" and "margin-right" are "auto", their used values
      // are equal.
      float horizontal_margin = (containing_block_width - border_box_width) / 2;
      set_margin_left(horizontal_margin);
      set_margin_right(horizontal_margin);
    }
  } else {
    // If "width" is set to "auto", any other "auto" values become "0" and
    // "width" follows from the resulting equality.
    set_margin_left(maybe_margin_left.value_or(0.0f));
    set_margin_right(maybe_margin_right.value_or(0.0f));

    set_width(containing_block_width - margin_left() - border_left_width() -
              padding_left() - padding_right() - border_right_width() -
              margin_right());
  }
}

void BlockContainerBox::UpdateWidthAssumingInlineLevelInFlowBox(
    float containing_block_width, const base::optional<float>& maybe_width,
    const base::optional<float>& maybe_margin_left,
    const base::optional<float>& maybe_margin_right,
    const base::optional<float>& maybe_height) {
  // A computed value of "auto" for "margin-left" or "margin-right" becomes
  // a used value of "0".
  //   http://www.w3.org/TR/CSS21/visudet.html#inlineblock-width
  set_margin_left(maybe_margin_left.value_or(0.0f));
  set_margin_right(maybe_margin_right.value_or(0.0f));

  if (!maybe_width) {
    // If "width" is "auto", the used value is the shrink-to-fit width.
    //   http://www.w3.org/TR/CSS21/visudet.html#inlineblock-width
    set_width(GetShrinkToFitWidth(containing_block_width, maybe_height));
  } else {
    set_width(*maybe_width);
  }
}

float BlockContainerBox::GetShrinkToFitWidth(
    float containing_block_width, const base::optional<float>& maybe_height) {
  LayoutParams child_layout_params;
  // The available width is the width of the containing block minus
  // the used values of "margin-left", "border-left-width", "padding-left",
  // "padding-right", "border-right-width", "margin-right".
  //   http://www.w3.org/TR/CSS21/visudet.html#shrink-to-fit-float
  child_layout_params.containing_block_size.set_width(
      containing_block_width - margin_left() - border_left_width() -
      padding_left() - padding_right() - border_right_width() - margin_right());
  // The "auto" height is not known yet but it shouldn't matter for in-flow
  // children, as per:
  //
  // If the height of the containing block is not specified explicitly (i.e.,
  // it depends on content height), and this element is not absolutely
  // positioned, the value [of "height"] computes to "auto".
  //   http://www.w3.org/TR/CSS21/visudet.html#the-height-property
  child_layout_params.containing_block_size.set_height(
      maybe_height.value_or(0.0f));
  // Although the spec does not mention it explicitly, Chromium operates under
  // the assumption that child block-level boxes must shrink instead of
  // expanding when calculating shrink-to-fit width of the parent box.
  child_layout_params.shrink_to_fit_width_forced = true;

  // Do a preliminary layout using the available width as a containing block
  // width. See |InlineFormattingContext::EndUpdates()| for details.
  //
  // TODO(***REMOVED***): Laying out the children twice has an exponential
  //               worst-case complexity (because every child could lay out
  //               itself twice as well). Figure out if there is a better
  //               way.
  scoped_ptr<FormattingContext> formatting_context =
      UpdateRectOfInFlowChildBoxes(child_layout_params);

  return formatting_context->shrink_to_fit_width();
}

// Based on http://www.w3.org/TR/CSS21/visudet.html#normal-block.
//
// TODO(***REMOVED***): Implement
//               http://www.w3.org/TR/CSS21/visudet.html#block-root-margin when
//               the margin collapsing is supported.
void BlockContainerBox::UpdateHeightAssumingInFlowBox(
    const base::optional<float>& maybe_height,
    const base::optional<float>& maybe_margin_top,
    const base::optional<float>& maybe_margin_bottom,
    const FormattingContext& formatting_context) {
  // If "margin-top", or "margin-bottom" are "auto", their used value is 0.
  set_margin_top(maybe_margin_top.value_or(0.0f));
  set_margin_bottom(maybe_margin_bottom.value_or(0.0f));

  // If "height" is "auto", the used value is the distance from box's top
  // content edge to the first applicable of the following:
  //     1. the bottom edge of the last line box, if the box establishes
  //        an inline formatting context with one or more lines;
  //     2. the bottom edge of the bottom margin of its last in-flow child.
  set_height(maybe_height.value_or(formatting_context.auto_height()));
}

}  // namespace layout
}  // namespace cobalt

// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/layout/block_formatting_context.h"

#include <algorithm>

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/box.h"

namespace cobalt {
namespace layout {

BlockFormattingContext::BlockFormattingContext(
    const LayoutParams& layout_params, const bool is_margin_collapsable)
    : is_margin_collapsable_(is_margin_collapsable),
      layout_params_(layout_params),
      collapsing_margin_(0) {}

BlockFormattingContext::~BlockFormattingContext() {}

void BlockFormattingContext::UpdateRect(Box* child_box) {
  DCHECK(!child_box->IsAbsolutelyPositioned());

  child_box->UpdateSize(layout_params_);
  UpdatePosition(child_box);

  // Shrink-to-fit width cannot be less than the width of the widest child.
  //   https://www.w3.org/TR/CSS21/visudet.html#float-width
  set_shrink_to_fit_width(
      std::max(shrink_to_fit_width(),
               child_box->GetMarginBoxRightEdgeOffsetFromContainingBlock()));

  // If "height" is "auto", the used value is the distance from box's top
  // content edge to the bottom edge of the bottom margin of its last in-flow
  // child.
  //   https://www.w3.org/TR/CSS21/visudet.html#normal-block
  set_auto_height(child_box->GetMarginBoxBottomEdgeOffsetFromContainingBlock());
  collapsing_margin_ = child_box->margin_bottom();
  if (!context_margin_top_) {
    context_margin_top_ = child_box->margin_top();
  }

  // The baseline of an "inline-block" is the baseline of its last line box
  // in the normal flow, unless it has no in-flow line boxes.
  //   https://www.w3.org/TR/CSS21/visudet.html#line-height
  if (child_box->AffectsBaselineInBlockFormattingContext()) {
    set_baseline_offset_from_top_content_edge(
        child_box->top() + child_box->GetBaselineOffsetFromTopMarginEdge());
  }
}

void BlockFormattingContext::EstimateStaticPosition(Box* child_box) {
  DCHECK(child_box->IsAbsolutelyPositioned());

  // The term "static position" (of an element) refers, roughly, to the position
  // an element would have had in the normal flow.
  //   https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width
  child_box->set_left(LayoutUnit());
}

void BlockFormattingContext::UpdatePosition(Box* child_box) {
  DCHECK_EQ(Box::kBlockLevel, child_box->GetLevel());

  // In a block formatting context, each box's left outer edge touches
  // the left edge of the containing block.
  //   https://www.w3.org/TR/CSS21/visuren.html#block-formatting
  child_box->set_left(LayoutUnit());

  LayoutUnit margin_top = child_box->margin_top();

  // For first child, if top margin will collapse with parent's top margin,
  // parent will handle margin positioning for both itself and the child.
  if (!context_margin_top_ && is_margin_collapsable_) {
    child_box->set_top(auto_height() - margin_top);
    return;
  }

  // Collapse top margin with previous sibling's bottom margin.
  LayoutUnit collapsed_margin = CollapseMargins(margin_top, collapsing_margin_);
  LayoutUnit combined_margin = collapsing_margin_ + margin_top;
  LayoutUnit position_difference = combined_margin - collapsed_margin;
  child_box->set_top(auto_height() - position_difference);
}

void BlockFormattingContext::CollapseContainingMargins(Box* containing_box) {
  // If no in-flow children, do not collapse margins.
  if (!context_margin_top_) {
    return;
  }

  LayoutUnit margin_top =
      layout_params_.maybe_margin_top.value_or(LayoutUnit());
  LayoutUnit margin_bottom =
      layout_params_.maybe_margin_bottom.value_or(LayoutUnit());

  // Collapse top margin with top margin of first in-flow child.
  if (containing_box->padding_top() == LayoutUnit()) {
    LayoutUnit collapsed_margin_top =
        CollapseMargins(margin_top, context_margin_top_.value_or(LayoutUnit()));
    containing_box->collapsed_margin_top_ = collapsed_margin_top;
  }

  // If height is auto, collapse bottom margin with bottom margin of last
  // in-flow child.
  if (!layout_params_.maybe_height &&
      containing_box->padding_bottom() == LayoutUnit()) {
    LayoutUnit collapsed_margin_bottom =
        CollapseMargins(margin_bottom, collapsing_margin_);
    containing_box->collapsed_margin_bottom_ = collapsed_margin_bottom;
    set_auto_height(auto_height() - collapsing_margin_);
  }
}

LayoutUnit BlockFormattingContext::CollapseMargins(
    const LayoutUnit box_margin, const LayoutUnit adjoining_margin) {
  // In a block formatting context, boxes are laid out one after the other,
  // vertically, beginning at the top of a containing block. The vertical
  // distance between two sibling boxes is determined by the "margin"
  // properties. Vertical margins between adjacent block-level boxes in a block
  // formatting context collapse.
  //   https://www.w3.org/TR/CSS21/visuren.html#block-formatting

  // When two or more margins collapse, the resulting margin width is the
  // maximum of the collapsing margins' widths.
  //   https://www.w3.org/TR/CSS21/box.html#collapsing-margins
  LayoutUnit collapsed_margin;
  if ((box_margin >= LayoutUnit()) && (adjoining_margin >= LayoutUnit())) {
    collapsed_margin = std::max(box_margin, adjoining_margin);
  } else if ((box_margin < LayoutUnit()) && (adjoining_margin < LayoutUnit())) {
    // If there are no positive margins, the maximum of the absolute values of
    // the adjoining margins is deducted from zero.
    collapsed_margin = LayoutUnit() + std::min(box_margin, adjoining_margin);
  } else {
    // In the case of negative margins, the maximum of the absolute values of
    // the negative adjoining margins is deducted from the maximum of the
    // positive adjoining margins.
    // When there is only one negative and one positive margin, that translates
    // to: The margins are summed.
    DCHECK(adjoining_margin.GreaterEqualOrNaN(LayoutUnit()) ||
           box_margin.GreaterEqualOrNaN(LayoutUnit()));
    collapsed_margin = adjoining_margin + box_margin;
  }

  return collapsed_margin;
}

}  // namespace layout
}  // namespace cobalt

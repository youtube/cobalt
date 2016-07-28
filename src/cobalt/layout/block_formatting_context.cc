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

#include "cobalt/layout/block_formatting_context.h"

#include <algorithm>

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/box.h"

namespace cobalt {
namespace layout {

BlockFormattingContext::BlockFormattingContext(
    const LayoutParams& layout_params)
    : layout_params_(layout_params), collapsing_margin_(0) {}

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
  UpdatePosition(child_box);
}

void BlockFormattingContext::UpdatePosition(Box* child_box) {
  DCHECK_EQ(Box::kBlockLevel, child_box->GetLevel());

  // In a block formatting context, each box's left outer edge touches
  // the left edge of the containing block.
  //   https://www.w3.org/TR/CSS21/visuren.html#block-formatting
  child_box->set_left(LayoutUnit());

  // In a block formatting context, boxes are laid out one after the other,
  // vertically, beginning at the top of a containing block. The vertical
  // distance between two sibling boxes is determined by the "margin"
  // properties. Vertical margins between adjacent block-level boxes in a block
  // formatting context collapse.
  //   https://www.w3.org/TR/CSS21/visuren.html#block-formatting

  // When two or more margins collapse, the resulting margin width is the
  // maximum of the collapsing margins' widths.
  //   https://www.w3.org/TR/CSS21/box.html#collapsing-margins
  const LayoutUnit margin_top = child_box->margin_top();
  LayoutUnit collapsed_margin;
  if ((margin_top >= LayoutUnit()) && (collapsing_margin_ >= LayoutUnit())) {
    collapsed_margin = std::max(margin_top, collapsing_margin_);
  } else if ((margin_top < LayoutUnit()) &&
             (collapsing_margin_ < LayoutUnit())) {
    // If there are no positive margins, the maximum of the absolute values of
    // the adjoining margins is deducted from zero.
    collapsed_margin = LayoutUnit() + std::min(margin_top, collapsing_margin_);
  } else {
    // In the case of negative margins, the maximum of the absolute values of
    // the negative adjoining margins is deducted from the maximum of the
    // positive adjoining margins.
    // When there is only one negative and one positive margin, that translates
    // to: The margins are summed.
    DCHECK(collapsing_margin_ >= LayoutUnit() || margin_top >= LayoutUnit());
    collapsed_margin = collapsing_margin_ + margin_top;
  }

  LayoutUnit combined_margin = collapsing_margin_ + margin_top;
  child_box->set_top(auto_height() - combined_margin + collapsed_margin);
}

}  // namespace layout
}  // namespace cobalt

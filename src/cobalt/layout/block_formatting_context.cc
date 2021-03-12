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
    const LayoutParams& layout_params, const bool is_margin_collapsible)
    : layout_params_(layout_params),
      margin_collapsing_params_(MarginCollapsingParams(is_margin_collapsible)) {
}

BlockFormattingContext::~BlockFormattingContext() {}

void BlockFormattingContext::UpdateRect(Box* child_box) {
  DCHECK(!child_box->IsAbsolutelyPositioned());

  child_box->UpdateSize(layout_params_);

  // In a block formatting context, each box's left outer edge touches
  // the left edge of the containing block.
  //   https://www.w3.org/TR/CSS21/visuren.html#block-formatting
  child_box->set_left(LayoutUnit());
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

  // The baseline of an "inline-block" is the baseline of its last line box
  // in the normal flow, unless it has no in-flow line boxes.
  //   https://www.w3.org/TR/CSS21/visudet.html#line-height
  if (child_box->AffectsBaselineInBlockFormattingContext()) {
    set_baseline_offset_from_top_content_edge(
        child_box->top() + child_box->GetBaselineOffsetFromTopMarginEdge());
  }
}

void BlockFormattingContext::UpdatePosition(Box* child_box) {
  DCHECK_EQ(Box::kBlockLevel, child_box->GetLevel());

  switch (child_box->GetMarginCollapsingStatus()) {
    case Box::kIgnore:
      child_box->set_top(auto_height());
      break;
    case Box::kSeparateAdjoiningMargins:
      child_box->set_top(auto_height());
      margin_collapsing_params_.collapsing_margin = LayoutUnit();
      margin_collapsing_params_.should_collapse_margin_bottom = false;
      if (!margin_collapsing_params_.context_margin_top) {
        margin_collapsing_params_.should_collapse_margin_top = false;
      }
      margin_collapsing_params_.should_collapse_own_margins_together = false;
      break;
    case Box::kCollapseMargins:
      margin_collapsing_params_.should_collapse_margin_bottom = true;

      // For first child, if top margin will collapse with parent's top margin,
      // parent will handle margin positioning for both itself and the child.
      if (!margin_collapsing_params_.context_margin_top &&
          margin_collapsing_params_.should_collapse_margin_top) {
        child_box->set_top(auto_height() - child_box->margin_top());
        if (child_box->collapsed_empty_margin_) {
          margin_collapsing_params_.should_collapse_margin_bottom = false;
        }
      } else {
        // Collapse top margin with previous sibling's bottom margin.
        LayoutUnit collapsed_margin =
            CollapseMargins(child_box->margin_top(),
                            margin_collapsing_params_.collapsing_margin);
        LayoutUnit combined_margin =
            margin_collapsing_params_.collapsing_margin +
            child_box->margin_top();
        LayoutUnit position_difference = combined_margin - collapsed_margin;
        child_box->set_top(auto_height() - position_difference);
      }

      // Collapse margins for in-flow siblings.
      margin_collapsing_params_.collapsing_margin =
          child_box->collapsed_empty_margin_.value_or(
              child_box->margin_bottom());
      if (!margin_collapsing_params_.context_margin_top) {
        margin_collapsing_params_.context_margin_top = child_box->margin_top();
      }
      break;
  }
}

void BlockFormattingContext::CollapseContainingMargins(Box* containing_box) {
  bool has_padding_top = containing_box->padding_top() != LayoutUnit();
  bool has_border_top = containing_box->border_top_width() != LayoutUnit();
  bool has_padding_bottom = containing_box->padding_bottom() != LayoutUnit();
  bool has_border_bottom =
      containing_box->border_bottom_width() != LayoutUnit();
  LayoutUnit margin_top =
      layout_params_.maybe_margin_top.value_or(LayoutUnit());
  LayoutUnit margin_bottom =
      layout_params_.maybe_margin_bottom.value_or(LayoutUnit());

  // If no in-flow children, do not collapse margins with children.
  if (!margin_collapsing_params_.context_margin_top) {
    // Empty boxes with auto or 0 height collapse top/bottom margins together.
    //   https://www.w3.org/TR/CSS22/box.html#collapsing-margins
    if (!has_padding_top && !has_border_top && !has_padding_bottom &&
        !has_border_bottom &&
        layout_params_.containing_block_size.height() == LayoutUnit() &&
        margin_collapsing_params_.should_collapse_own_margins_together) {
      containing_box->collapsed_empty_margin_ =
          CollapseMargins(margin_top, margin_bottom);
      return;
    }
    // Reset in case min-height iteration reverses 0/auto height criteria.
    containing_box->collapsed_empty_margin_.reset();
    return;
  }

  // Collapse top margin with top margin of first in-flow child.
  if (!has_padding_top && !has_border_top &&
      margin_collapsing_params_.should_collapse_margin_top) {
    LayoutUnit collapsed_margin_top = CollapseMargins(
        margin_top,
        margin_collapsing_params_.context_margin_top.value_or(LayoutUnit()));
    containing_box->collapsed_margin_top_ = collapsed_margin_top;
  }

  // If height is auto, collapse bottom margin with bottom margin of last
  // in-flow child.
  if (!layout_params_.maybe_height && !has_padding_bottom &&
      !has_border_bottom &&
      margin_collapsing_params_.should_collapse_margin_bottom) {
    LayoutUnit collapsed_margin_bottom = CollapseMargins(
        margin_bottom, margin_collapsing_params_.collapsing_margin);
    containing_box->collapsed_margin_bottom_ = collapsed_margin_bottom;
    set_auto_height(auto_height() -
                    margin_collapsing_params_.collapsing_margin);
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

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

#include "cobalt/layout/block_formatting_block_container_box.h"

#include <algorithm>
#include <memory>

#include "cobalt/cssom/computed_style.h"
#include "cobalt/cssom/computed_style_utils.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/anonymous_block_box.h"
#include "cobalt/layout/block_formatting_context.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/layout/white_space_processing.h"

namespace cobalt {
namespace layout {

BlockFormattingBlockContainerBox::BlockFormattingBlockContainerBox(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        css_computed_style_declaration,
    BaseDirection base_direction, UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker)
    : BlockContainerBox(css_computed_style_declaration, base_direction,
                        used_style_provider, layout_stat_tracker) {}

bool BlockFormattingBlockContainerBox::TryAddChild(
    const scoped_refptr<Box>& child_box) {
  AddChild(child_box);
  return true;
}

void BlockFormattingBlockContainerBox::AddChild(
    const scoped_refptr<Box>& child_box) {
  switch (child_box->GetLevel()) {
    case kBlockLevel:
      if (!child_box->IsAbsolutelyPositioned()) {
        PushBackDirectChild(child_box);
        break;
      }
    // Fall through if child is out-of-flow.

    case kInlineLevel:
      // An inline formatting context required,
      // add a child to an anonymous block box.
      GetOrAddAnonymousBlockBox()->AddInlineLevelChild(child_box);
      break;
  }
}

std::unique_ptr<FormattingContext>
BlockFormattingBlockContainerBox::UpdateRectOfInFlowChildBoxes(
    const LayoutParams& child_layout_params) {
  // Only collapse in-flow, block-level boxes. Do not collapse root element and
  // the initial containing block. Do not collapse boxes with overflow not equal
  // to 'visible', because these create new formatting contexts.
  bool is_collapsible =
      !IsAbsolutelyPositioned() && GetLevel() == Box::kBlockLevel && parent() &&
      parent()->parent() && !IsOverflowCropped(computed_style());

  // Margins should only collapse if no padding or border separate them.
  //   https://www.w3.org/TR/CSS22/box.html#collapsing-margins
  bool top_margin_is_collapsible = is_collapsible &&
                                   padding_top() == LayoutUnit() &&
                                   border_top_width() == LayoutUnit();
  // Lay out child boxes in the normal flow.
  //   https://www.w3.org/TR/CSS21/visuren.html#normal-flow
  std::unique_ptr<BlockFormattingContext> block_formatting_context(
      new BlockFormattingContext(child_layout_params,
                                 top_margin_is_collapsible));
  for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    block_formatting_context->UpdateRect(child_box);
  }

  if (is_collapsible) {
    block_formatting_context->CollapseContainingMargins(this);
  }

  return std::unique_ptr<FormattingContext>(block_formatting_context.release());
}

AnonymousBlockBox*
BlockFormattingBlockContainerBox::GetLastChildAsAnonymousBlockBox() {
  return child_boxes().empty() ? NULL
                               : child_boxes().back()->AsAnonymousBlockBox();
}

AnonymousBlockBox*
BlockFormattingBlockContainerBox::GetOrAddAnonymousBlockBox() {
  AnonymousBlockBox* anonymous_block_box = GetLastChildAsAnonymousBlockBox();
  // If either the last box is not an anonymous block box, or the anonymous
  // block box already has a trailing line break and can't accept any additional
  // children, then create a new anonymous block box.
  if (anonymous_block_box == NULL ||
      anonymous_block_box->HasTrailingLineBreak()) {
    // TODO: Determine which animations to propagate to the anonymous block box,
    //       instead of none at all.
    scoped_refptr<cssom::CSSComputedStyleDeclaration>
        new_computed_style_declaration =
            new cssom::CSSComputedStyleDeclaration();
    new_computed_style_declaration->SetData(
        GetComputedStyleOfAnonymousBox(css_computed_style_declaration()));
    new_computed_style_declaration->set_animations(
        new web_animations::AnimationSet());
    scoped_refptr<AnonymousBlockBox> new_anonymous_block_box(
        new AnonymousBlockBox(new_computed_style_declaration, base_direction(),
                              used_style_provider(), layout_stat_tracker()));
    anonymous_block_box = new_anonymous_block_box.get();
    PushBackDirectChild(new_anonymous_block_box);
  }
  return anonymous_block_box;
}

BlockLevelBlockContainerBox::BlockLevelBlockContainerBox(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        css_computed_style_declaration,
    BaseDirection base_direction, UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker)
    : BlockFormattingBlockContainerBox(css_computed_style_declaration,
                                       base_direction, used_style_provider,
                                       layout_stat_tracker) {}

BlockLevelBlockContainerBox::~BlockLevelBlockContainerBox() {}

Box::Level BlockLevelBlockContainerBox::GetLevel() const { return kBlockLevel; }

#ifdef COBALT_BOX_DUMP_ENABLED

void BlockLevelBlockContainerBox::DumpClassName(std::ostream* stream) const {
  *stream << "BlockLevelBlockContainerBox ";
}

#endif  // COBALT_BOX_DUMP_ENABLED

InlineLevelBlockContainerBox::InlineLevelBlockContainerBox(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        css_computed_style_declaration,
    BaseDirection base_direction, const scoped_refptr<Paragraph>& paragraph,
    int32 text_position, UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker)
    : BlockFormattingBlockContainerBox(css_computed_style_declaration,
                                       base_direction, used_style_provider,
                                       layout_stat_tracker),
      paragraph_(paragraph),
      text_position_(text_position),
      is_hidden_by_ellipsis_(false),
      was_hidden_by_ellipsis_(false) {}

InlineLevelBlockContainerBox::~InlineLevelBlockContainerBox() {}

Box::Level InlineLevelBlockContainerBox::GetLevel() const {
  return kInlineLevel;
}

WrapResult InlineLevelBlockContainerBox::TryWrapAt(
    WrapAtPolicy wrap_at_policy, WrapOpportunityPolicy wrap_opportunity_policy,
    bool is_line_existence_justified, LayoutUnit available_width,
    bool should_collapse_trailing_white_space) {
  // NOTE: This logic must stay in sync with ReplacedBox::TryWrapAt().
  DCHECK(!IsAbsolutelyPositioned());

  // Wrapping is not allowed until the line's existence is justified, meaning
  // that wrapping cannot occur before the box. Given that this box cannot be
  // split, no wrappable point is available.
  if (!is_line_existence_justified) {
    return kWrapResultNoWrap;
  }

  // Atomic inline elements participate in the inline formatting context as a
  // single opaque box. Therefore, the parent's style should be used, as the
  // internals of the atomic inline element have no impact on the formatting of
  // the line.
  // https://www.w3.org/TR/CSS21/visuren.html#inline-boxes
  if (!parent()) {
    return kWrapResultNoWrap;
  }

  bool style_allows_break_word = parent()->computed_style()->overflow_wrap() ==
                                 cssom::KeywordValue::GetBreakWord();

  if (!ShouldProcessWrapOpportunityPolicy(wrap_opportunity_policy,
                                          style_allows_break_word)) {
    return kWrapResultNoWrap;
  }

  // Even when the style prevents wrapping, wrapping can still occur before the
  // box if the line's existence has already been justified and whitespace
  // precedes it.
  if (!DoesAllowTextWrapping(parent()->computed_style()->white_space())) {
    if (text_position_ > 0 &&
        paragraph_->IsCollapsibleWhiteSpace(text_position_ - 1)) {
      return kWrapResultWrapBefore;
    } else {
      return kWrapResultNoWrap;
    }
  }

  Paragraph::BreakPolicy break_policy =
      Paragraph::GetBreakPolicyFromWrapOpportunityPolicy(
          wrap_opportunity_policy, style_allows_break_word);
  return paragraph_->IsBreakPosition(text_position_, break_policy)
             ? kWrapResultWrapBefore
             : kWrapResultNoWrap;
}

base::Optional<int> InlineLevelBlockContainerBox::GetBidiLevel() const {
  return paragraph_->GetBidiLevel(text_position_);
}

bool InlineLevelBlockContainerBox::DoesFulfillEllipsisPlacementRequirement()
    const {
  // This box fulfills the requirement that the first character or inline-level
  // element must appear on the line before ellipsing can occur.
  //   https://www.w3.org/TR/css3-ui/#propdef-text-overflow
  return true;
}

void InlineLevelBlockContainerBox::DoPreEllipsisPlacementProcessing() {
  was_hidden_by_ellipsis_ = is_hidden_by_ellipsis_;
  is_hidden_by_ellipsis_ = false;
}

void InlineLevelBlockContainerBox::DoPostEllipsisPlacementProcessing() {
  if (was_hidden_by_ellipsis_ != is_hidden_by_ellipsis_) {
    InvalidateRenderTreeNodesOfBoxAndAncestors();
  }
}

bool InlineLevelBlockContainerBox::IsHiddenByEllipsis() const {
  return is_hidden_by_ellipsis_;
}

#ifdef COBALT_BOX_DUMP_ENABLED

void InlineLevelBlockContainerBox::DumpClassName(std::ostream* stream) const {
  *stream << "InlineLevelBlockContainerBox ";
}

void InlineLevelBlockContainerBox::DumpProperties(std::ostream* stream) const {
  BlockContainerBox::DumpProperties(stream);

  *stream << "text_position=" << text_position_ << " "
          << "bidi_level=" << paragraph_->GetBidiLevel(text_position_) << " ";
}

#endif  // COBALT_BOX_DUMP_ENABLED

void InlineLevelBlockContainerBox::DoPlaceEllipsisOrProcessPlacedEllipsis(
    BaseDirection base_direction, LayoutUnit desired_offset,
    bool* is_placement_requirement_met, bool* is_placed,
    LayoutUnit* placed_offset) {
  // If the ellipsis is already placed, then simply mark the box as hidden by
  // the ellipsis: "Implementations must hide characters and atomic inline-level
  // elements at the applicable edge(s) of the line as necessary to fit the
  // ellipsis."
  //   https://www.w3.org/TR/css3-ui/#propdef-text-overflow
  if (*is_placed) {
    is_hidden_by_ellipsis_ = true;
    // Otherwise, the box is placing the ellipsis.
  } else {
    *is_placed = true;

    // The first character or atomic inline-level element on a line must be
    // clipped rather than ellipsed.
    //   https://www.w3.org/TR/css3-ui/#propdef-text-overflow
    // If this requirement has been met, then place the ellipsis at the start
    // edge of atomic inline-level element, as it should be fully hidden.
    if (*is_placement_requirement_met) {
      *placed_offset =
          GetMarginBoxStartEdgeOffsetFromContainingBlock(base_direction);
      is_hidden_by_ellipsis_ = true;
      // Otherwise, this box is fulfilling the required first inline-level
      // element and the ellipsis must be added at the end edge.
    } else {
      *placed_offset =
          GetMarginBoxEndEdgeOffsetFromContainingBlock(base_direction);
    }
  }
}

}  // namespace layout
}  // namespace cobalt

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

#ifndef COBALT_LAYOUT_INLINE_CONTAINER_BOX_H_
#define COBALT_LAYOUT_INLINE_CONTAINER_BOX_H_

#include "cobalt/dom/font_list.h"
#include "cobalt/layout/container_box.h"

namespace cobalt {
namespace layout {

// The CSS 2.1 specification defines an inline box as an inline-level box whose
// contents participate in its containing inline formatting context. In fact,
// this definition matches two different types of boxes:
//   - a text box;
//   - an inline-level container box that contains other inline-level boxes
//     (including text boxes).
// This class implements the latter.
//
// Note that "inline box" and "inline-level box" are two different concepts.
// Inline-level boxes that are not inline boxes (such as inline-block elements)
// are called atomic inline-level boxes because they participate in their inline
// formatting context as a single opaque box.
//   https://www.w3.org/TR/CSS21/visuren.html#inline-boxes
class InlineContainerBox : public ContainerBox {
 public:
  InlineContainerBox(const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
                         css_computed_style_declaration,
                     UsedStyleProvider* used_style_provider,
                     LayoutStatTracker* layout_stat_tracker);
  ~InlineContainerBox() override;

  // From |Box|.
  Level GetLevel() const override;

  void UpdateContentSizeAndMargins(const LayoutParams& layout_params) override;

  WrapResult TryWrapAt(WrapAtPolicy wrap_at_policy,
                       WrapOpportunityPolicy wrap_opportunity_policy,
                       bool is_line_existence_justified,
                       LayoutUnit available_width,
                       bool should_collapse_trailing_white_space) override;

  Box* GetSplitSibling() const override;

  bool DoesFulfillEllipsisPlacementRequirement() const override;
  void DoPreEllipsisPlacementProcessing() override;
  void DoPostEllipsisPlacementProcessing() override;

  bool TrySplitAtSecondBidiLevelRun() override;
  base::optional<int> GetBidiLevel() const override;

  void SetShouldCollapseLeadingWhiteSpace(
      bool should_collapse_leading_white_space) override;
  void SetShouldCollapseTrailingWhiteSpace(
      bool should_collapse_trailing_white_space) override;
  bool HasLeadingWhiteSpace() const override;
  bool HasTrailingWhiteSpace() const override;
  bool IsCollapsed() const override;

  bool JustifiesLineExistence() const override;
  bool HasTrailingLineBreak() const override;
  bool AffectsBaselineInBlockFormattingContext() const override;
  LayoutUnit GetBaselineOffsetFromTopMarginEdge() const override;
  LayoutUnit GetInlineLevelBoxHeight() const override;
  LayoutUnit GetInlineLevelTopMargin() const override;

  // From |ContainerBox|.
  bool TryAddChild(const scoped_refptr<Box>& child_box) override;
  scoped_refptr<ContainerBox> TrySplitAtEnd() override;

 protected:
  // From |Box|.
  bool IsTransformable() const override;

#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpClassName(std::ostream* stream) const override;
  void DumpProperties(std::ostream* stream) const override;
#endif  // COBALT_BOX_DUMP_ENABLED

 private:
  // From |Box|.
  void DoPlaceEllipsisOrProcessPlacedEllipsis(
      BaseDirection base_direction, LayoutUnit desired_offset,
      bool* is_placement_requirement_met, bool* is_placed,
      LayoutUnit* placed_offset) override;

  WrapResult TryWrapAtBefore(WrapOpportunityPolicy wrap_opportunity_policy,
                             bool is_line_requirement_met,
                             LayoutUnit available_width,
                             bool should_collapse_trailing_white_space);
  WrapResult TryWrapAtLastOpportunityWithinWidth(
      WrapOpportunityPolicy wrap_opportunity_policy,
      bool is_line_requirement_met, LayoutUnit available_width,
      bool should_collapse_trailing_white_space);
  WrapResult TryWrapAtLastOpportunityBeforeIndex(
      size_t index, WrapOpportunityPolicy wrap_opportunity_policy,
      bool is_line_requirement_met, LayoutUnit available_width,
      bool should_collapse_trailing_white_space);
  WrapResult TryWrapAtFirstOpportunity(
      WrapOpportunityPolicy wrap_opportunity_policy,
      bool is_line_requirement_met, LayoutUnit available_width,
      bool should_collapse_trailing_white_space);
  WrapResult TryWrapAtIndex(size_t wrap_index, WrapAtPolicy wrap_at_policy,
                            WrapOpportunityPolicy wrap_opportunity_policy,
                            bool is_line_requirement_met,
                            LayoutUnit available_width,
                            bool should_collapse_trailing_white_space);

  void SplitAtIterator(Boxes::const_iterator child_split_iterator);

  bool should_collapse_leading_white_space_;
  bool should_collapse_trailing_white_space_;
  bool has_leading_white_space_;
  bool has_trailing_white_space_;
  bool is_collapsed_;

  bool justifies_line_existence_;
  size_t first_box_justifying_line_existence_index_;
  LayoutUnit baseline_offset_from_margin_box_top_;
  LayoutUnit line_height_;
  LayoutUnit inline_top_margin_;

  // A font used for text width and line height calculations.
  const scoped_refptr<dom::FontList> used_font_;

  // A reference to the next inline container box in a linked list of inline
  // container boxes produced from splits of the initial text box. This enables
  // HTMLElement to retain access to all of its layout boxes after they are
  // split.
  scoped_refptr<InlineContainerBox> split_sibling_;

  DISALLOW_COPY_AND_ASSIGN(InlineContainerBox);
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_INLINE_CONTAINER_BOX_H_

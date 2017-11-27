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

#ifndef COBALT_LAYOUT_BLOCK_CONTAINER_BOX_H_
#define COBALT_LAYOUT_BLOCK_CONTAINER_BOX_H_

#include "base/optional.h"
#include "cobalt/layout/base_direction.h"
#include "cobalt/layout/container_box.h"
#include "cobalt/layout/size_layout_unit.h"

namespace cobalt {
namespace layout {

class FormattingContext;

// A base class for block container boxes which implements most of the
// functionality except establishing a formatting context.
//
// Derived classes establish either:
//   - a block formatting context (and thus contain only block-level boxes);
//   - an inline formatting context (and thus contain only inline-level boxes).
//   https://www.w3.org/TR/CSS21/visuren.html#block-boxes
//
// Note that "block container box" and "block-level box" are different concepts.
// A block container box itself may either be a block-level box or
// an inline-level box.
//   https://www.w3.org/TR/CSS21/visuren.html#inline-boxes
class BlockContainerBox : public ContainerBox {
 public:
  BlockContainerBox(const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
                        css_computed_style_declaration,
                    BaseDirection base_direction,
                    UsedStyleProvider* used_style_provider,
                    LayoutStatTracker* layout_stat_tracker);
  ~BlockContainerBox() override;

  // From |Box|.
  void UpdateContentSizeAndMargins(const LayoutParams& layout_params) override;
  WrapResult TryWrapAt(WrapAtPolicy wrap_at_policy,
                       WrapOpportunityPolicy wrap_opportunity_policy,
                       bool is_line_existence_justified,
                       LayoutUnit available_width,
                       bool should_collapse_trailing_white_space) override;

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
  bool AffectsBaselineInBlockFormattingContext() const override;
  LayoutUnit GetBaselineOffsetFromTopMarginEdge() const override;

  // From |ContainerBox|.
  scoped_refptr<ContainerBox> TrySplitAtEnd() override;

  BaseDirection GetBaseDirection() const;

 protected:
  // From |Box|.
  bool IsTransformable() const override;

#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpProperties(std::ostream* stream) const override;
#endif  // COBALT_BOX_DUMP_ENABLED

  // Rest of the protected methods.

  // Lays out children recursively.
  virtual scoped_ptr<FormattingContext> UpdateRectOfInFlowChildBoxes(
      const LayoutParams& child_layout_params) = 0;

 private:
  void UpdateContentWidthAndMargins(
      LayoutUnit containing_block_width, bool shrink_to_fit_width_forced,
      bool width_depends_on_containing_block,
      const base::optional<LayoutUnit>& maybe_left,
      const base::optional<LayoutUnit>& maybe_right,
      const base::optional<LayoutUnit>& maybe_margin_left,
      const base::optional<LayoutUnit>& maybe_margin_right,
      const base::optional<LayoutUnit>& maybe_width,
      const base::optional<LayoutUnit>& maybe_height);
  void UpdateContentHeightAndMargins(
      const SizeLayoutUnit& containing_block_size,
      const base::optional<LayoutUnit>& maybe_top,
      const base::optional<LayoutUnit>& maybe_bottom,
      const base::optional<LayoutUnit>& maybe_margin_top,
      const base::optional<LayoutUnit>& maybe_margin_bottom,
      const base::optional<LayoutUnit>& maybe_height);

  void UpdateWidthAssumingAbsolutelyPositionedBox(
      LayoutUnit containing_block_width,
      const base::optional<LayoutUnit>& maybe_left,
      const base::optional<LayoutUnit>& maybe_right,
      const base::optional<LayoutUnit>& maybe_width,
      const base::optional<LayoutUnit>& maybe_margin_left,
      const base::optional<LayoutUnit>& maybe_margin_right,
      const base::optional<LayoutUnit>& maybe_height);
  void UpdateHeightAssumingAbsolutelyPositionedBox(
      LayoutUnit containing_block_height,
      const base::optional<LayoutUnit>& maybe_top,
      const base::optional<LayoutUnit>& maybe_bottom,
      const base::optional<LayoutUnit>& maybe_height,
      const base::optional<LayoutUnit>& maybe_margin_top,
      const base::optional<LayoutUnit>& maybe_margin_bottom,
      const FormattingContext& formatting_context);

  void UpdateWidthAssumingBlockLevelInFlowBox(
      LayoutUnit containing_block_width,
      const base::optional<LayoutUnit>& maybe_width,
      const base::optional<LayoutUnit>& maybe_margin_left,
      const base::optional<LayoutUnit>& maybe_margin_right);
  void UpdateWidthAssumingInlineLevelInFlowBox(
      LayoutUnit containing_block_width,
      const base::optional<LayoutUnit>& maybe_width,
      const base::optional<LayoutUnit>& maybe_margin_left,
      const base::optional<LayoutUnit>& maybe_margin_right,
      const base::optional<LayoutUnit>& maybe_height);

  void UpdateHeightAssumingInFlowBox(
      const base::optional<LayoutUnit>& maybe_height,
      const base::optional<LayoutUnit>& maybe_margin_top,
      const base::optional<LayoutUnit>& maybe_margin_bottom,
      const FormattingContext& formatting_context);

  LayoutUnit GetShrinkToFitWidth(
      LayoutUnit containing_block_width,
      const base::optional<LayoutUnit>& maybe_height);

  // A vertical offset of the baseline of the last child box that has one,
  // relatively to the origin of the block container box. Disengaged, if none
  // of the child boxes have a baseline.
  base::optional<LayoutUnit> maybe_baseline_offset_from_top_margin_edge_;

  // The primary direction in which inline content is ordered on a line and the
  // sides on which the "start" and "end" of a line are.
  // https://www.w3.org/TR/css-writing-modes-3/#inline-base-direction
  BaseDirection base_direction_;

  DISALLOW_COPY_AND_ASSIGN(BlockContainerBox);
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_BLOCK_CONTAINER_BOX_H_

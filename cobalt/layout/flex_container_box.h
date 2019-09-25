// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_FLEX_CONTAINER_BOX_H_
#define COBALT_LAYOUT_FLEX_CONTAINER_BOX_H_

#include <memory>

#include "base/optional.h"
#include "cobalt/cssom/css_computed_style_declaration.h"
#include "cobalt/layout/base_direction.h"
#include "cobalt/layout/block_container_box.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/flex_formatting_context.h"
#include "cobalt/layout/layout_unit.h"
#include "cobalt/layout/paragraph.h"

namespace cobalt {
namespace layout {

class AnonymousBlockBox;

class FlexContainerBox : public BlockContainerBox {
 public:
  FlexContainerBox(const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
                       css_computed_style_declaration,
                   BaseDirection base_direction,
                   UsedStyleProvider* used_style_provider,
                   LayoutStatTracker* layout_stat_tracker);
  ~FlexContainerBox() override;

  // From |Box|.
  void UpdateContentSizeAndMargins(const LayoutParams& layout_params) override;
  WrapResult TryWrapAt(WrapAtPolicy wrap_at_policy,
                       WrapOpportunityPolicy wrap_opportunity_policy,
                       bool is_line_existence_justified,
                       LayoutUnit available_width,
                       bool should_collapse_trailing_white_space) override;

  bool TrySplitAtSecondBidiLevelRun() override;

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

  bool TryAddChild(const scoped_refptr<Box>& child_box) override;

  // A convenience method to add children. It is guaranteed that a block
  // container box is able to become a parent of both block-level and
  // inline-level boxes.
  void AddChild(const scoped_refptr<Box>& child_box);

  BaseDirection base_direction() const { return base_direction_; }

 protected:
  // From |Box|.
  bool IsTransformable() const override;

  // From |BlockContainerBox|.
  std::unique_ptr<FormattingContext> UpdateRectOfInFlowChildBoxes(
      const LayoutParams& child_layout_params) override;

  // The primary direction in which inline content is ordered on a line and the
  // sides on which the "start" and "end" of a line are.
  // https://www.w3.org/TR/css-writing-modes-3/#inline-base-direction
  const BaseDirection base_direction_;

 private:
  base::Optional<LayoutUnit> main_space_;
  base::Optional<LayoutUnit> cross_space_;

  base::Optional<LayoutUnit> min_main_space_;
  base::Optional<LayoutUnit> max_main_space_;

  base::Optional<LayoutUnit> min_cross_space_;
  base::Optional<LayoutUnit> max_cross_space_;

  LayoutUnit baseline_;

  // Return true if the main direction of the flex container is horizontal, e.g.
  // 'flex-direction: row' or 'flex-direction: row-reverse'. Note: This assumes
  // that the inline direction is horizontal. Since Cobalt does not support
  // vertical writing modes, that is currently always true.
  bool MainDirectionIsHorizontal() const;

  // Return true if the direction of the main axis is reversed, e.g.
  // 'flex-direction: column-reverse' or 'flex-direction: row-reverse'.
  bool DirectionIsReversed() const;

  bool ContainerIsMultiLine() const;

  // Ensure that the boxes are in order modified document order.
  //   https://www.w3.org/TR/css-flexbox-1/#order-modified-document-order
  void EnsureBoxesAreInOrderModifiedDocumentOrder();

  // First step of the line length determination.
  void DetermineAvailableSpace(const LayoutParams& layout_params,
                               bool main_direction_is_horizontal,
                               bool width_depends_on_containing_block,
                               const base::Optional<LayoutUnit>& maybe_width,
                               bool height_depends_on_containing_block,
                               const base::Optional<LayoutUnit>& maybe_height);

  AnonymousBlockBox* GetLastChildAsAnonymousBlockBox();
  AnonymousBlockBox* GetOrAddAnonymousBlockBox();

  DISALLOW_COPY_AND_ASSIGN(FlexContainerBox);
};

class BlockLevelFlexContainerBox : public FlexContainerBox {
 public:
  BlockLevelFlexContainerBox(
      const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
          css_computed_style_declaration,
      BaseDirection base_direction, UsedStyleProvider* used_style_provider,
      LayoutStatTracker* layout_stat_tracker);
  ~BlockLevelFlexContainerBox() override;

  // From |Box|.
  Level GetLevel() const override;
  base::Optional<int> GetBidiLevel() const override;

 protected:
// From |Box|.
#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpClassName(std::ostream* stream) const override;
#endif  // COBALT_BOX_DUMP_ENABLED
};

class InlineLevelFlexContainerBox : public FlexContainerBox {
 public:
  InlineLevelFlexContainerBox(
      const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
          css_computed_style_declaration,
      BaseDirection base_direction, const scoped_refptr<Paragraph>& paragraph,
      int32 text_position, UsedStyleProvider* used_style_provider,
      LayoutStatTracker* layout_stat_tracker);
  ~InlineLevelFlexContainerBox() override;

  // From |Box|.
  Level GetLevel() const override;
  base::Optional<int> GetBidiLevel() const override;

 protected:
// From |Box|.
#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpClassName(std::ostream* stream) const override;
#endif  // COBALT_BOX_DUMP_ENABLED
 private:
  const scoped_refptr<Paragraph> paragraph_;
  int32 text_position_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_FLEX_CONTAINER_BOX_H_

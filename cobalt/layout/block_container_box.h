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

#ifndef LAYOUT_BLOCK_CONTAINER_BOX_H_
#define LAYOUT_BLOCK_CONTAINER_BOX_H_

#include "base/optional.h"
#include "cobalt/layout/container_box.h"

namespace cobalt {
namespace layout {

class FormattingContext;

// A base class for block container boxes which implements most of the
// functionality except establishing a formatting context.
//
// Derived classes establish either:
//   - a block formatting context (and thus contain only block-level boxes);
//   - an inline formatting context (and thus contain only inline-level boxes).
//   http://www.w3.org/TR/CSS21/visuren.html#block-boxes
//
// Note that "block container box" and "block-level box" are different concepts.
// A block container box itself may either be a block-level box or
// an inline-level box.
//   http://www.w3.org/TR/CSS21/visuren.html#inline-boxes
class BlockContainerBox : public ContainerBox {
 public:
  BlockContainerBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider);
  ~BlockContainerBox() OVERRIDE;

  // From |Box|.
  void UpdateContentSizeAndMargins(const LayoutParams& layout_params) OVERRIDE;
  scoped_ptr<Box> TrySplitAt(float available_width,
                             bool allow_overflow) OVERRIDE;

  scoped_ptr<Box> TrySplitAtSecondBidiLevelRun() OVERRIDE;
  base::optional<int> GetBidiLevel() const OVERRIDE;

  void SetShouldCollapseLeadingWhiteSpace(
      bool should_collapse_leading_white_space) OVERRIDE;
  void SetShouldCollapseTrailingWhiteSpace(
      bool should_collapse_trailing_white_space) OVERRIDE;
  bool HasLeadingWhiteSpace() const OVERRIDE;
  bool HasTrailingWhiteSpace() const OVERRIDE;
  bool IsCollapsed() const OVERRIDE;

  bool JustifiesLineExistence() const OVERRIDE;
  bool AffectsBaselineInBlockFormattingContext() const OVERRIDE;
  float GetBaselineOffsetFromTopMarginEdge() const OVERRIDE;

  // From |ContainerBox|.
  scoped_ptr<ContainerBox> TrySplitAtEnd() OVERRIDE;

 protected:
  // From |Box|.
  bool IsTransformable() const OVERRIDE;

#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpProperties(std::ostream* stream) const OVERRIDE;
#endif  // COBALT_BOX_DUMP_ENABLED

  // Rest of the protected methods.

  // Lays out children recursively.
  virtual scoped_ptr<FormattingContext> UpdateRectOfInFlowChildBoxes(
      const LayoutParams& child_layout_params) = 0;

 private:
  void UpdateWidthAssumingAbsolutelyPositionedBox(
      float containing_block_width, const base::optional<float>& maybe_left,
      const base::optional<float>& maybe_right,
      const base::optional<float>& maybe_width,
      const base::optional<float>& maybe_margin_left,
      const base::optional<float>& maybe_margin_right,
      const base::optional<float>& maybe_height);
  void UpdateHeightAssumingAbsolutelyPositionedBox(
      float containing_block_height, const base::optional<float>& maybe_top,
      const base::optional<float>& maybe_bottom,
      const base::optional<float>& maybe_height,
      const base::optional<float>& maybe_margin_top,
      const base::optional<float>& maybe_margin_bottom,
      const FormattingContext& formatting_context);

  void UpdateWidthAssumingBlockLevelInFlowBox(
      float containing_block_width, const base::optional<float>& maybe_width,
      const base::optional<float>& maybe_margin_left,
      const base::optional<float>& maybe_margin_right);
  void UpdateWidthAssumingInlineLevelInFlowBox(
      float containing_block_width, const base::optional<float>& maybe_width,
      const base::optional<float>& maybe_margin_left,
      const base::optional<float>& maybe_margin_right,
      const base::optional<float>& maybe_height);

  void UpdateHeightAssumingInFlowBox(
      const base::optional<float>& maybe_height,
      const base::optional<float>& maybe_margin_top,
      const base::optional<float>& maybe_margin_bottom,
      const FormattingContext& formatting_context);

  float GetShrinkToFitWidth(float containing_block_width,
                            const base::optional<float>& maybe_height);

  // A vertical offset of the baseline of the last child box that has one,
  // relatively to the origin of the block container box. Disengaged, if none
  // of the child boxes have a baseline.
  base::optional<float> maybe_baseline_offset_from_top_margin_edge_;

  DISALLOW_COPY_AND_ASSIGN(BlockContainerBox);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_BLOCK_CONTAINER_BOX_H_

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
  void UpdateUsedSize(const LayoutParams& layout_params) OVERRIDE;
  scoped_ptr<Box> TrySplitAt(float available_width) OVERRIDE;

  bool IsCollapsed() const OVERRIDE;
  bool HasLeadingWhiteSpace() const OVERRIDE;
  bool HasTrailingWhiteSpace() const OVERRIDE;
  void CollapseLeadingWhiteSpace() OVERRIDE;
  void CollapseTrailingWhiteSpace() OVERRIDE;

  bool JustifiesLineExistence() const OVERRIDE;
  bool AffectsBaselineInBlockFormattingContext() const OVERRIDE;
  float GetHeightAboveBaseline() const OVERRIDE;

  // From |ContainerBox|.
  scoped_ptr<ContainerBox> TrySplitAtEnd() OVERRIDE;

 protected:
  // From |Box|.
  bool IsTransformable() const OVERRIDE;

  void DumpProperties(std::ostream* stream) const OVERRIDE;

  // Rest of the protected methods.

  // For block-level boxes, precisely calculates the used width and determines
  // whether the value depends on a containing block (it does not if the
  // computed value is an absolute length).
  //
  // For inline-block boxes, approximates the used width by treating "auto"
  // in the same way as for block boxes, and determines whether the exact value
  // depends on children (and thus the second pass of the layout is needed).
  //
  // Called before the first pass of the layout, when children sizes are
  // not known yet.
  virtual float GetUsedWidthBasedOnContainingBlock(
      float containing_block_width, bool* width_depends_on_containing_block,
      bool* width_depends_on_child_boxes) const = 0;
  // Calculates the used height if it does not depend on children.
  // Otherwise returns zero and sets |height_depends_on_child_boxes|.
  //
  // Called before the first pass of the layout, when children sizes are
  // not known yet.
  virtual float GetUsedHeightBasedOnContainingBlock(
      float containing_block_height,
      bool* height_depends_on_child_boxes) const = 0;

  // Lays out children recursively.
  virtual scoped_ptr<FormattingContext> UpdateUsedRectOfChildren(
      const LayoutParams& child_layout_params) = 0;

 private:
  LayoutParams GetChildLayoutOptions(const LayoutParams& layout_params,
                                     bool* width_depends_on_child_boxes,
                                     bool* height_depends_on_child_boxes) const;

  // A vertical offset of the baseline of the last child box that has one,
  // relatively to the origin of the block container box. Disengaged, if none
  // of the child boxes have a baseline.
  base::optional<float> maybe_height_above_baseline_;

  DISALLOW_COPY_AND_ASSIGN(BlockContainerBox);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_BLOCK_CONTAINER_BOX_H_

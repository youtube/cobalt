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

class BlockFormattingContext;
class InlineFormattingContext;

// A block container box establishes either:
//   - a block formatting context (and thus only contains block-level boxes);
//   - an inline formatting context (and thus only contains inline-level boxes).
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
      const cssom::TransitionSet* transitions);
  ~BlockContainerBox() OVERRIDE;

  // From |Box|.
  void Layout(const LayoutOptions& layout_options) OVERRIDE;
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
  bool TryAddChild(scoped_ptr<Box>* child_box) OVERRIDE;
  scoped_ptr<ContainerBox> TrySplitAtEnd() OVERRIDE;

  // Rest of the public methods.

  // A convenience method to add children. It is guaranteed that a block
  // container box is able to become a parent of both block-level and
  // inline-level boxes.
  void AddChild(scoped_ptr<Box> child_box);

 protected:
  // From |Box|.
  void AddContentToRenderTree(
      render_tree::CompositionNode::Builder* composition_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const OVERRIDE;
  bool IsTransformable() const OVERRIDE;

  void DumpClassName(std::ostream* stream) const OVERRIDE;
  void DumpProperties(std::ostream* stream) const OVERRIDE;
  void DumpChildrenWithIndent(std::ostream* stream, int indent) const OVERRIDE;

  // Rest of the protected methods.
  void SwapChildBoxes(ChildBoxes* child_boxes) {
    child_boxes_.swap(*child_boxes);
  }

 private:
  // Defines the formatting context established by this box.
  // Do not confuse with the formatting context in which the box should
  // participate.
  //   http://www.w3.org/TR/CSS21/visuren.html#block-boxes
  enum FormattingContext { kBlockFormattingContext, kInlineFormattingContext };

  void AddChildAssumingBlockFormattingContext(scoped_ptr<Box> child_box);
  void AddChildAssumingInlineFormattingContext(scoped_ptr<Box> child_box);

  // Infers the established formatting context from the level of the child box.
  static FormattingContext ConvertLevelToFormattingContext(Level level);

  AnonymousBlockBox* GetOrAddAnonymousBlockBox();

  template <typename FormattingContextT>
  void LayoutAssuming(const LayoutOptions& layout_options);
  template <typename FormattingContextT>
  scoped_ptr<FormattingContextT> LayoutChildrenAssuming(
      const LayoutOptions& child_layout_options);

  LayoutOptions GetChildLayoutOptions(
      const LayoutOptions& layout_options, bool* width_depends_on_child_boxes,
      bool* height_depends_on_child_boxes) const;
  float GetChildIndependentUsedWidth(float containing_block_width,
                                     bool* width_depends_on_containing_block,
                                     bool* width_depends_on_child_boxes) const;
  float GetChildIndependentUsedHeight(
      float containing_block_height, bool* height_depends_on_child_boxes) const;
  float GetChildDependentUsedWidth(
      const BlockFormattingContext& block_formatting_context) const;
  float GetChildDependentUsedWidth(
      const InlineFormattingContext& inline_formatting_context) const;
  float GetChildDependentUsedHeight(
      const BlockFormattingContext& block_formatting_context) const;
  float GetChildDependentUsedHeight(
      const InlineFormattingContext& inline_formatting_context) const;

  // A formatting context is inferred from the level of child boxes.
  // A block container box assumes an inline formatting context by default,
  // and keeps it until inline-level child boxes are added. When the first
  // block-level box is added, a block formatting context is inferred.
  FormattingContext formatting_context_;
  // Child boxes, including anonymous block boxes, if any.
  ChildBoxes child_boxes_;
  // A vertical offset of the baseline of the last child box that has one,
  // relatively to the origin of the block container box. Disengaged, if none
  // of the child boxes have a baseline.
  base::optional<float> height_above_baseline_;

  DISALLOW_COPY_AND_ASSIGN(BlockContainerBox);
};

// Often abbreviated as simply "blocks", block-level block container boxes
// participate in the block formatting context and are the most common form
// of block container boxes.
//   http://www.w3.org/TR/CSS21/visuren.html#block-boxes
class BlockLevelBlockContainerBox : public BlockContainerBox {
 public:
  BlockLevelBlockContainerBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions);
  ~BlockLevelBlockContainerBox() OVERRIDE;

  // From |Box|.
  Level GetLevel() const OVERRIDE;
};

// Non-replaced inline-block elements generate block container boxes that
// participate in the inline formatting context as a single opaque box. They
// belong to a wider group called atomic inline-level boxes.
//   http://www.w3.org/TR/CSS21/visuren.html#inline-boxes
class InlineLevelBlockContainerBox : public BlockContainerBox {
 public:
  InlineLevelBlockContainerBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions);
  ~InlineLevelBlockContainerBox() OVERRIDE;

  // From |Box|.
  Level GetLevel() const OVERRIDE;
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_BLOCK_CONTAINER_BOX_H_

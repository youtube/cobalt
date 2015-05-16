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

#ifndef LAYOUT_INLINE_CONTAINER_BOX_H_
#define LAYOUT_INLINE_CONTAINER_BOX_H_

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
//   http://www.w3.org/TR/CSS21/visuren.html#inline-boxes
class InlineContainerBox : public ContainerBox {
 public:
  InlineContainerBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider);
  ~InlineContainerBox() OVERRIDE;

  // From |Box|.
  Level GetLevel() const OVERRIDE;

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
  bool TryAddChild(scoped_ptr<Box>* child_box) OVERRIDE;
  scoped_ptr<ContainerBox> TrySplitAtEnd() OVERRIDE;

 protected:
  // From |Box|.
  void AddContentToRenderTree(
      render_tree::CompositionNode::Builder* composition_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const OVERRIDE;
  bool IsTransformable() const OVERRIDE;

  void DumpClassName(std::ostream* stream) const OVERRIDE;
  void DumpChildrenWithIndent(std::ostream* stream, int indent) const OVERRIDE;

 private:
  ChildBoxes::const_iterator FindFirstNonCollapsedChildBox() const;
  ChildBoxes::const_iterator FindLastNonCollapsedChildBox() const;

  ChildBoxes child_boxes_;

  bool justifies_line_existence_;
  float height_above_baseline_;

  DISALLOW_COPY_AND_ASSIGN(InlineContainerBox);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_INLINE_CONTAINER_BOX_H_

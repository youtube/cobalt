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

#ifndef LAYOUT_BLOCK_FORMATTING_BLOCK_CONTAINER_BOX_H_
#define LAYOUT_BLOCK_FORMATTING_BLOCK_CONTAINER_BOX_H_

#include "cobalt/layout/block_container_box.h"
#include "cobalt/layout/paragraph.h"

namespace cobalt {
namespace layout {

// A block container box that establishes a block formatting context.
// Implementation-specific, not defined in CSS 2.1.
class BlockFormattingBlockContainerBox : public BlockContainerBox {
 public:
  BlockFormattingBlockContainerBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider);

  // From |ContainerBox|.
  bool TryAddChild(scoped_ptr<Box>* child_box) OVERRIDE;

  // Rest of the public methods.

  // A convenience method to add children. It is guaranteed that a block
  // container box is able to become a parent of both block-level and
  // inline-level boxes.
  void AddChild(scoped_ptr<Box> child_box);

 protected:
  // From |BlockContainerBox|.
  scoped_ptr<FormattingContext> UpdateRectOfInFlowChildBoxes(
      const LayoutParams& child_layout_params) OVERRIDE;

 private:
  AnonymousBlockBox* GetLastChildAsAnonymousBlockBox();
  AnonymousBlockBox* GetOrAddAnonymousBlockBox();
};

// Often abbreviated as simply "blocks", block-level block container boxes
// participate in the block formatting context and are the most common form
// of block container boxes.
//   http://www.w3.org/TR/CSS21/visuren.html#block-boxes
//
// Although this class always establishes a block formatting context, it can
// nevertheless accommodate inline-level children through the creation
// of anonymous block boxes. When all children are inline-level this becomes
// slightly suboptimal from a memory standpoint but it simplifies
// the implementation and is conformance-neutral.
//   http://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
class BlockLevelBlockContainerBox : public BlockFormattingBlockContainerBox {
 public:
  BlockLevelBlockContainerBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider);
  ~BlockLevelBlockContainerBox() OVERRIDE;

  // From |Box|.
  Level GetLevel() const OVERRIDE;

 protected:
  // From |Box|.
#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpClassName(std::ostream* stream) const OVERRIDE;
#endif  // COBALT_BOX_DUMP_ENABLED
};

// Non-replaced inline-block elements generate block container boxes that
// participate in the inline formatting context as a single opaque box. They
// belong to a wider group called atomic inline-level boxes.
//   http://www.w3.org/TR/CSS21/visuren.html#inline-boxes
//
// Although this class always establishes a block formatting context, it can
// nevertheless accommodate inline-level children through the creation
// of anonymous block boxes. When all children are inline-level this becomes
// slightly suboptimal from a memory standpoint but it simplifies
// the implementation and is conformance-neutral.
//   http://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
class InlineLevelBlockContainerBox : public BlockFormattingBlockContainerBox {
 public:
  InlineLevelBlockContainerBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider,
      const scoped_refptr<Paragraph>& paragraph, int32 text_position);
  ~InlineLevelBlockContainerBox() OVERRIDE;

  // From |Box|.
  Level GetLevel() const OVERRIDE;
  base::optional<int> GetBidiLevel() const OVERRIDE;

 protected:
  // From |Box|.
#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpClassName(std::ostream* stream) const OVERRIDE;
  void DumpProperties(std::ostream* stream) const OVERRIDE;
#endif  // COBALT_BOX_DUMP_ENABLED

 private:
  const scoped_refptr<Paragraph> paragraph_;
  int32 text_position_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_BLOCK_FORMATTING_BLOCK_CONTAINER_BOX_H_

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

#ifndef COBALT_LAYOUT_BLOCK_FORMATTING_BLOCK_CONTAINER_BOX_H_
#define COBALT_LAYOUT_BLOCK_FORMATTING_BLOCK_CONTAINER_BOX_H_

#include "cobalt/layout/base_direction.h"
#include "cobalt/layout/block_container_box.h"
#include "cobalt/layout/paragraph.h"

namespace cobalt {
namespace layout {

// A block container box that establishes a block formatting context.
// Implementation-specific, not defined in CSS 2.1.
class BlockFormattingBlockContainerBox : public BlockContainerBox {
 public:
  BlockFormattingBlockContainerBox(
      const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
          css_computed_style_declaration,
      BaseDirection base_direction, UsedStyleProvider* used_style_provider,
      LayoutStatTracker* layout_stat_tracker);

  // From |ContainerBox|.
  bool TryAddChild(const scoped_refptr<Box>& child_box) override;

  // Rest of the public methods.

  // A convenience method to add children. It is guaranteed that a block
  // container box is able to become a parent of both block-level and
  // inline-level boxes.
  void AddChild(const scoped_refptr<Box>& child_box);

 protected:
  // From |BlockContainerBox|.
  scoped_ptr<FormattingContext> UpdateRectOfInFlowChildBoxes(
      const LayoutParams& child_layout_params) override;

 private:
  AnonymousBlockBox* GetLastChildAsAnonymousBlockBox();
  AnonymousBlockBox* GetOrAddAnonymousBlockBox();
};

// Often abbreviated as simply "blocks", block-level block container boxes
// participate in the block formatting context and are the most common form
// of block container boxes.
//   https://www.w3.org/TR/CSS21/visuren.html#block-boxes
//
// Although this class always establishes a block formatting context, it can
// nevertheless accommodate inline-level children through the creation
// of anonymous block boxes. When all children are inline-level this becomes
// slightly suboptimal from a memory standpoint but it simplifies
// the implementation and is conformance-neutral.
//   https://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
class BlockLevelBlockContainerBox : public BlockFormattingBlockContainerBox {
 public:
  BlockLevelBlockContainerBox(
      const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
          css_computed_style_declaration,
      BaseDirection base_direction, UsedStyleProvider* used_style_provider,
      LayoutStatTracker* layout_stat_tracker);
  ~BlockLevelBlockContainerBox() override;

  // From |Box|.
  Level GetLevel() const override;

 protected:
  // From |Box|.
#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpClassName(std::ostream* stream) const override;
#endif  // COBALT_BOX_DUMP_ENABLED
};

// Non-replaced inline-block elements generate block container boxes that
// participate in the inline formatting context as a single opaque box. They
// belong to a wider group called atomic inline-level boxes.
//   https://www.w3.org/TR/CSS21/visuren.html#inline-boxes
//
// Although this class always establishes a block formatting context, it can
// nevertheless accommodate inline-level children through the creation
// of anonymous block boxes. When all children are inline-level this becomes
// slightly suboptimal from a memory standpoint but it simplifies
// the implementation and is conformance-neutral.
//   https://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
class InlineLevelBlockContainerBox : public BlockFormattingBlockContainerBox {
 public:
  InlineLevelBlockContainerBox(
      const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
          css_computed_style_declaration,
      BaseDirection base_direction, const scoped_refptr<Paragraph>& paragraph,
      int32 text_position, UsedStyleProvider* used_style_provider,
      LayoutStatTracker* layout_stat_tracker);
  ~InlineLevelBlockContainerBox() override;

  // From |Box|.
  Level GetLevel() const override;

  WrapResult TryWrapAt(WrapAtPolicy wrap_at_policy,
                       WrapOpportunityPolicy wrap_opportunity_policy,
                       bool is_line_existence_justified,
                       LayoutUnit available_width,
                       bool should_collapse_trailing_white_space) override;

  base::optional<int> GetBidiLevel() const override;

  bool DoesFulfillEllipsisPlacementRequirement() const override;
  void DoPreEllipsisPlacementProcessing() override;
  void DoPostEllipsisPlacementProcessing() override;
  bool IsHiddenByEllipsis() const override;

 protected:
  // From |Box|.
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

  const scoped_refptr<Paragraph> paragraph_;
  int32 text_position_;

  // This flag indicates that the box is fully hidden by the ellipsis and it,
  // along with its contents will not be visible.
  // "Implementations must hide characters and atomic inline-level elements at
  // the applicable edge(s) of the line as necessary to fit the ellipsis."
  //   https://www.w3.org/TR/css3-ui/#propdef-text-overflow
  bool is_hidden_by_ellipsis_;
  // Tracking of the previous value of |is_hidden_by_ellipsis_|, which allows
  // for determination of whether or not the value changed during ellipsis
  // placement. When this occurs, the cached render tree nodes of this box and
  // its ancestors are invalidated.
  bool was_hidden_by_ellipsis_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_BLOCK_FORMATTING_BLOCK_CONTAINER_BOX_H_

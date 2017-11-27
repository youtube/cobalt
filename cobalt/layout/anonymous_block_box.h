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

#ifndef COBALT_LAYOUT_ANONYMOUS_BLOCK_BOX_H_
#define COBALT_LAYOUT_ANONYMOUS_BLOCK_BOX_H_

#include <vector>

#include "cobalt/dom/font_list.h"
#include "cobalt/layout/base_direction.h"
#include "cobalt/layout/block_container_box.h"
#include "cobalt/layout/box.h"

namespace cobalt {
namespace layout {

// A block-level block container box that establish an inline formatting
// context. Anonymous block boxes are created to enclose inline-level
// children in a block formatting context.
//   https://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
class AnonymousBlockBox : public BlockContainerBox {
 public:
  AnonymousBlockBox(const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
                        css_computed_style_declaration,
                    BaseDirection base_direction,
                    UsedStyleProvider* used_style_provider,
                    LayoutStatTracker* layout_stat_tracker);

  // From |Box|.
  Level GetLevel() const override;
  AnonymousBlockBox* AsAnonymousBlockBox() override;
  const AnonymousBlockBox* AsAnonymousBlockBox() const override;

  void SplitBidiLevelRuns() override;

  bool HasTrailingLineBreak() const override;

  void RenderAndAnimateContent(
      render_tree::CompositionNode::Builder* border_node_builder,
      ContainerBox* stacking_context) const override;

  // From |ContainerBox|.

  // This method should never be called, instead all children have to be added
  // through |AddInlineLevelChild|.
  bool TryAddChild(const scoped_refptr<Box>& child_box) override;

  // Rest of the public methods.

  // An anonymous block box may only contain inline-level children.
  void AddInlineLevelChild(const scoped_refptr<Box>& child_box);

 protected:
  // From |Box|.
#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpClassName(std::ostream* stream) const override;
#endif  // COBALT_BOX_DUMP_ENABLED

  // From |BlockContainerBox|.
  scoped_ptr<FormattingContext> UpdateRectOfInFlowChildBoxes(
      const LayoutParams& child_layout_params) override;

 private:
  bool AreEllipsesEnabled() const;

  typedef std::vector<math::Vector2dF> EllipsesCoordinates;

  // A font used for text width and line height calculations.
  const scoped_refptr<dom::FontList> used_font_;

  // The XY coordinates of all ellipses placed while laying out the anonymous
  // block box within an inline formatting context.
  EllipsesCoordinates ellipses_coordinates_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_ANONYMOUS_BLOCK_BOX_H_

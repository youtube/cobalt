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

#ifndef LAYOUT_ANONYMOUS_BLOCK_BOX_H_
#define LAYOUT_ANONYMOUS_BLOCK_BOX_H_

#include "cobalt/layout/block_container_box.h"
#include "cobalt/layout/box.h"
#include "cobalt/render_tree/font.h"

namespace cobalt {
namespace layout {

// A block-level block container box that establish an inline formatting
// context. Anonymous block boxes are created to enclose inline-level
// children in a block formatting context.
//   http://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
class AnonymousBlockBox : public BlockContainerBox {
 public:
  AnonymousBlockBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider);

  // From |Box|.
  Level GetLevel() const OVERRIDE;
  AnonymousBlockBox* AsAnonymousBlockBox() OVERRIDE;

  void SplitBidiLevelRuns() OVERRIDE;

  // From |ContainerBox|.

  // This method should never be called, instead all children have to be added
  // through |AddInlineLevelChild|.
  bool TryAddChild(scoped_ptr<Box>* child_box) OVERRIDE;

  // Rest of the public methods.

  // An anonymous block box may only contain inline-level children.
  void AddInlineLevelChild(scoped_ptr<Box> child_box);

 protected:
  // From |Box|.
#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpClassName(std::ostream* stream) const OVERRIDE;
#endif  // COBALT_BOX_DUMP_ENABLED

  // From |BlockContainerBox|.
  scoped_ptr<FormattingContext> UpdateRectOfInFlowChildBoxes(
      const LayoutParams& child_layout_params) OVERRIDE;

 private:
  // A font used for text width and line height calculations.
  const scoped_refptr<render_tree::Font> used_font_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_ANONYMOUS_BLOCK_BOX_H_

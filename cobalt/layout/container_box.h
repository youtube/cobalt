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

#ifndef LAYOUT_CONTAINER_BOX_H_
#define LAYOUT_CONTAINER_BOX_H_

#include "base/memory/scoped_vector.h"
#include "cobalt/layout/box.h"

namespace cobalt {
namespace layout {

// Defines a base interface for block and inline container boxes that allows
// the box generator to be agnostic to a box type. Implementation-specific,
// not defined in CSS 2.1.
class ContainerBox : public Box {
 public:
  ContainerBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider);
  ~ContainerBox() OVERRIDE;

  // Attempts to add a child box and takes the ownership if succeeded. Returns
  // true if the child's box level is compatible with the container box. Block
  // container boxes are able to become parents of both block- and inline-level
  // boxes, while inline container boxes are only able to become parents
  // of inline-level boxes.
  virtual bool TryAddChild(scoped_ptr<Box>* child_box) = 0;
  // Attempts to split the box right before the end. Returns the part after
  // the split if the split succeeded. Only inline boxes are splittable and it's
  // always possible to split them. The returned part is identical to
  // the original container, except that the former is empty.
  //
  // A box generator uses this method to break inline boxes around block-level
  // boxes.
  virtual scoped_ptr<ContainerBox> TrySplitAtEnd() = 0;

  // Adds a positioned child (a Box for which Box::IsPositioned() returns
  // true) to the set of positioned children for this container box.  Note that
  // this does not imply that this is the child's parent, it just implies that
  // this is the child's containing block.
  void AddPositionedChild(Box* child_box);

 protected:
  typedef ScopedVector<Box> ChildBoxes;

  void UpdateUsedSizeOfPositionedChildren(const LayoutParams& layout_params);
  void AddPositionedChildrenToRenderTree(
      render_tree::CompositionNode::Builder* composition_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const;

  std::vector<Box*> positioned_child_boxes_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContainerBox);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_CONTAINER_BOX_H_

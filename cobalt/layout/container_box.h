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

  // From |Box|.
  void AddContentToRenderTree(
      render_tree::CompositionNode::Builder* composition_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const OVERRIDE;

  void DumpChildrenWithIndent(std::ostream* stream, int indent) const OVERRIDE;

  bool ContainingBlockForAbsoluteElements() const;

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

  // child_box will be added to the end of the list of direct children.
  void PushBackDirectChild(scoped_ptr<Box> child_box);

  // Adds a direct child at the specified position in this container's list of
  // children.
  ChildBoxes::const_iterator InsertDirectChild(
      ChildBoxes::const_iterator position, scoped_ptr<Box> child_box);

  // Moves a range of children from a range within a source container node, to
  // this destination container node at the specified position.
  void MoveChildrenFrom(ChildBoxes::const_iterator position_in_destination,
                        ContainerBox* source_box,
                        ChildBoxes::const_iterator source_start,
                        ChildBoxes::const_iterator source_end);

  const ChildBoxes& child_boxes() const { return child_boxes_; }

 private:
  static ChildBoxes::iterator RemoveConst(
      ChildBoxes* container, ChildBoxes::const_iterator const_iter);
  ContainerBox* FindContainingBlock(Box* box);

  // Introduces a child box into the box hierarchy as a direct child of this
  // container node.  Calling this function will result in other relationships
  // being established as well (such as the child box's containing block, which
  // may not be this box, such as for absolute or fixed position children.)
  void OnChildAdded(Box* child_box);

  // A list of our direct children.  If a box is one of our child boxes, we
  // are that box's parent.  We may not be the box's containing block (such
  // as for 'absolute' or 'fixed' position elements).
  ChildBoxes child_boxes_;

  // A list of our positioned child boxes.  These must be rendered after
  // our standard in-flow children have been rendered.  For each box in our
  // list of positioned_child_boxes_, we are that child's containing block.
  std::vector<Box*> positioned_child_boxes_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContainerBox);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_CONTAINER_BOX_H_

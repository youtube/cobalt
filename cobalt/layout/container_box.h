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

#include <set>
#include <vector>

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
  void SplitBidiLevelRuns() OVERRIDE;

  void RenderAndAnimateContent(
      render_tree::CompositionNode::Builder* border_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const OVERRIDE;

#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpChildrenWithIndent(std::ostream* stream, int indent) const OVERRIDE;
#endif  // COBALT_BOX_DUMP_ENABLED

  // Returns true if the given style allows a container box to act as a
  // containing block for absolutely positioned elements.  For example it will
  // be true if this box's style is itself 'absolute'.
  bool IsContainingBlockForPositionAbsoluteElements() const;

  // Returns true if the given style allows a container box to act as a
  // containing block for fixed positioned elements.  For example it will
  // be true if this box is transformed, as indicated at the bottom of this
  // section: http://www.w3.org/TR/css3-transforms/#transform-rendering.
  bool IsContainingBlockForPositionFixedElements() const;

  // Returns true if this container box serves as a stacking context for
  // descendant elements.
  bool IsStackingContext() const;

 protected:
  typedef ScopedVector<Box> ChildBoxes;

  class ZIndexComparator {
   public:
    bool operator()(const Box* lhs, const Box* rhs) const {
      return lhs->GetZIndex() < rhs->GetZIndex();
    }
  };
  typedef std::multiset<Box*, ZIndexComparator> ZIndexSortedList;

  void UpdateRectOfPositionedChildBoxes(
      const LayoutParams& child_layout_params);

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

  void UpdateCrossReferencesWithContext(
      ContainerBox* fixed_containing_block,
      ContainerBox* absolute_containing_block,
      ContainerBox* stacking_context) OVERRIDE;

  bool ValidateUpdateSizeInputs(const LayoutParams& params) OVERRIDE;

 private:
  static ChildBoxes::iterator RemoveConst(
      ChildBoxes* container, ChildBoxes::const_iterator const_iter);
  ContainerBox* FindContainingBlock(Box* box);

  // These helper functions are called from Box::SetupAsPositionedChild().
  void AddContainingBlockChild(Box* child_box);
  void AddStackingContextChild(Box* child_box);

  // Updates used values of left/top/right/bottom given the child_box's
  // 'position' property is set to 'relative'.
  //    http://www.w3.org/TR/CSS21/visuren.html#relative-positioning
  void UpdateOffsetOfRelativelyPositionedChildBox(
      Box* child_box, const LayoutParams& child_layout_params);

  // Updates the sizes of absolutely (and fixed) position of the child box.
  // This is meant to be called by UpdateRectOfPositionedChildBoxes(), after the
  // child has gone through the in-flow layout.
  //    http://www.w3.org/TR/CSS21/visuren.html#absolute-positioning
  void UpdateRectOfAbsolutelyPositionedChildBox(
      Box* child_box, const LayoutParams& child_layout_params);

  // Add children (sorted by z-index) that belong to our stacking context to the
  // render tree.  The specific list of children to be rendered must be passed
  // in also, though it will likely only ever be either negative_z_index_child_
  // or non_negative_z_index_child_.
  void RenderAndAnimateStackingContextChildren(
      const ZIndexSortedList& z_index_child_list,
      render_tree::CompositionNode::Builder* border_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const;

  // Introduces a child box into the box hierarchy as a direct child of this
  // container node.  Calling this function will result in other relationships
  // being established as well (such as the child box's containing block, which
  // may not be this box, such as for absolute or fixed position children.)
  void OnChildAdded(Box* child_box);

  // A list of our direct children.  If a box is one of our child boxes, we
  // are that box's parent.  We may not be the box's containing block (such
  // as for 'absolute' or 'fixed' position elements).
  ChildBoxes child_boxes_;

  // A list of our positioned child boxes.  For each box in our list of
  // positioned_child_boxes_, we are that child's containing block.  This is
  // used for properly positioning and sizing positioned child elements.
  std::vector<Box*> positioned_child_boxes_;

  // A list of all children within our stacking context, sorted by z-index.
  // Every positioned box should appear in exactly one z_index list somewhere
  // in the box tree.  These lists are only used to determine render order.
  ZIndexSortedList negative_z_index_child_;
  ZIndexSortedList non_negative_z_index_child_;

  bool update_size_results_valid_;

  // Boxes and ContainerBoxes are closely related.  For example, when
  // Box::SetupAsPositionedChild() is called, it will internally call
  // ContainerBox::AddContainingBlockChild() and
  // ContainerBox::AddStackingContextChild().
  // This mutual friendship is a result of the need to maintain 2-way links
  // between boxes and containers.
  friend class Box;

  DISALLOW_COPY_AND_ASSIGN(ContainerBox);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_CONTAINER_BOX_H_

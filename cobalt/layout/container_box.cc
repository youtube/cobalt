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

#include "cobalt/layout/container_box.h"

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

ContainerBox::ContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider)
    : Box(computed_style, transitions, used_style_provider) {}

ContainerBox::~ContainerBox() {}

// Given a non-const container and a const iterator from that container, this
// will return a non-const iterator.
ContainerBox::ChildBoxes::iterator ContainerBox::RemoveConst(
    ChildBoxes* container, ChildBoxes::const_iterator const_iter) {
  // Since ChildBoxes is a vector, std::distance and std::advance are both
  // constant time.
  ContainerBox::ChildBoxes::iterator iter = container->begin();
  std::advance(iter, std::distance<ContainerBox::ChildBoxes::const_iterator>(
                         iter, const_iter));
  return iter;
}

void ContainerBox::PushBackDirectChild(scoped_ptr<Box> child_box) {
  Box* child_to_add = child_box.release();
  child_boxes_.push_back(child_to_add);
  OnChildAdded(child_to_add);
}

ContainerBox::ChildBoxes::const_iterator ContainerBox::InsertDirectChild(
    ChildBoxes::const_iterator position, scoped_ptr<Box> child_box) {
  Box* child_to_add = child_box.release();
  ChildBoxes::const_iterator inserted =
      child_boxes_.insert(RemoveConst(&child_boxes_, position), child_to_add);
  OnChildAdded(child_to_add);

  return inserted;
}

void ContainerBox::MoveChildrenFrom(
    ChildBoxes::const_iterator position_in_destination,
    ContainerBox* source_box, ChildBoxes::const_iterator source_start,
    ChildBoxes::const_iterator source_end) {
  // Add the children to our list of child boxes.
  ChildBoxes::iterator source_start_non_const =
      RemoveConst(&source_box->child_boxes_, source_start);
  ChildBoxes::iterator source_end_non_const =
      RemoveConst(&source_box->child_boxes_, source_end);
  child_boxes_.insert(RemoveConst(&child_boxes_, position_in_destination),
                      source_start_non_const, source_end_non_const);

  // Setup any link relationships given our new parent.
  for (ChildBoxes::const_iterator iter = source_start; iter != source_end;
       ++iter) {
    DCHECK_NE(this, (*iter)->parent())
        << "Move children within the same container node is not supported by "
           "this method.";
    (*iter)->parent_ = NULL;
    OnChildAdded(*iter);
  }

  // Erase the children from their previous container's list of children.
  source_box->child_boxes_.weak_erase(source_start_non_const,
                                      source_end_non_const);
}

void ContainerBox::OnChildAdded(Box* child_box) {
  DCHECK(!child_box->parent());
  child_box->parent_ = this;

  // If we're not an absolute positioned node (TODO(***REMOVED***): and also not fixed)
  // but we are positioned, then our direct parent is our containing block, so
  // set that up now.  If we are absolute or fixed position, then
  // AddPositionedChild() must be called manually on the containing block.
  if (child_box->IsPositioned()) {
    if (child_box->computed_style()->position() !=
        cssom::KeywordValue::GetAbsolute()) {
      AddPositionedChild(child_box);
    }
  }
}

// Returns true if the given style allows a container box to act as a containing
// block for absolutely positioned elements.  For example it will be true if
// this box's style is itself 'absolute'.
bool ContainerBox::ContainingBlockForAbsoluteElements() const {
  return computed_style()->position() == cssom::KeywordValue::GetAbsolute() ||
         computed_style()->position() == cssom::KeywordValue::GetRelative() ||
         computed_style()->transform() != cssom::KeywordValue::GetNone();
}

void ContainerBox::AddPositionedChild(Box* child_box) {
  // Add this child to our list of positioned children.  This is also our
  // opportunity to setup a containing_block link in the child node should
  // we choose to introduce it.
  DCHECK(child_box->IsPositioned());
  positioned_child_boxes_.push_back(child_box);
}

namespace {
math::PointF GetUsedPositionRelativeToAncestor(Box* box,
                                               ContainerBox* ancestor) {
  math::PointF relative_position = box->used_position();
  ContainerBox* current_ancestor = box->parent();
  while (current_ancestor != ancestor) {
    DCHECK(current_ancestor)
        << "Unable to find ancestor while traversing parents.";
    // We should not determine a used position through a transform, as
    // rectangles may not remain rectangles past it, and thus obtaining
    // a position may be misleading.
    DCHECK_EQ(cssom::KeywordValue::GetNone(),
              current_ancestor->computed_style()->transform());
    relative_position += current_ancestor->used_position().OffsetFromOrigin();
    current_ancestor = current_ancestor->parent();
  }

  return relative_position;
}
}  // namespace

void ContainerBox::UpdateUsedSizeOfPositionedChildren(
    const LayoutParams& layout_params) {
  LayoutParams child_layout_params = layout_params;

  // Compute sizes for positioned children with invalidated sizes.
  for (ChildBoxes::const_iterator iter = positioned_child_boxes_.begin();
       iter != positioned_child_boxes_.end(); ++iter) {
    Box* child_box = *iter;

    child_box->UpdateUsedSizeIfInvalid(child_layout_params);
    math::PointF static_position =
        GetUsedPositionRelativeToAncestor(child_box, this);

    UsedBoxMetrics horizontal_metrics = UsedBoxMetrics::ComputeHorizontal(
        used_width(), *child_box->computed_style());
    horizontal_metrics.size = child_box->used_width();
    horizontal_metrics.ResolveConstraints(used_width());
    child_box->set_used_left(horizontal_metrics.start_offset
                                 ? *horizontal_metrics.start_offset
                                 : static_position.x());


    UsedBoxMetrics vertical_metrics = UsedBoxMetrics::ComputeVertical(
        used_height(), *child_box->computed_style());
    vertical_metrics.size = child_box->used_height();
    vertical_metrics.ResolveConstraints(used_height());
    child_box->set_used_top(vertical_metrics.start_offset
                                ? *vertical_metrics.start_offset
                                : static_position.y());
  }
}

void ContainerBox::AddPositionedChildrenToRenderTree(
    render_tree::CompositionNode::Builder* composition_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  // Add all positioned children to the render tree.  It is important that
  // these are added in their own group like this since they must appear
  // above standard in-flow elements.
  for (ChildBoxes::const_iterator iter = positioned_child_boxes_.begin();
       iter != positioned_child_boxes_.end(); ++iter) {
    Box* child_box = *iter;
    child_box->AddToRenderTree(composition_node_builder,
                               node_animations_map_builder);
  }
}

void ContainerBox::AddContentToRenderTree(
    render_tree::CompositionNode::Builder* composition_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  // Render child boxes.
  // TODO(***REMOVED***): Take a stacking context into account:
  //               http://www.w3.org/TR/CSS21/visuren.html#z-index
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    if (!child_box->IsPositioned()) {
      child_box->AddToRenderTree(composition_node_builder,
                                 node_animations_map_builder);
    }
  }

  // Positioned children must be rendered on top of children rendered via the
  // standard flow.
  //   http://www.w3.org/TR/CSS21/visuren.html#z-index
  AddPositionedChildrenToRenderTree(composition_node_builder,
                                    node_animations_map_builder);
}

void ContainerBox::DumpChildrenWithIndent(std::ostream* stream,
                                          int indent) const {
  Box::DumpChildrenWithIndent(stream, indent);

  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->DumpWithIndent(stream, indent);
  }
}

}  // namespace layout
}  // namespace cobalt

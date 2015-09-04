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
#include "cobalt/cssom/number_value.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/math/transform_2d.h"

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
    Box* child_box = *iter;
    DCHECK_NE(this, child_box->parent())
        << "Move children within the same container node is not supported by "
           "this method.";

    child_box->parent_ = NULL;
    child_box->containing_block_ = NULL;
    OnChildAdded(child_box);
  }

  // Erase the children from their previous container's list of children.
  source_box->child_boxes_.weak_erase(source_start_non_const,
                                      source_end_non_const);
}

void ContainerBox::OnChildAdded(Box* child_box) {
  DCHECK(!child_box->parent());
  DCHECK(!child_box->containing_block());

  child_box->parent_ = this;
  child_box->containing_block_ = this;
}

// Returns true if the given style allows a container box to act as a containing
// block for absolutely positioned elements.  For example it will be true if
// this box's style is itself 'absolute'.
bool ContainerBox::IsContainingBlockForAbsoluteElements() const {
  return parent() == NULL ||
         computed_style()->position() == cssom::KeywordValue::GetAbsolute() ||
         computed_style()->position() == cssom::KeywordValue::GetRelative() ||
         computed_style()->transform() != cssom::KeywordValue::GetNone();
}

// Returns true if this container box serves as a stacking context for
// descendant elements.
bool ContainerBox::IsStackingContext() const {
  return parent() == NULL ||
         base::polymorphic_downcast<const cssom::NumberValue*>(
             computed_style()->opacity().get())->value() < 1.0f ||
         computed_style()->transform() != cssom::KeywordValue::GetNone() ||
         (IsPositioned() &&
          computed_style()->z_index() != cssom::KeywordValue::GetAuto());
}

void ContainerBox::AddContainingBlockChild(Box* child_box) {
  DCHECK_EQ(this, child_box->containing_block());
  positioned_child_boxes_.push_back(child_box);
}

void ContainerBox::AddStackingContextChild(Box* child_box) {
  DCHECK_EQ(this, child_box->stacking_context());
  int child_z_index = child_box->GetZIndex();
  DCHECK(child_z_index == 0 || IsStackingContext())
      << "Children with non-zero z-indices can only be added to container "
         "boxes that establish stacking contexts.";

  if (child_z_index < 0) {
    negative_z_index_child_.insert(child_box);
  } else {
    non_negative_z_index_child_.insert(child_box);
  }
}

namespace {

math::Vector2dF GetOffsetFromContainingBlock(Box* child_box) {
  math::Vector2dF relative_position;
  for (Box* ancestor_box = child_box->parent(),
            *containing_block = child_box->containing_block();
       ancestor_box != containing_block;
       ancestor_box = ancestor_box->parent()) {
    DCHECK(ancestor_box)
        << "Unable to find containing block while traversing parents.";
    // We should not determine a used position through a transform, as
    // rectangles may not remain rectangles past it, and thus obtaining
    // a position may be misleading.
    DCHECK_EQ(cssom::KeywordValue::GetNone(),
              ancestor_box->computed_style()->transform());

    relative_position += ancestor_box->GetContentBoxOffsetFromMarginBox();
    relative_position +=
        ancestor_box->margin_box_offset_from_containing_block();
  }
  return relative_position;
}

}  // namespace

void ContainerBox::UpdateRectOfPositionedChildBoxes(
    const LayoutParams& child_layout_params) {
  for (ChildBoxes::const_iterator child_box_iterator =
           positioned_child_boxes_.begin();
       child_box_iterator != positioned_child_boxes_.end();
       ++child_box_iterator) {
    Box* child_box = *child_box_iterator;

    DCHECK_EQ(this, child_box->containing_block());
    math::Vector2dF static_position_offset =
        GetOffsetFromContainingBlock(child_box);
    child_box->set_left(child_box->left() + static_position_offset.x());
    child_box->set_top(child_box->top() + static_position_offset.y());

    child_box->UpdateSize(child_layout_params);
  }
}

namespace {

math::Vector2dF GetOffsetFromStackingContext(Box* child_box) {
  math::Vector2dF relative_position;
  for (Box* containing_block = child_box->containing_block(),
            *stacking_context = child_box->stacking_context();
       containing_block != stacking_context;
       containing_block = containing_block->containing_block()) {
    DCHECK(containing_block) << "Unable to find stacking context while "
                                "traversing containing blocks.";
    // We should not determine a used position through a transform, as
    // rectangles may not remain rectangles past it, and thus obtaining
    // a position may be misleading.
    DCHECK_EQ(cssom::KeywordValue::GetNone(),
              containing_block->computed_style()->transform());

    relative_position += containing_block->GetContentBoxOffsetFromMarginBox();
    relative_position +=
        containing_block->margin_box_offset_from_containing_block();
  }
  return relative_position;
}

}  // namespace


void ContainerBox::RenderAndAnimateStackingContextChildren(
    const ZIndexSortedList& z_index_child_list,
    render_tree::CompositionNode::Builder* content_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  // Render all children of the passed in list in sorted order.
  for (ZIndexSortedList::const_iterator iter = z_index_child_list.begin();
       iter != z_index_child_list.end(); ++iter) {
    Box* child_box = *iter;

    DCHECK_EQ(this, child_box->stacking_context());
    math::Vector2dF position_offset = GetOffsetFromStackingContext(child_box);

    if (position_offset.x() == 0.0f && position_offset.y() == 0.0f) {
      // If there is no offset between the child box's containing block
      // and this box (the stacking context), we do not need a CompositionNode
      // for positioning.
      child_box->RenderAndAnimate(content_node_builder,
                                  node_animations_map_builder);
    } else {
      // Since the child box was laid out relative to its containing block,
      // but we are rendering it from the stacking context box, we must
      // transform it by the relative transform between the containing block
      // and stacking context.
      render_tree::CompositionNode::Builder position_offset_node_builder;
      child_box->RenderAndAnimate(&position_offset_node_builder,
                                  node_animations_map_builder);
      content_node_builder->AddChild(
          new render_tree::CompositionNode(position_offset_node_builder.Pass()),
          math::TranslateMatrix(position_offset.x(), position_offset.y()));
    }
  }
}

void ContainerBox::SplitBidiLevelRuns() {
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->SplitBidiLevelRuns();
  }
}

void ContainerBox::UpdateCrossReferencesWithContext(
    ContainerBox* absolute_containing_block, ContainerBox* stacking_context) {
  // First setup this box with cross-references as necessary.
  Box::UpdateCrossReferencesWithContext(absolute_containing_block,
                                        stacking_context);

  // In addition to updating our cross references like a normal Box, we
  // also recursively update the cross-references of our children.
  ContainerBox* absolute_containing_block_of_children =
      IsContainingBlockForAbsoluteElements() ? this : absolute_containing_block;

  ContainerBox* stacking_context_of_children =
      IsStackingContext() ? this : stacking_context;

  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->UpdateCrossReferencesWithContext(
        absolute_containing_block_of_children, stacking_context_of_children);
  }
}

void ContainerBox::RenderAndAnimateContent(
    render_tree::CompositionNode::Builder* border_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  float content_box_left = border_left_width() + padding_left();
  float content_box_top = border_top_width() + padding_top();
  render_tree::CompositionNode::Builder offset_content_node_builder;
  render_tree::CompositionNode::Builder* content_node_builder =
      content_box_left == 0 && content_box_top == 0
          ? border_node_builder
          : &offset_content_node_builder;

  // Render all positioned children in our stacking context that have negative
  // z-index values.
  //   http://www.w3.org/TR/CSS21/visuren.html#z-index
  RenderAndAnimateStackingContextChildren(negative_z_index_child_,
                                          content_node_builder,
                                          node_animations_map_builder);
  // Render laid out child boxes.
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    if (!child_box->IsPositioned()) {
      child_box->RenderAndAnimate(content_node_builder,
                                  node_animations_map_builder);
    }
  }

  // Render all positioned children with non-negative z-index values.
  //   http://www.w3.org/TR/CSS21/visuren.html#z-index
  RenderAndAnimateStackingContextChildren(non_negative_z_index_child_,
                                          content_node_builder,
                                          node_animations_map_builder);

  if (!offset_content_node_builder.composed_children().empty()) {
    scoped_refptr<render_tree::CompositionNode> content_node =
        new render_tree::CompositionNode(offset_content_node_builder.Pass());
    border_node_builder->AddChild(
        content_node, math::TranslateMatrix(content_box_left, content_box_top));
  }
}

#ifdef COBALT_BOX_DUMP_ENABLED

void ContainerBox::DumpChildrenWithIndent(std::ostream* stream,
                                          int indent) const {
  Box::DumpChildrenWithIndent(stream, indent);

  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->DumpWithIndent(stream, indent);
  }
}

#endif  // COBALT_BOX_DUMP_ENABLED

}  // namespace layout
}  // namespace cobalt

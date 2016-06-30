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
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        css_computed_style_declaration,
    UsedStyleProvider* used_style_provider, StatTracker* stat_tracker)
    : Box(css_computed_style_declaration, used_style_provider, stat_tracker),
      update_size_results_valid_(false),
      are_bidi_levels_runs_split_(false) {}

ContainerBox::~ContainerBox() {}

// Given a non-const container and a const iterator from that container, this
// will return a non-const iterator.
Boxes::iterator ContainerBox::RemoveConst(Boxes* container,
                                          Boxes::const_iterator const_iter) {
  // Since ChildBoxes is a vector, std::distance and std::advance are both
  // constant time.
  Boxes::iterator iter = container->begin();
  std::advance(iter, std::distance<Boxes::const_iterator>(iter, const_iter));
  return iter;
}

void ContainerBox::PushBackDirectChild(const scoped_refptr<Box>& child_box) {
  child_boxes_.push_back(child_box);
  OnChildAdded(child_box);
}

Boxes::const_iterator ContainerBox::InsertDirectChild(
    Boxes::const_iterator position, const scoped_refptr<Box>& child_box) {
  Boxes::const_iterator inserted =
      child_boxes_.insert(RemoveConst(&child_boxes_, position), child_box);
  OnChildAdded(child_box);
  if (child_box->IsPositioned() || child_box->IsTransformed()) {
    child_box->SetupAsPositionedChild();
  }

  return inserted;
}

void ContainerBox::MoveChildrenFrom(
    Boxes::const_iterator position_in_destination, ContainerBox* source_box,
    Boxes::const_iterator source_start, Boxes::const_iterator source_end) {
  // Add the children to our list of child boxes.
  Boxes::iterator source_start_non_const =
      RemoveConst(&source_box->child_boxes_, source_start);
  Boxes::iterator source_end_non_const =
      RemoveConst(&source_box->child_boxes_, source_end);
  child_boxes_.insert(RemoveConst(&child_boxes_, position_in_destination),
                      source_start_non_const, source_end_non_const);

  // Setup any link relationships given our new parent.
  for (Boxes::const_iterator iter = source_start; iter != source_end; ++iter) {
    Box* child_box = *iter;
    DCHECK_NE(this, child_box->parent())
        << "Move children within the same container node is not supported by "
           "this method.";
    if (child_box->IsPositioned() || child_box->IsTransformed()) {
      child_box->RemoveAsPositionedChild();
      // For positioned boxes, the parent box is not always also the containing
      // block. Instead of calling OnChildAdded(), only update the containing
      // block if that is the case.

      child_box->parent_ = this;
      update_size_results_valid_ = false;
    } else {
      child_box->parent_ = NULL;
      OnChildAdded(child_box);
    }
    child_box->UpdateCrossReferences();
  }

  // Erase the children from their previous container's list of children.
  source_box->child_boxes_.erase(source_start_non_const, source_end_non_const);

  // Invalidate the source box's size calculations.
  source_box->update_size_results_valid_ = false;
}

void ContainerBox::OnChildAdded(Box* child_box) {
  DCHECK(!child_box->parent());

  child_box->parent_ = this;

  // Invalidate our size calculations as a result of having our set of
  // children modified.
  update_size_results_valid_ = false;
}

// Returns true if the given style allows a container box to act as a containing
// block for absolutely positioned elements.  For example it will be true if
// this box's style is itself 'absolute'.
bool ContainerBox::IsContainingBlockForPositionAbsoluteElements() const {
  return parent() == NULL || IsPositioned() || IsTransformed();
}

bool ContainerBox::IsContainingBlockForPositionFixedElements() const {
  return parent() == NULL || IsTransformed();
}

// Returns true if this container box serves as a stacking context for
// descendant elements.  The core stacking context creation criteria is given
// here (https://www.w3.org/TR/CSS21/visuren.html#z-index) however it is
// extended by various other specification documents such as those describing
// opacity (https://www.w3.org/TR/css3-color/#transparency) and transforms
// (https://www.w3.org/TR/css3-transforms/#transform-rendering).
bool ContainerBox::IsStackingContext() const {
  bool has_opacity =
      base::polymorphic_downcast<const cssom::NumberValue*>(
          computed_style()->opacity().get())->value() < 1.0f;
  bool is_positioned_with_non_auto_z_index =
      IsPositioned() &&
      computed_style()->z_index() != cssom::KeywordValue::GetAuto();

  return parent() == NULL ||
         has_opacity ||
         IsTransformed() ||
         is_positioned_with_non_auto_z_index;
}

void ContainerBox::AddContainingBlockChild(Box* child_box) {
  DCHECK_NE(this, child_box);
  DCHECK_EQ(this, child_box->GetContainingBlock());
  positioned_child_boxes_.push_back(child_box);
}

void ContainerBox::AddStackingContextChild(Box* child_box) {
  DCHECK_NE(this, child_box);
  DCHECK_EQ(this, child_box->GetStackingContext());
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

void ContainerBox::RemoveContainingBlockChild(Box* child_box) {
  DCHECK(child_box->IsPositioned() || child_box->IsTransformed());

  std::vector<Box*>::iterator positioned_child_iterator =
      std::find(positioned_child_boxes_.begin(), positioned_child_boxes_.end(),
                child_box);
  if (positioned_child_iterator != positioned_child_boxes_.end()) {
    positioned_child_boxes_.erase(positioned_child_iterator);
  } else {
    NOTREACHED();
  }
}

void ContainerBox::RemoveStackingContextChild(Box* child_box) {
  DCHECK(child_box->IsPositioned() || child_box->IsTransformed());

  if (child_box->GetZIndex() < 0) {
    ZIndexSortedList::iterator source_child_iterator =
        std::find(negative_z_index_child_.begin(),
                  negative_z_index_child_.end(), child_box);
    if (source_child_iterator != negative_z_index_child_.end()) {
      negative_z_index_child_.erase(source_child_iterator);
    }
  } else {
    ZIndexSortedList::iterator source_child_iterator =
        std::find(non_negative_z_index_child_.begin(),
                  non_negative_z_index_child_.end(), child_box);
    if (source_child_iterator != non_negative_z_index_child_.end()) {
      non_negative_z_index_child_.erase(source_child_iterator);
    }
  }
}

namespace {

Vector2dLayoutUnit GetOffsetFromContainingBlockToParent(Box* child_box) {
  Vector2dLayoutUnit relative_position;
  for (Box *ancestor_box = child_box->parent(),
           *containing_block = child_box->GetContainingBlock();
       ancestor_box != containing_block;
       ancestor_box = ancestor_box->parent()) {
    DCHECK(ancestor_box)
        << "Unable to find containing block while traversing parents.";
    // We should not determine a used position through a transform, as
    // rectangles may not remain rectangles past it, and thus obtaining
    // a position may be misleading.
    DCHECK(!ancestor_box->IsTransformed());

    relative_position += ancestor_box->GetContentBoxOffsetFromMarginBox();
    relative_position +=
        ancestor_box->margin_box_offset_from_containing_block();
  }
  return relative_position;
}

}  // namespace

bool ContainerBox::ValidateUpdateSizeInputs(const LayoutParams& params) {
  // Take into account whether our children have been modified to determine
  // if our sizes are invalid and need to be recomputed.
  if (Box::ValidateUpdateSizeInputs(params) && update_size_results_valid_) {
    return true;
  } else {
    update_size_results_valid_ = true;
    return false;
  }
}

ContainerBox* ContainerBox::AsContainerBox() { return this; }
const ContainerBox* ContainerBox::AsContainerBox() const { return this; }

void ContainerBox::UpdateRectOfPositionedChildBoxes(
    const LayoutParams& relative_child_layout_params,
    const LayoutParams& absolute_child_layout_params) {
  for (std::vector<Box*>::const_iterator child_box_iterator =
           positioned_child_boxes_.begin();
       child_box_iterator != positioned_child_boxes_.end();
       ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    DCHECK_EQ(this, child_box->GetContainingBlock());

    const scoped_refptr<cssom::PropertyValue>& child_box_position =
        child_box->computed_style()->position();

    if (child_box_position == cssom::KeywordValue::GetRelative()) {
      UpdateOffsetOfRelativelyPositionedChildBox(child_box,
                                                 relative_child_layout_params);
    } else if (child_box_position == cssom::KeywordValue::GetAbsolute()) {
      UpdateRectOfAbsolutelyPositionedChildBox(child_box,
                                               absolute_child_layout_params);
    } else {
      UpdateRectOfFixedPositionedChildBox(child_box,
                                          relative_child_layout_params);
    }
  }
}

void ContainerBox::UpdateOffsetOfRelativelyPositionedChildBox(
    Box* child_box, const LayoutParams& child_layout_params) {
  DCHECK_EQ(child_box->computed_style()->position(),
            cssom::KeywordValue::GetRelative());

  base::optional<LayoutUnit> maybe_left = GetUsedLeftIfNotAuto(
      child_box->computed_style(), child_layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_right = GetUsedRightIfNotAuto(
      child_box->computed_style(), child_layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_top = GetUsedTopIfNotAuto(
      child_box->computed_style(), child_layout_params.containing_block_size);
  base::optional<LayoutUnit> maybe_bottom = GetUsedBottomIfNotAuto(
      child_box->computed_style(), child_layout_params.containing_block_size);

  Vector2dLayoutUnit offset;

  // The following steps are performed according to the procedure described
  // here: https://www.w3.org/TR/CSS21/visuren.html#relative-positioning

  // For relatively positioned elements, 'left' and 'right' move the box(es)
  // horizontally, without changing their size.
  if (!maybe_left && !maybe_right) {
    // If both 'left' and 'right' are 'auto' (their initial values), the used
    // values are '0' (i.e., the boxes stay in their original position).
    offset.set_x(LayoutUnit());
  } else if (maybe_left && !maybe_right) {
    // If 'right' is 'auto', its used value is minus the value of 'left'.
    offset.set_x(*maybe_left);
  } else if (!maybe_left && maybe_right) {
    // If 'left' is 'auto', its used value is minus the value of 'right'
    offset.set_x(-*maybe_right);
  } else {
    // If neither 'left' nor 'right' is 'auto', the position is
    // over-constrained, and one of them has to be ignored. If the 'direction'
    // property of the containing block is 'ltr', the value of 'left' wins and
    // 'right' becomes -'left'. If 'direction' of the containing block is 'rtl',
    // 'right' wins and 'left' is ignored.

    // TODO(***REMOVED***): Take into account the value of the 'direction' property,
    //               which doesn't exist at the time of this writing.
    offset.set_x(*maybe_left);
  }

  // The 'top' and 'bottom' properties move relatively positioned element(s) up
  // or down without changing their size.
  if (!maybe_top && !maybe_bottom) {
    // If both are 'auto', their used values are both '0'.
    offset.set_y(LayoutUnit());
  } else if (maybe_top && !maybe_bottom) {
    // If one of them is 'auto', it becomes the negative of the other.
    offset.set_y(*maybe_top);
  } else if (!maybe_top && maybe_bottom) {
    // If one of them is 'auto', it becomes the negative of the other.
    offset.set_y(-*maybe_bottom);
  } else {
    // If neither is 'auto', 'bottom' is ignored (i.e., the used value of
    // 'bottom' will be minus the value of 'top').
    offset.set_y(*maybe_top);
  }

  child_box->set_left(child_box->left() + offset.x());
  child_box->set_top(child_box->top() + offset.y());
}

void ContainerBox::UpdateRectOfFixedPositionedChildBox(
    Box* child_box, const LayoutParams& child_layout_params) {
  Vector2dLayoutUnit offset_from_containing_block_to_parent =
      GetOffsetFromContainingBlockToParent(child_box);
  child_box->SetStaticPositionLeftFromContainingBlockToParent(
      offset_from_containing_block_to_parent.x());
  child_box->SetStaticPositionTopFromContainingBlockToParent(
      offset_from_containing_block_to_parent.y());
  child_box->UpdateSize(child_layout_params);
}

void ContainerBox::UpdateRectOfAbsolutelyPositionedChildBox(
    Box* child_box, const LayoutParams& child_layout_params) {
  Vector2dLayoutUnit offset_from_containing_block_to_parent =
      GetOffsetFromContainingBlockToParent(child_box);
  // The containing block is formed by the padding box instead of the content
  // box, as described in
  // http://www.w3.org/TR/CSS21/visudet.html#containing-block-details.
  offset_from_containing_block_to_parent += GetContentBoxOffsetFromPaddingBox();
  child_box->SetStaticPositionLeftFromContainingBlockToParent(
      offset_from_containing_block_to_parent.x());
  child_box->SetStaticPositionTopFromContainingBlockToParent(
      offset_from_containing_block_to_parent.y());
  child_box->UpdateSize(child_layout_params);
}

namespace {

Vector2dLayoutUnit GetOffsetFromContainingBlockToStackingContext(
    Box* child_box) {
  DCHECK((child_box->computed_style()->position() ==
          cssom::KeywordValue::GetFixed()) ||
         (child_box->computed_style()->position() ==
          cssom::KeywordValue::GetAbsolute()));

  Vector2dLayoutUnit relative_position;
  for (Box *containing_block = child_box->GetContainingBlock(),
           *current_box = child_box->GetStackingContext();
       current_box != containing_block;
       current_box = current_box->GetContainingBlock()) {
    if (!current_box) {
      NOTREACHED() << "Unable to find stacking context while "
                      "traversing containing blocks.";
      break;
    }
    // We should not determine a used position through a transform, as
    // rectangles may not remain rectangles past it, and thus obtaining
    // a position may be misleading.
    DCHECK(!current_box->IsTransformed());

    relative_position += current_box->GetContentBoxOffsetFromMarginBox();
    relative_position += current_box->margin_box_offset_from_containing_block();
  }
  return relative_position;
}

Vector2dLayoutUnit GetOffsetFromStackingContextToContainingBlock(
    Box* child_box) {
  const scoped_refptr<cssom::PropertyValue>& child_box_position =
      child_box->computed_style()->position();
  if (child_box_position == cssom::KeywordValue::GetFixed()) {
    // Elements with fixed position will have their containing block farther
    // up the hierarchy than the stacking context, so handle this case
    // specially.
    return -GetOffsetFromContainingBlockToStackingContext(child_box);
  }

  Vector2dLayoutUnit relative_position;
  if (child_box_position == cssom::KeywordValue::GetAbsolute()) {
    // The containing block is formed by the padding box instead of the content
    // box, as described in
    // http://www.w3.org/TR/CSS21/visudet.html#containing-block-details.
    relative_position -=
        child_box->GetContainingBlock()->GetContentBoxOffsetFromPaddingBox();
  }
  for (Box *current_box = child_box->GetContainingBlock(),
           *stacking_context = child_box->GetStackingContext();
       current_box != stacking_context;
       current_box = current_box->GetContainingBlock()) {
    if (!current_box) {
      // Elements with absolute position may have their containing block farther
      // up the hierarchy than the stacking context, so handle this case here.
      DCHECK(child_box_position == cssom::KeywordValue::GetAbsolute());
      return -GetOffsetFromContainingBlockToStackingContext(child_box);
    }
    // We should not determine a used position through a transform, as
    // rectangles may not remain rectangles past it, and thus obtaining
    // a position may be misleading.
    DCHECK(!current_box->IsTransformed());

    relative_position += current_box->GetContentBoxOffsetFromMarginBox();
    relative_position += current_box->margin_box_offset_from_containing_block();

    if (current_box->computed_style()->position() ==
        cssom::KeywordValue::GetAbsolute()) {
      relative_position -= current_box->GetContainingBlock()
                               ->GetContentBoxOffsetFromPaddingBox();
    }
  }
  return relative_position;
}

}  // namespace

void ContainerBox::RenderAndAnimateStackingContextChildren(
    const ZIndexSortedList& z_index_child_list,
    render_tree::CompositionNode::Builder* content_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder,
    const Vector2dLayoutUnit& offset_from_parent_node) const {
  // Render all children of the passed in list in sorted order.
  for (ZIndexSortedList::const_iterator iter = z_index_child_list.begin();
       iter != z_index_child_list.end(); ++iter) {
    Box* child_box = *iter;

    DCHECK_EQ(this, child_box->GetStackingContext());
    Vector2dLayoutUnit position_offset =
        GetOffsetFromStackingContextToContainingBlock(child_box) +
        offset_from_parent_node;

    child_box->RenderAndAnimate(content_node_builder,
                                node_animations_map_builder, position_offset);
  }
}

void ContainerBox::SplitBidiLevelRuns() {
  // Only split the child boxes if the bidi level runs haven't already been
  // split.
  if (!are_bidi_levels_runs_split_) {
    are_bidi_levels_runs_split_ = true;

    for (Boxes::const_iterator child_box_iterator = child_boxes_.begin();
         child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
      Box* child_box = *child_box_iterator;
      child_box->SplitBidiLevelRuns();
    }
  }
}

void ContainerBox::UpdateCrossReferencesWithContext(
    ContainerBox* fixed_containing_block,
    ContainerBox* absolute_containing_block, ContainerBox* stacking_context) {
  positioned_child_boxes_.clear();
  negative_z_index_child_.clear();
  non_negative_z_index_child_.clear();

  // First setup this box with cross-references as necessary.
  Box::UpdateCrossReferencesWithContext(
      fixed_containing_block, absolute_containing_block, stacking_context);

  // In addition to updating our cross references like a normal Box, we
  // also recursively update the cross-references of our children.
  ContainerBox* fixed_containing_block_of_children =
      IsContainingBlockForPositionFixedElements() ? this
                                                  : fixed_containing_block;

  ContainerBox* absolute_containing_block_of_children =
      IsContainingBlockForPositionAbsoluteElements()
          ? this
          : absolute_containing_block;

  ContainerBox* stacking_context_of_children =
      IsStackingContext() ? this : stacking_context;

  for (Boxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->UpdateCrossReferencesWithContext(
        fixed_containing_block_of_children,
        absolute_containing_block_of_children, stacking_context_of_children);
  }
}

void ContainerBox::RenderAndAnimateContent(
    render_tree::CompositionNode::Builder* border_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  Vector2dLayoutUnit content_box_offset(border_left_width() + padding_left(),
                                        border_top_width() + padding_top());
  // Render all positioned children in our stacking context that have negative
  // z-index values.
  //   https://www.w3.org/TR/CSS21/visuren.html#z-index
  RenderAndAnimateStackingContextChildren(
      negative_z_index_child_, border_node_builder, node_animations_map_builder,
      content_box_offset);
  // Render laid out child boxes.
  for (Boxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    if (!child_box->IsPositioned() && !child_box->IsTransformed()) {
      child_box->RenderAndAnimate(
          border_node_builder, node_animations_map_builder, content_box_offset);
    }
  }
  // Render all positioned children with non-negative z-index values.
  //   https://www.w3.org/TR/CSS21/visuren.html#z-index
  RenderAndAnimateStackingContextChildren(
      non_negative_z_index_child_, border_node_builder,
      node_animations_map_builder, content_box_offset);
}

#ifdef COBALT_BOX_DUMP_ENABLED

void ContainerBox::DumpChildrenWithIndent(std::ostream* stream,
                                          int indent) const {
  Box::DumpChildrenWithIndent(stream, indent);

  for (Boxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->DumpWithIndent(stream, indent);
  }
}

#endif  // COBALT_BOX_DUMP_ENABLED

}  // namespace layout
}  // namespace cobalt

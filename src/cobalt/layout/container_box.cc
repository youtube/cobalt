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
    UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker)
    : Box(css_computed_style_declaration, used_style_provider,
          layout_stat_tracker),
      update_size_results_valid_(false),
      are_cross_references_valid_(false),
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
  // Verify that the child doesn't have a pre-existing parent.
  DCHECK(!child_box->parent());

  // Verify that this container hasn't had its sizes and cross references
  // already updated. This is because children should only ever be added to
  // containers created during the current box generation run.
  DCHECK(!update_size_results_valid_);
  DCHECK(!are_cross_references_valid_);

  child_box->parent_ = this;
  child_boxes_.push_back(child_box);
}

Boxes::const_iterator ContainerBox::InsertSplitSiblingOfDirectChild(
    Boxes::const_iterator child_position) {
  Box* split_sibling = (*child_position)->GetSplitSibling();

  // Verify that the split sibling exists and that it doesn't have a
  // pre-existing parent.
  DCHECK(split_sibling);
  DCHECK(!split_sibling->parent());

  // Set the parent of the split sibling to this container.
  split_sibling->parent_ = this;

  // Add the split sibling to this container after it's sibling.
  Boxes::const_iterator split_sibling_position = child_boxes_.insert(
      RemoveConst(&child_boxes_, ++child_position), split_sibling);

  // Invalidate the size now that the children have changed.
  update_size_results_valid_ = false;

  // Check to see if the split sibling is positioned, which means that it
  // needs to invalidate its cross references.
  // NOTE: Only block level and atomic inline-level elements are transformable.
  // As these are not splittable, the split sibling does not need to be checked
  // for being transformed.
  // https://www.w3.org/TR/css-transforms-1/#transformable-element
  DCHECK(!split_sibling->IsTransformable());
  if (split_sibling->IsPositioned()) {
    // Absolutely positioned boxes are not splittable.
    DCHECK(!split_sibling->IsAbsolutelyPositioned());
    // This container is the containing block because the split sibling cannot
    // be an absolutely positioned box.
    are_cross_references_valid_ = false;
    split_sibling->GetStackingContext()->are_cross_references_valid_ = false;
  }

  return split_sibling_position;
}

void ContainerBox::MoveDirectChildrenToSplitSibling(
    Boxes::const_iterator start_position) {
  // Verify that the move includes children.
  DCHECK(start_position != child_boxes_.end());

  ContainerBox* split_sibling = GetSplitSibling()->AsContainerBox();

  // Verify that the split sibling exists and that it hasn't been processed yet.
  DCHECK(split_sibling);
  DCHECK(!split_sibling->parent());
  DCHECK(!split_sibling->update_size_results_valid_);
  DCHECK(!split_sibling->are_cross_references_valid_);

  // Update the parent of the children being moved.
  for (Boxes::const_iterator iter = start_position; iter != child_boxes_.end();
       ++iter) {
    (*iter)->parent_ = split_sibling;
  }

  // Add the children to the split sibling's list of children.
  Boxes::iterator source_start_non_const =
      RemoveConst(&child_boxes_, start_position);
  Boxes::iterator source_end_non_const =
      RemoveConst(&child_boxes_, child_boxes_.end());
  split_sibling->child_boxes_.insert(
      RemoveConst(&split_sibling->child_boxes_,
                  split_sibling->child_boxes_.end()),
      source_start_non_const, source_end_non_const);

  // Erase the children from this container's list of children.
  child_boxes_.erase(source_start_non_const, source_end_non_const);

  // Invalidate the size now that the children have changed.
  update_size_results_valid_ = false;

  // Children are only being removed from this container. As a result, the cross
  // references only need to be invalidated if there is a non-empty cross
  // reference list that can potentially lose an element.
  if (!positioned_child_boxes_.empty() || !negative_z_index_child_.empty() ||
      !non_negative_z_index_child_.empty()) {
    are_cross_references_valid_ = false;
  }
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

void ContainerBox::UpdateCrossReferences() {
  if (!are_cross_references_valid_) {
    // Cross references are not cleared when they are invalidated. This is
    // because they can be invalidated while they are being walked if a
    // relatively positioned descendant is split. Therefore, they need to be
    // cleared now.
    positioned_child_boxes_.clear();
    negative_z_index_child_.clear();
    non_negative_z_index_child_.clear();

    bool is_nearest_containing_block = true;
    bool is_nearest_absolute_containing_block =
        IsContainingBlockForPositionAbsoluteElements();
    bool is_nearest_fixed_containing_block =
        IsContainingBlockForPositionFixedElements();
    bool is_nearest_stacking_context = IsStackingContext();

    for (Boxes::const_iterator child_box_iterator = child_boxes_.begin();
         child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
      Box* child_box = *child_box_iterator;
      child_box->UpdateCrossReferencesOfContainerBox(
          this, is_nearest_containing_block,
          is_nearest_absolute_containing_block,
          is_nearest_fixed_containing_block, is_nearest_stacking_context);
    }

    are_cross_references_valid_ = true;
  }
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

void ContainerBox::InvalidateCrossReferencesOfBoxAndAncestors() {
  // NOTE: The cross reference containers are not cleared here. Instead they are
  // cleared when the cross references are updated.
  are_cross_references_valid_ = false;
  Box::InvalidateCrossReferencesOfBoxAndAncestors();
}

ContainerBox* ContainerBox::AsContainerBox() { return this; }
const ContainerBox* ContainerBox::AsContainerBox() const { return this; }

void ContainerBox::UpdateRectOfPositionedChildBoxes(
    const LayoutParams& relative_child_layout_params,
    const LayoutParams& absolute_child_layout_params) {
  // Ensure that the cross references are up to date.
  UpdateCrossReferences();

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
    // Verify that the positioned child boxes didn't get cleared during the
    // walk. This should never happen because the cross references being
    // invalidated should not cause them to be cleared.
    DCHECK_GT(positioned_child_boxes_.size(), size_t(0));
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

    // TODO: Take into account the value of the 'direction' property, which
    //       doesn't exist at the time of this writing.
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

void ContainerBox::UpdateCrossReferencesOfContainerBox(
    ContainerBox* source_box, bool is_nearest_containing_block,
    bool is_nearest_absolute_containing_block,
    bool is_nearest_fixed_containing_block, bool is_nearest_stacking_context) {
  // First update the source container box's cross references with this box.
  Box::UpdateCrossReferencesOfContainerBox(
      source_box, is_nearest_containing_block,
      is_nearest_absolute_containing_block, is_nearest_fixed_containing_block,
      is_nearest_stacking_context);

  // In addition to updating the source container box's cross references with
  // this box, we also recursively update it with our children.

  // Set the nearest flags for the children. If this container box is any of the
  // specified types, then the target container box cannot be the nearest box of
  // that type for the children.

  bool is_nearest_containing_block_of_children = false;

  bool is_nearest_absolute_containing_block_of_children =
      is_nearest_absolute_containing_block &&
      !IsContainingBlockForPositionAbsoluteElements();

  bool is_nearest_fixed_containing_block_of_children =
      is_nearest_fixed_containing_block &&
      !IsContainingBlockForPositionFixedElements();

  bool is_nearest_stacking_context_of_children =
      is_nearest_stacking_context && !IsStackingContext();

  // Only process the children if the target container box is still the nearest
  // box of one of the types. If it is not, then it is impossible for any of the
  // children to be added to the cross references.
  if (is_nearest_absolute_containing_block_of_children ||
      is_nearest_fixed_containing_block_of_children ||
      is_nearest_stacking_context_of_children) {
    for (Boxes::const_iterator child_box_iterator = child_boxes_.begin();
         child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
      Box* child_box = *child_box_iterator;
      child_box->UpdateCrossReferencesOfContainerBox(
          source_box, is_nearest_containing_block_of_children,
          is_nearest_absolute_containing_block_of_children,
          is_nearest_fixed_containing_block_of_children,
          is_nearest_stacking_context_of_children);
    }
  }
}

void ContainerBox::RenderAndAnimateContent(
    render_tree::CompositionNode::Builder* border_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  // Ensure that the cross references are up to date.
  const_cast<ContainerBox*>(this)->UpdateCrossReferences();

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

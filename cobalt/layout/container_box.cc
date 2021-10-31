// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/layout/container_box.h"

#include <algorithm>

#include "cobalt/cssom/computed_style_utils.h"
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
      are_bidi_levels_runs_split_(false),
      next_draw_order_position_(0) {}

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
  // needs to invalidate its containing block's cross references.
  bool is_positioned = split_sibling->IsPositioned();
  if (is_positioned) {
    split_sibling->GetContainingBlock()->are_cross_references_valid_ = false;
  }
  // Check to see if the split sibling is a stacking context child, which means
  // that it needs to invalidate its stacking context's cross references.
  if (is_positioned || split_sibling->IsStackingContext()) {
    split_sibling->GetStackingContext()->are_cross_references_valid_ = false;
  }

  // Invalidate the render tree nodes now that the children have changed.
  InvalidateRenderTreeNodesOfBoxAndAncestors();

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

  // Handle invalidating cross references. Children are only being removed from
  // this container, so cross references only need to be invalidated if there is
  // a non-empty cross reference list that can potentially be impacted.

  // If there are any positioned boxes, then they need to be re-generated.
  if (!positioned_child_boxes_.empty()) {
    are_cross_references_valid_ = false;
  }

  // There are two cases where the stacking context's cross references can be
  // impacted by children moving from one container to another. With both cases,
  // stacking context children must exist or there is nothing to update.
  //   1. Stacking context children are potentially moving from this child
  //      container to the split sibling child container.
  //   2. Stacking context children contained within this overflow hidden
  //      container are potentially moving to the split sibling overflow hidden
  //      container.
  if (HasStackingContextChildren() || IsOverflowCropped(computed_style())) {
    // Walk up the tree until the nearest stacking context is found. If this box
    // is a stacking context, then it will be used.
    ContainerBox* nearest_stacking_context = this;
    while (!nearest_stacking_context->IsStackingContext()) {
      nearest_stacking_context = nearest_stacking_context->parent();
    }
    if (nearest_stacking_context->HasStackingContextChildren()) {
      nearest_stacking_context->are_cross_references_valid_ = false;
    }
  }

  // Invalidate the render tree nodes now that the children have changed.
  InvalidateRenderTreeNodesOfBoxAndAncestors();
}

bool ContainerBox::IsContainingBlockForPositionAbsoluteElements() const {
  return parent() == NULL ||
         computed_style()->IsContainingBlockForPositionAbsoluteElements();
}

bool ContainerBox::IsContainingBlockForPositionFixedElements() const {
  return parent() == NULL || IsTransformed();
}

bool ContainerBox::IsStackingContext() const {
  // The core stacking context creation criteria is given here
  // (https://www.w3.org/TR/CSS21/visuren.html#z-index) however it is extended
  // by various other specification documents such as those describing opacity
  // (https://www.w3.org/TR/css3-color/#transparency) and transforms
  // (https://www.w3.org/TR/css3-transforms/#transform-rendering).
  // NOTE: Fixed position elements are treated as stacking contexts. While the
  // spec does not specify that they create a stacking context, this is the
  // functionality of Chrome, Firefox, Edge, and Safari.
  return parent() == NULL ||
         (base::polymorphic_downcast<const cssom::NumberValue*>(
              computed_style()->opacity().get())
              ->value() < 1.0f) ||
         IsTransformed() ||
         (IsPositioned() &&
          computed_style()->z_index() != cssom::KeywordValue::GetAuto()) ||
         computed_style()->position() == cssom::KeywordValue::GetFixed();
}

void ContainerBox::UpdateCrossReferences() {
  if (!are_cross_references_valid_) {
    // If this point is reached, then the cross references for this container
    // box are being re-generated.
    layout_stat_tracker()->OnUpdateCrossReferences();

    bool is_stacking_context = IsStackingContext();

    // Cross references are not cleared when they are invalidated. This is
    // because they can be invalidated while they are being walked if a
    // relatively positioned descendant is split. Therefore, they need to be
    // cleared now before they are regenerated.
    positioned_child_boxes_.clear();
    if (is_stacking_context) {
      negative_z_index_stacking_context_children_.clear();
      non_negative_z_index_stacking_context_children_.clear();
    }

    // This stack tracks all container boxes within the stacking context. The
    // container boxes will only be pushed and popped if the current box is
    // their stacking context.
    StackingContextContainerBoxStack stacking_context_container_box_stack;

    const RelationshipToBox kNearestContainingBlockOfChildren = kIsBox;
    RelationshipToBox nearest_absolute_containing_block =
        IsContainingBlockForPositionAbsoluteElements() ? kIsBox
                                                       : kIsBoxAncestor;
    RelationshipToBox nearest_fixed_containing_block =
        IsContainingBlockForPositionFixedElements() ? kIsBox : kIsBoxAncestor;
    RelationshipToBox nearest_stacking_context =
        is_stacking_context ? kIsBox : kIsBoxAncestor;

    for (Boxes::const_iterator child_box_iterator = child_boxes_.begin();
         child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
      Box* child_box = child_box_iterator->get();
      child_box->UpdateCrossReferencesOfContainerBox(
          this, kNearestContainingBlockOfChildren,
          nearest_absolute_containing_block, nearest_fixed_containing_block,
          nearest_stacking_context, &stacking_context_container_box_stack);
    }

    are_cross_references_valid_ = true;
  }
}

void ContainerBox::AddContainingBlockChild(Box* child_box) {
  DCHECK_NE(this, child_box);
  DCHECK_EQ(this, child_box->GetContainingBlock());
  positioned_child_boxes_.push_back(child_box);
}

void ContainerBox::AddStackingContextChild(
    Box* child_box, int child_z_index,
    RelationshipToBox containing_block_relationship,
    const ContainingBlocksWithOverflowHidden& overflow_hidden_to_apply) {
  DCHECK_NE(this, child_box);
  DCHECK_EQ(child_z_index, child_box->GetZIndex());
  // If this is a stacking context, then verify that the child box's stacking
  // context is this stacking context.
  // Otherwise, verify that this box's stacking context is also the child box's
  // stacking context and that the z_index of the child is 0 (non-zero z-index
  // children must be added directly to the stacking context).
  DCHECK(this->IsStackingContext()
             ? this == child_box->GetStackingContext()
             : this->GetStackingContext() == child_box->GetStackingContext() &&
                   child_z_index == 0);

  ZIndexSortedList& stacking_context_children =
      child_z_index < 0 ? negative_z_index_stacking_context_children_
                        : non_negative_z_index_stacking_context_children_;
  stacking_context_children.insert(StackingContextChildInfo(
      child_box, child_z_index, containing_block_relationship,
      overflow_hidden_to_apply));
}

bool ContainerBox::HasStackingContextChildren() const {
  return !negative_z_index_stacking_context_children_.empty() ||
         !non_negative_z_index_stacking_context_children_.empty();
}

namespace {

InsetsLayoutUnit GetOffsetFromContainingBlockToParentOfAbsolutelyPositionedBox(
    const ContainerBox* containing_block, Box* child_box) {
  // NOTE: Bottom inset is not computed and should not be queried.
  DCHECK(child_box->IsAbsolutelyPositioned());
  DCHECK_EQ(child_box->GetContainingBlock(), containing_block);

  InsetsLayoutUnit offset;

  const ContainerBox* current_box = child_box->parent();
  while (current_box != containing_block) {
    DCHECK(current_box->parent());
    DCHECK(!current_box->IsTransformed());
    const ContainerBox* next_box = current_box->GetContainingBlock();
    offset +=
        current_box->GetContentBoxInsetFromContainingBlockContentBox(next_box);
    current_box = next_box;
  }

  // The containing block is formed by the padding box instead of the content
  // box for absolutely positioned boxes, as described in
  // http://www.w3.org/TR/CSS21/visudet.html#containing-block-details.
  // NOTE: While not explicitly stated in the spec, which specifies that
  // the containing block of a 'fixed' position element must always be the
  // viewport, all major browsers use the padding box of a transformed ancestor
  // as the containing block for 'fixed' position elements.
  offset += InsetsLayoutUnit(containing_block->padding_left(),
                             containing_block->padding_top(),
                             containing_block->padding_right(), LayoutUnit());

  return offset;
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

    if (child_box_position == cssom::KeywordValue::GetAbsolute()) {
      UpdateRectOfAbsolutelyPositionedChildBox(child_box,
                                               absolute_child_layout_params);
    } else if (child_box_position == cssom::KeywordValue::GetFixed()) {
      UpdateRectOfAbsolutelyPositionedChildBox(child_box,
                                               relative_child_layout_params);
    } else {
      DCHECK(child_box_position == cssom::KeywordValue::GetRelative());
      UpdateOffsetOfRelativelyPositionedChildBox(child_box,
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

  base::Optional<LayoutUnit> maybe_left = GetUsedLeftIfNotAuto(
      child_box->computed_style(), child_layout_params.containing_block_size);
  base::Optional<LayoutUnit> maybe_right = GetUsedRightIfNotAuto(
      child_box->computed_style(), child_layout_params.containing_block_size);
  base::Optional<LayoutUnit> maybe_top = GetUsedTopIfNotAuto(
      child_box->computed_style(), child_layout_params.containing_block_size);
  base::Optional<LayoutUnit> maybe_bottom = GetUsedBottomIfNotAuto(
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
    if (child_layout_params.containing_block_direction ==
        kLeftToRightBaseDirection) {
      offset.set_x(*maybe_left);
    } else {
      offset.set_x(-*maybe_right);
    }
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

void ContainerBox::UpdateRectOfAbsolutelyPositionedChildBox(
    Box* child_box, const LayoutParams& child_layout_params) {
  InsetsLayoutUnit offset_from_containing_block_to_parent =
      GetOffsetFromContainingBlockToParentOfAbsolutelyPositionedBox(this,
                                                                    child_box);

  child_box->SetStaticPositionLeftFromContainingBlockToParent(
      offset_from_containing_block_to_parent.left());
  child_box->SetStaticPositionRightFromContainingBlockToParent(
      offset_from_containing_block_to_parent.right());
  child_box->SetStaticPositionTopFromContainingBlockToParent(
      offset_from_containing_block_to_parent.top());
  child_box->UpdateSize(child_layout_params);
}

namespace {

// This class handles all logic involved with calling RenderAndAnimate() on
// stacking context children., including determining their offset from the child
// container and applying overflow hidden from its containing blocks.
class RenderAndAnimateStackingContextChildrenCoordinator {
 public:
  RenderAndAnimateStackingContextChildrenCoordinator(
      ContainerBox* stacking_context, const ContainerBox* child_container,
      const Vector2dLayoutUnit& child_container_offset_from_parent_node,
      render_tree::CompositionNode::Builder* base_node_builder)
      : stacking_context_(stacking_context),
        child_container_(child_container),
        child_container_offset_from_parent_node_(
            child_container_offset_from_parent_node),
        base_node_builder_(base_node_builder) {}

  // The destructor handles generating filter nodes for any remaining entries in
  // |overflow_hidden_stack_|.
  ~RenderAndAnimateStackingContextChildrenCoordinator();

  // Applies overflow hidden from the child's containing blocks and then calls
  // RenderAndAnimate() on it.
  void RenderAndAnimateChild(
      const ContainerBox::StackingContextChildInfo& child_info);

 private:
  struct OverflowHiddenInfo {
    explicit OverflowHiddenInfo(ContainerBox* containing_block)
        : node_builder(math::Vector2dF()), containing_block(containing_block) {}

    render_tree::CompositionNode::Builder node_builder;
    ContainerBox* containing_block;
  };

  typedef std::vector<OverflowHiddenInfo> OverflowHiddenStack;

  // Updates |overflow_hidden_stack_| with the overflow hidden containing blocks
  // of the current child. Any entries in |overflow_hidden_stack_| that are no
  // longer valid are popped. All entries that remain valid are retained, so
  // that filter nodes can be shared across stacking context children.
  void ApplyOverflowHiddenForChild(
      const Box::ContainingBlocksWithOverflowHidden& overflow_hidden_to_apply);

  // Generates a filter node from the top entry in |overflow_hidden_stack_| and
  // adds it to the active node builder after the top entry is popped.
  void PopOverflowHiddenEntryFromStack();

  // Returns the node builder from the top entry in |overflow_hidden_stack_| if
  // it is not empty; otherwise, returns |base_node_builder_|.
  render_tree::CompositionNode::Builder* GetActiveNodeBuilder();

  // Returns the offset from the child container's content box to the containing
  // block's content box.
  Vector2dLayoutUnit GetOffsetFromChildContainerToContainingBlockContentBox(
      const ContainerBox* containing_block,
      const Box::RelationshipToBox
          containing_block_relationship_to_child_container) const;

  ContainerBox* stacking_context_;
  const ContainerBox* child_container_;
  const Vector2dLayoutUnit& child_container_offset_from_parent_node_;

  render_tree::CompositionNode::Builder* base_node_builder_;
  OverflowHiddenStack overflow_hidden_stack_;
};

RenderAndAnimateStackingContextChildrenCoordinator::
    ~RenderAndAnimateStackingContextChildrenCoordinator() {
  while (!overflow_hidden_stack_.empty()) {
    PopOverflowHiddenEntryFromStack();
  }
}

void RenderAndAnimateStackingContextChildrenCoordinator::RenderAndAnimateChild(
    const ContainerBox::StackingContextChildInfo& child_info) {
  ApplyOverflowHiddenForChild(child_info.overflow_hidden_to_apply);

  // Generate the offset from the child container to the child box.
  const ContainerBox* child_containing_block =
      child_info.box->GetContainingBlock();
  Vector2dLayoutUnit position_offset =
      child_container_offset_from_parent_node_ +
      GetOffsetFromChildContainerToContainingBlockContentBox(
          child_containing_block, child_info.containing_block_relationship) +
      child_info.box->GetContainingBlockOffsetFromItsContentBox(
          child_containing_block);

  child_info.box->RenderAndAnimate(GetActiveNodeBuilder(), position_offset,
                                   stacking_context_);
}

void RenderAndAnimateStackingContextChildrenCoordinator::
    ApplyOverflowHiddenForChild(const Box::ContainingBlocksWithOverflowHidden&
                                    overflow_hidden_to_apply) {
  // Walk the overflow hidden list being applied and the active overflow hidden
  // stack looking for the first index where there's a mismatch. All entries
  // prior to this are retained. This allows as many FilterNodes as possible to
  // be shared between the stacking context children.
  size_t index = 0;
  for (; index < overflow_hidden_to_apply.size() &&
         index < overflow_hidden_stack_.size();
       ++index) {
    if (overflow_hidden_to_apply[index] !=
        overflow_hidden_stack_[index].containing_block) {
      break;
    }
  }

  // Pop all entries in the active overflow hidden stack that follow the index
  // mismatch. They're no longer contained within the overflow hidden list being
  // applied.
  while (index < overflow_hidden_stack_.size()) {
    PopOverflowHiddenEntryFromStack();
  }

  // Add the new overflow hidden entries to the active stack.
  for (; index < overflow_hidden_to_apply.size(); ++index) {
    overflow_hidden_stack_.push_back(
        OverflowHiddenInfo(overflow_hidden_to_apply[index]));
  }
}

void RenderAndAnimateStackingContextChildrenCoordinator::
    PopOverflowHiddenEntryFromStack() {
  DCHECK(!overflow_hidden_stack_.empty());
  // Before popping the top of the stack, a filter node is generated from it
  // that is added to next node builder.
  OverflowHiddenInfo& overflow_hidden_info = overflow_hidden_stack_.back();

  ContainerBox* containing_block = overflow_hidden_info.containing_block;
  DCHECK(IsOverflowCropped(containing_block->computed_style()));

  // Determine the offset from the child container to this containing block's
  // border box.
  Vector2dLayoutUnit containing_block_border_offset =
      GetOffsetFromChildContainerToContainingBlockContentBox(
          containing_block, Box::kIsBoxAncestor) -
      containing_block->GetContentBoxOffsetFromBorderBox();

  // Apply the overflow hidden from this containing block to its composition
  // node; the resulting filter node is added to the next active node builder.
  scoped_refptr<render_tree::Node> filter_node =
      containing_block->RenderAndAnimateOverflow(
          new render_tree::CompositionNode(
              std::move(overflow_hidden_info.node_builder)),
          math::Vector2dF(containing_block_border_offset.x().toFloat(),
                          containing_block_border_offset.y().toFloat()));

  overflow_hidden_stack_.pop_back();
  GetActiveNodeBuilder()->AddChild(filter_node);
}

render_tree::CompositionNode::Builder*
RenderAndAnimateStackingContextChildrenCoordinator::GetActiveNodeBuilder() {
  return overflow_hidden_stack_.empty()
             ? base_node_builder_
             : &(overflow_hidden_stack_.back().node_builder);
}

Vector2dLayoutUnit RenderAndAnimateStackingContextChildrenCoordinator::
    GetOffsetFromChildContainerToContainingBlockContentBox(
        const ContainerBox* containing_block,
        const Box::RelationshipToBox
            containing_block_relationship_to_child_container) const {
  if (containing_block_relationship_to_child_container == Box::kIsBox) {
    DCHECK_EQ(child_container_, containing_block);
    return Vector2dLayoutUnit();
  }

  Vector2dLayoutUnit offset;
  const ContainerBox* current_box =
      containing_block_relationship_to_child_container == Box::kIsBoxAncestor
          ? child_container_
          : containing_block;
  const ContainerBox* end_box =
      containing_block_relationship_to_child_container == Box::kIsBoxAncestor
          ? containing_block
          : child_container_;

  // Walk up the containing blocks from |current_box| to |end_box| adding the
  // offsets from each box to its containing block.
  // NOTE: |end_box| can be skipped during this walk both when |end_box| is not
  // positioned and when a fixed position box is encountered during the walk. In
  // this case, the walk will end when a box is found that either does not have
  // a parent (meaning that it's the root box) or is transformed (it is
  // impossible for |end_box| to be a more distant ancestor than a transformed
  // box).
  while (current_box != end_box && current_box->parent() &&
         !current_box->IsTransformed()) {
    const ContainerBox* next_box = current_box->GetContainingBlock();
    offset +=
        current_box->GetContentBoxOffsetFromContainingBlockContentBox(next_box);
    current_box = next_box;
  }

  // If |current_box| does not equal |end_box|, then |end_box| was skipped
  // during the walk up the tree. Initiate a second walk up the tree from the
  // end box to the box where the first walk ended, subtracting the offsets
  // during this walk to remove the extra offsets added after passing |end_box|
  // during the first walk.
  std::swap(current_box, end_box);
  while (current_box != end_box) {
    DCHECK(current_box->parent());
    DCHECK(!current_box->IsTransformed());
    const ContainerBox* next_box = current_box->GetContainingBlock();
    offset -=
        current_box->GetContentBoxOffsetFromContainingBlockContentBox(next_box);
    current_box = next_box;
  }

  // If the containing block is an ancestor of the child container, then
  // reverse the offset now. The earlier calculations were for the containing
  // block being a descendant of the child container.
  if (containing_block_relationship_to_child_container == Box::kIsBoxAncestor) {
    offset = -offset;
  }

  return offset;
}

}  // namespace

void ContainerBox::RenderAndAnimateStackingContextChildren(
    const ZIndexSortedList& stacking_context_child_list,
    render_tree::CompositionNode::Builder* content_node_builder,
    const Vector2dLayoutUnit& offset_from_parent_node,
    ContainerBox* stacking_context) const {
  // Create a coordinator that handles all logic involved with calling
  // RenderAndAnimate() on stacking context children., including determining
  // their offset from the child container and applying overflow hidden from the
  // child's containing blocks.
  RenderAndAnimateStackingContextChildrenCoordinator coordinator(
      stacking_context, this, offset_from_parent_node, content_node_builder);

  // Render all children of the passed in list in sorted order.
  for (ZIndexSortedList::const_iterator iter =
           stacking_context_child_list.begin();
       iter != stacking_context_child_list.end(); ++iter) {
    const StackingContextChildInfo& child_info = *iter;

    // If this container box is a stacking context, then verify that this child
    // box belongs to it; otherwise, verify that the container box and the child
    // box share the same stacking context and that the child's z-index is 0.
    DCHECK(this->IsStackingContext()
               ? this == child_info.box->GetStackingContext()
               : this->GetStackingContext() ==
                         child_info.box->GetStackingContext() &&
                     child_info.z_index == 0);

    coordinator.RenderAndAnimateChild(child_info);
  }
}

size_t ContainerBox::AddToDrawOrderInThisStackingContext() {
  return next_draw_order_position_++;
}

void ContainerBox::SplitBidiLevelRuns() {
  // Only split the child boxes if the bidi level runs haven't already been
  // split.
  if (!are_bidi_levels_runs_split_) {
    are_bidi_levels_runs_split_ = true;

    for (Boxes::const_iterator child_box_iterator = child_boxes_.begin();
         child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
      Box* child_box = child_box_iterator->get();
      child_box->SplitBidiLevelRuns();
    }
  }
}

void ContainerBox::UpdateCrossReferencesOfContainerBox(
    ContainerBox* source_box, RelationshipToBox nearest_containing_block,
    RelationshipToBox nearest_absolute_containing_block,
    RelationshipToBox nearest_fixed_containing_block,
    RelationshipToBox nearest_stacking_context,
    StackingContextContainerBoxStack* stacking_context_container_box_stack) {
  // First update the source container box's cross references with this box.
  Box::UpdateCrossReferencesOfContainerBox(
      source_box, nearest_containing_block, nearest_absolute_containing_block,
      nearest_fixed_containing_block, nearest_stacking_context,
      stacking_context_container_box_stack);

  // In addition to updating the source container box's cross references with
  // this box, we also recursively update it with our children.

  // Set the nearest flags for the children. If this container box is any of the
  // specified types, then the target container box cannot be the nearest box of
  // that type for the children.

  const RelationshipToBox kNearestContainingBlockOfChildren = kIsBoxDescendant;
  RelationshipToBox nearest_absolute_containing_block_of_children =
      nearest_absolute_containing_block != kIsBoxDescendant &&
              IsContainingBlockForPositionAbsoluteElements()
          ? kIsBoxDescendant
          : nearest_absolute_containing_block;
  RelationshipToBox nearest_fixed_containing_block_of_children =
      nearest_fixed_containing_block != kIsBoxDescendant &&
              IsContainingBlockForPositionFixedElements()
          ? kIsBoxDescendant
          : nearest_fixed_containing_block;
  RelationshipToBox nearest_stacking_context_of_children =
      nearest_stacking_context != kIsBoxDescendant && IsStackingContext()
          ? kIsBoxDescendant
          : nearest_stacking_context;

  // If the source box is the stacking context for this container box, then
  // clear out this box's stacking context children. They are being set now by
  // the stacking context. Additionally, add this container to the stack, so
  // that it will be considered as a possible destination for the stacking
  // context children.
  if (nearest_stacking_context_of_children == kIsBox) {
    negative_z_index_stacking_context_children_.clear();
    non_negative_z_index_stacking_context_children_.clear();

    bool has_absolute_position =
        computed_style()->position() == cssom::KeywordValue::GetAbsolute();
    bool has_overflow_hidden = IsOverflowCropped(computed_style());

    stacking_context_container_box_stack->push_back(
        StackingContextContainerBoxInfo(
            this, IsContainingBlockForPositionAbsoluteElements(),
            has_absolute_position, has_overflow_hidden));
  }

  // Only process the children if the target container box is still the nearest
  // box of one of the types. If it is not, then it is impossible for any of the
  // children to be added to the cross references.
  if (nearest_absolute_containing_block_of_children == kIsBox ||
      nearest_fixed_containing_block_of_children == kIsBox ||
      nearest_stacking_context_of_children == kIsBox) {
    for (Boxes::const_iterator child_box_iterator = child_boxes_.begin();
         child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
      Box* child_box = child_box_iterator->get();
      child_box->UpdateCrossReferencesOfContainerBox(
          source_box, kNearestContainingBlockOfChildren,
          nearest_absolute_containing_block_of_children,
          nearest_fixed_containing_block_of_children,
          nearest_stacking_context_of_children,
          stacking_context_container_box_stack);
    }
  }

  // Pop this container from the stack if it was previously added.
  if (nearest_stacking_context_of_children == kIsBox) {
    stacking_context_container_box_stack->pop_back();
  }
}

void ContainerBox::AddBoxAndDescendantsToDrawOrderInStackingContext(
    ContainerBox* stacking_context) {
  Box::AddBoxAndDescendantsToDrawOrderInStackingContext(stacking_context);
  // If this is a stacking context, then there's no more work to do. None of
  // the stacking context children can be part of the ancestor stacking context.
  // If this is not a stacking context, then all of the stacking context
  // children are still part of the same stacking context.
  if (!IsStackingContext()) {
    // Non-stacking contexts can only have stacking context children with a
    // z-index of zero. Any child with non-zero z-index, must be added to the
    // stacking context itself.
    DCHECK(negative_z_index_stacking_context_children_.empty());
    for (ZIndexSortedList::const_iterator iter =
             non_negative_z_index_stacking_context_children_.begin();
         iter != non_negative_z_index_stacking_context_children_.end();
         ++iter) {
      DCHECK_EQ(iter->z_index, 0);
      iter->box->AddBoxAndDescendantsToDrawOrderInStackingContext(
          stacking_context);
    }
  }
}

void ContainerBox::RenderAndAnimateContent(
    render_tree::CompositionNode::Builder* content_node_builder,
    ContainerBox* stacking_context) const {
  // Update the stacking context if this is found to be a stacking context.
  ContainerBox* this_as_stacking_context = const_cast<ContainerBox*>(this);

  // Reset the draw order for this box. Even if it isn't explicitly a stacking
  // context, it'll be used as one for in-flow, non-positioned child boxes.
  this_as_stacking_context->next_draw_order_position_ = 0;

  // If this is a stacking context, then ensure that the
  // stacking context children are up to date. This will also update the
  // stacking context children for any descendant container boxes that are
  // part of this stacking context.
  if (IsStackingContext()) {
    stacking_context = this_as_stacking_context;
    stacking_context->UpdateCrossReferences();
  }

  Vector2dLayoutUnit content_box_offset(GetContentBoxOffsetFromBorderBox());

  // Render all child stacking contexts and positioned children in our stacking
  // context that have negative z-index values.
  //   https://www.w3.org/TR/CSS21/visuren.html#z-index
  RenderAndAnimateStackingContextChildren(
      negative_z_index_stacking_context_children_, content_node_builder,
      content_box_offset, stacking_context);

  // Render in-flow, non-positioned child boxes.
  //   https://www.w3.org/TR/CSS21/visuren.html#z-index
  for (Boxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = child_box_iterator->get();
    if (!child_box->IsPositioned() && !child_box->IsStackingContext()) {
      child_box->RenderAndAnimate(content_node_builder, content_box_offset,
                                  this_as_stacking_context);
    }
  }

  // Render all child stacking contexts and positioned children in our stacking
  // context that have non-negative z-index values.
  //   https://www.w3.org/TR/CSS21/visuren.html#z-index
  RenderAndAnimateStackingContextChildren(
      non_negative_z_index_stacking_context_children_, content_node_builder,
      content_box_offset, stacking_context);
}

#ifdef COBALT_BOX_DUMP_ENABLED

void ContainerBox::DumpChildrenWithIndent(std::ostream* stream,
                                          int indent) const {
  Box::DumpChildrenWithIndent(stream, indent);

  for (Boxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = child_box_iterator->get();
    child_box->DumpWithIndent(stream, indent);
  }
}

#endif  // COBALT_BOX_DUMP_ENABLED

}  // namespace layout
}  // namespace cobalt

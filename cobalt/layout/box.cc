// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/layout/box.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/integer_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/shadow_value.h"
#include "cobalt/cssom/transform_function_list_value.h"
#include "cobalt/dom/serializer.h"
#include "cobalt/layout/container_box.h"
#include "cobalt/layout/render_tree_animations.h"
#include "cobalt/layout/size_layout_unit.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/math/vector2d.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/render_tree/border.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/rect_shadow_node.h"
#include "cobalt/render_tree/rounded_corners.h"
#include "cobalt/render_tree/shadow.h"

using cobalt::render_tree::Border;
using cobalt::render_tree::Brush;
using cobalt::render_tree::CompositionNode;
using cobalt::render_tree::FilterNode;
using cobalt::render_tree::MatrixTransformNode;
using cobalt::render_tree::OpacityFilter;
using cobalt::render_tree::RectNode;
using cobalt::render_tree::RoundedCorner;
using cobalt::render_tree::RoundedCorners;
using cobalt::render_tree::ViewportFilter;
using cobalt::render_tree::animations::Animation;
using cobalt::render_tree::animations::AnimateNode;

namespace cobalt {
namespace layout {

Box::Box(const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
             css_computed_style_declaration,
         UsedStyleProvider* used_style_provider,
         LayoutStatTracker* layout_stat_tracker)
    : css_computed_style_declaration_(css_computed_style_declaration),
      used_style_provider_(used_style_provider),
      layout_stat_tracker_(layout_stat_tracker),
      parent_(NULL),
      draw_order_position_in_stacking_context_(0) {
  DCHECK(animations());
  DCHECK(used_style_provider_);

  layout_stat_tracker_->OnBoxCreated();

#ifdef _DEBUG
  margin_box_offset_from_containing_block_.SetVector(LayoutUnit(),
                                                     LayoutUnit());
  static_position_offset_from_parent_.SetVector(LayoutUnit(), LayoutUnit());
  static_position_offset_from_containing_block_to_parent_.SetVector(
      LayoutUnit(), LayoutUnit());
  margin_insets_.SetInsets(LayoutUnit(), LayoutUnit(), LayoutUnit(),
                           LayoutUnit());
  border_insets_.SetInsets(LayoutUnit(), LayoutUnit(), LayoutUnit(),
                           LayoutUnit());
  padding_insets_.SetInsets(LayoutUnit(), LayoutUnit(), LayoutUnit(),
                            LayoutUnit());
  content_size_.SetSize(LayoutUnit(), LayoutUnit());
#endif  // _DEBUG
}

Box::~Box() { layout_stat_tracker_->OnBoxDestroyed(); }

bool Box::IsPositioned() const {
  return computed_style()->position() != cssom::KeywordValue::GetStatic();
}

bool Box::IsTransformed() const {
  return computed_style()->transform() != cssom::KeywordValue::GetNone();
}

bool Box::IsAbsolutelyPositioned() const {
  return computed_style()->position() == cssom::KeywordValue::GetAbsolute() ||
         computed_style()->position() == cssom::KeywordValue::GetFixed();
}

void Box::UpdateSize(const LayoutParams& layout_params) {
  if (ValidateUpdateSizeInputs(layout_params)) {
    return;
  }

  // If this point is reached, then the size of the box is being re-calculated.
  layout_stat_tracker_->OnUpdateSize();

  UpdateBorders();
  UpdatePaddings(layout_params);
  UpdateContentSizeAndMargins(layout_params);

  // After a size update, this portion of the render tree must be updated, so
  // invalidate any cached render tree nodes.
  InvalidateRenderTreeNodesOfBoxAndAncestors();
}

bool Box::ValidateUpdateSizeInputs(const LayoutParams& params) {
  if (last_update_size_params_ && params == *last_update_size_params_) {
    return true;
  } else {
    last_update_size_params_ = params;
    return false;
  }
}

void Box::InvalidateUpdateSizeInputsOfBox() {
  last_update_size_params_ = base::nullopt;
}

void Box::InvalidateUpdateSizeInputsOfBoxAndAncestors() {
  InvalidateUpdateSizeInputsOfBox();
  if (parent_) {
    parent_->InvalidateUpdateSizeInputsOfBoxAndAncestors();
  }
}

Vector2dLayoutUnit Box::GetContainingBlockOffsetFromRoot(
    bool transform_forms_root) const {
  if (!parent_) {
    return Vector2dLayoutUnit();
  }

  const ContainerBox* containing_block = GetContainingBlock();
  return containing_block->GetContentBoxOffsetFromRoot(transform_forms_root) +
         GetContainingBlockOffsetFromItsContentBox(containing_block);
}

Vector2dLayoutUnit Box::GetContainingBlockOffsetFromItsContentBox(
    const ContainerBox* containing_block) const {
  DCHECK(containing_block == GetContainingBlock());
  // If the box is absolutely positioned, then its containing block is formed by
  // the padding box instead of the content box, as described in
  // http://www.w3.org/TR/CSS21/visudet.html#containing-block-details.
  // NOTE: While not explicitly stated in the spec, which specifies that the
  // containing block of a 'fixed' position element must always be the viewport,
  // all major browsers use the padding box of a transformed ancestor as the
  // containing block for 'fixed' position elements.
  return IsAbsolutelyPositioned()
             ? -containing_block->GetContentBoxOffsetFromPaddingBox()
             : Vector2dLayoutUnit();
}

void Box::SetStaticPositionLeftFromParent(LayoutUnit left) {
  if (left != static_position_offset_from_parent_.x()) {
    static_position_offset_from_parent_.set_x(left);
    // Invalidate the size if the static position offset changes, as the
    // positioning for absolutely positioned elements is handled within the size
    // update.
    InvalidateUpdateSizeInputsOfBox();
  }
}

void Box::SetStaticPositionLeftFromContainingBlockToParent(LayoutUnit left) {
  if (left != static_position_offset_from_containing_block_to_parent_.x()) {
    static_position_offset_from_containing_block_to_parent_.set_x(left);
    // Invalidate the size if the static position offset changes, as the
    // positioning for absolutely positioned elements is handled within the size
    // update.
    InvalidateUpdateSizeInputsOfBox();
  }
}

LayoutUnit Box::GetStaticPositionLeft() const {
  DCHECK(IsAbsolutelyPositioned());
  return static_position_offset_from_parent_.x() +
         static_position_offset_from_containing_block_to_parent_.x();
}

void Box::SetStaticPositionTopFromParent(LayoutUnit top) {
  if (top != static_position_offset_from_parent_.y()) {
    static_position_offset_from_parent_.set_y(top);
    // Invalidate the size if the static position offset changes, as the
    // positioning for absolutely positioned elements is handled within the size
    // update.
    InvalidateUpdateSizeInputsOfBox();
  }
}

void Box::SetStaticPositionTopFromContainingBlockToParent(LayoutUnit top) {
  if (top != static_position_offset_from_containing_block_to_parent_.y()) {
    static_position_offset_from_containing_block_to_parent_.set_y(top);
    // Invalidate the size if the static position offset changes, as the
    // positioning for absolutely positioned elements is handled within the size
    // update.
    InvalidateUpdateSizeInputsOfBox();
  }
}

LayoutUnit Box::GetStaticPositionTop() const {
  DCHECK(IsAbsolutelyPositioned());
  return static_position_offset_from_parent_.y() +
         static_position_offset_from_containing_block_to_parent_.y();
}

void Box::InvalidateCrossReferencesOfBoxAndAncestors() {
  if (parent_) {
    parent_->InvalidateCrossReferencesOfBoxAndAncestors();
  }
}

void Box::InvalidateRenderTreeNodesOfBoxAndAncestors() {
  cached_render_tree_node_info_ = base::nullopt;
  if (parent_) {
    parent_->InvalidateRenderTreeNodesOfBoxAndAncestors();
  }
}

LayoutUnit Box::GetMarginBoxWidth() const {
  return margin_left() + GetBorderBoxWidth() + margin_right();
}

LayoutUnit Box::GetMarginBoxHeight() const {
  return margin_top() + GetBorderBoxHeight() + margin_bottom();
}

Vector2dLayoutUnit Box::GetMarginBoxOffsetFromRoot(
    bool transform_forms_root) const {
  Vector2dLayoutUnit containing_block_offset_from_root =
      (!transform_forms_root || !IsTransformed())
          ? GetContainingBlockOffsetFromRoot(transform_forms_root)
          : Vector2dLayoutUnit();
  return containing_block_offset_from_root +
         margin_box_offset_from_containing_block();
}

LayoutUnit Box::GetMarginBoxRightEdgeOffsetFromContainingBlock() const {
  return left() + GetMarginBoxWidth();
}

LayoutUnit Box::GetMarginBoxBottomEdgeOffsetFromContainingBlock() const {
  return top() + GetMarginBoxHeight();
}

LayoutUnit Box::GetMarginBoxStartEdgeOffsetFromContainingBlock(
    BaseDirection base_direction) const {
  return base_direction == kRightToLeftBaseDirection
             ? GetMarginBoxRightEdgeOffsetFromContainingBlock()
             : left();
}

LayoutUnit Box::GetMarginBoxEndEdgeOffsetFromContainingBlock(
    BaseDirection base_direction) const {
  return base_direction == kRightToLeftBaseDirection
             ? left()
             : GetMarginBoxRightEdgeOffsetFromContainingBlock();
}

RectLayoutUnit Box::GetBorderBoxFromRoot(bool transform_forms_root) const {
  Vector2dLayoutUnit border_box_offset =
      GetBorderBoxOffsetFromRoot(transform_forms_root);
  return RectLayoutUnit(border_box_offset.x(), border_box_offset.y(),
                        GetBorderBoxWidth(), GetBorderBoxHeight());
}

RectLayoutUnit Box::GetTransformedBorderBoxFromRoot() const {
  // Initialize the box corners to the border box.
  RectLayoutUnit border_box =
      GetBorderBoxFromRoot(true /*transform_forms_root*/);
  std::vector<math::Vector2dF> box_corners;
  box_corners.push_back(
      math::Vector2dF(border_box.x().toFloat(), border_box.y().toFloat()));
  box_corners.push_back(
      math::Vector2dF((border_box.x() + border_box.width()).toFloat(),
                      border_box.y().toFloat()));
  box_corners.push_back(
      math::Vector2dF(border_box.x().toFloat(),
                      (border_box.y() + border_box.height()).toFloat()));
  box_corners.push_back(
      math::Vector2dF((border_box.x() + border_box.width()).toFloat(),
                      (border_box.y() + border_box.height()).toFloat()));

  // Update the coordinates of the 4 corners by walking up to root and removing
  // any transforms that have been applied.
  for (const Box* check_box = this; check_box != NULL;
       check_box = check_box->GetContainingBlock()) {
    check_box->ApplyTransformActionToCoordinates(kExitTransform, &box_corners);
  }

  // Generate the new box from the min and max values of all of the corners.
  math::Vector2dF& current_corner = box_corners[0];
  math::Vector2dF min_corner(current_corner);
  math::Vector2dF max_corner(current_corner);
  for (size_t i = 1; i < 4; ++i) {
    current_corner = box_corners[i];
    min_corner.SetToMin(current_corner);
    max_corner.SetToMax(current_corner);
  }

  return RectLayoutUnit(LayoutUnit(min_corner.x()), LayoutUnit(min_corner.y()),
                        LayoutUnit(max_corner.x() - min_corner.x()),
                        LayoutUnit(max_corner.y() - min_corner.y()));
}

LayoutUnit Box::GetBorderBoxWidth() const {
  return border_left_width() + GetPaddingBoxWidth() + border_right_width();
}

LayoutUnit Box::GetBorderBoxHeight() const {
  return border_top_width() + GetPaddingBoxHeight() + border_bottom_width();
}

SizeLayoutUnit Box::GetBorderBoxSize() const {
  return SizeLayoutUnit(GetBorderBoxWidth(), GetBorderBoxHeight());
}

Vector2dLayoutUnit Box::GetBorderBoxOffsetFromRoot(
    bool transform_forms_root) const {
  return GetMarginBoxOffsetFromRoot(transform_forms_root) +
         GetBorderBoxOffsetFromMarginBox();
}

Vector2dLayoutUnit Box::GetBorderBoxOffsetFromMarginBox() const {
  return Vector2dLayoutUnit(margin_left(), margin_top());
}

LayoutUnit Box::GetPaddingBoxWidth() const {
  return padding_left() + width() + padding_right();
}

LayoutUnit Box::GetPaddingBoxHeight() const {
  return padding_top() + height() + padding_bottom();
}

SizeLayoutUnit Box::GetPaddingBoxSize() const {
  return SizeLayoutUnit(GetPaddingBoxWidth(), GetPaddingBoxHeight());
}

Vector2dLayoutUnit Box::GetPaddingBoxOffsetFromRoot(
    bool transform_forms_root) const {
  return GetBorderBoxOffsetFromRoot(transform_forms_root) +
         GetPaddingBoxOffsetFromBorderBox();
}

Vector2dLayoutUnit Box::GetPaddingBoxOffsetFromBorderBox() const {
  return Vector2dLayoutUnit(border_left_width(), border_top_width());
}

Vector2dLayoutUnit Box::GetContentBoxOffsetFromRoot(
    bool transform_forms_root) const {
  return GetMarginBoxOffsetFromRoot(transform_forms_root) +
         GetContentBoxOffsetFromMarginBox();
}

Vector2dLayoutUnit Box::GetContentBoxOffsetFromMarginBox() const {
  return Vector2dLayoutUnit(GetContentBoxLeftEdgeOffsetFromMarginBox(),
                            GetContentBoxTopEdgeOffsetFromMarginBox());
}

Vector2dLayoutUnit Box::GetContentBoxOffsetFromBorderBox() const {
  return Vector2dLayoutUnit(border_left_width() + padding_left(),
                            border_top_width() + padding_top());
}

LayoutUnit Box::GetContentBoxLeftEdgeOffsetFromMarginBox() const {
  return margin_left() + border_left_width() + padding_left();
}

LayoutUnit Box::GetContentBoxTopEdgeOffsetFromMarginBox() const {
  return margin_top() + border_top_width() + padding_top();
}

Vector2dLayoutUnit Box::GetContentBoxOffsetFromContainingBlockContentBox(
    const ContainerBox* containing_block) const {
  return GetContainingBlockOffsetFromItsContentBox(containing_block) +
         GetContentBoxOffsetFromContainingBlock();
}

Vector2dLayoutUnit Box::GetContentBoxOffsetFromContainingBlock() const {
  return Vector2dLayoutUnit(GetContentBoxLeftEdgeOffsetFromContainingBlock(),
                            GetContentBoxTopEdgeOffsetFromContainingBlock());
}

LayoutUnit Box::GetContentBoxLeftEdgeOffsetFromContainingBlock() const {
  return left() + GetContentBoxLeftEdgeOffsetFromMarginBox();
}

LayoutUnit Box::GetContentBoxTopEdgeOffsetFromContainingBlock() const {
  return top() + GetContentBoxTopEdgeOffsetFromMarginBox();
}

LayoutUnit Box::GetContentBoxStartEdgeOffsetFromContainingBlock(
    BaseDirection base_direction) const {
  return base_direction == kRightToLeftBaseDirection
             ? GetContentBoxLeftEdgeOffsetFromContainingBlock() + width()
             : GetContentBoxLeftEdgeOffsetFromContainingBlock();
}

LayoutUnit Box::GetContentBoxEndEdgeOffsetFromContainingBlock(
    BaseDirection base_direction) const {
  return base_direction == kRightToLeftBaseDirection
             ? GetContentBoxLeftEdgeOffsetFromContainingBlock()
             : GetContentBoxLeftEdgeOffsetFromContainingBlock() + width();
}

Vector2dLayoutUnit Box::GetContentBoxOffsetFromPaddingBox() const {
  return Vector2dLayoutUnit(padding_left(), padding_top());
}

LayoutUnit Box::GetInlineLevelBoxHeight() const { return GetMarginBoxHeight(); }

LayoutUnit Box::GetInlineLevelTopMargin() const { return LayoutUnit(); }

void Box::TryPlaceEllipsisOrProcessPlacedEllipsis(
    BaseDirection base_direction, LayoutUnit desired_offset,
    bool* is_placement_requirement_met, bool* is_placed,
    LayoutUnit* placed_offset) {
  // Ellipsis placement should only occur in inline level boxes.
  DCHECK(GetLevel() == kInlineLevel);

  // Check for whether this box or a previous box meets the placement
  // requirement that the first character or atomic inline-level element on a
  // line must appear before the ellipsis
  // (https://www.w3.org/TR/css3-ui/#propdef-text-overflow).
  // NOTE: 'Meet' is used in this context to to indicate that either this box or
  // a previous box within the line fulfilled the placement requirement.
  // 'Fulfill' only refers to the specific box and does not take into account
  // previous boxes within the line.
  bool box_meets_placement_requirement =
      *is_placement_requirement_met ||
      DoesFulfillEllipsisPlacementRequirement();

  // If the box was already placed or meets the placement requirement and the
  // desired offset comes before the margin box's end edge, then set the flag
  // indicating that DoPlaceEllipsisOrProcessPlacedEllipsis() should be called.
  bool should_place_ellipsis_or_process_placed_ellipsis;
  if (*is_placed) {
    should_place_ellipsis_or_process_placed_ellipsis = true;
  } else if (box_meets_placement_requirement) {
    LayoutUnit end_offset =
        GetMarginBoxEndEdgeOffsetFromContainingBlock(base_direction);
    should_place_ellipsis_or_process_placed_ellipsis =
        base_direction == kRightToLeftBaseDirection
            ? desired_offset >= end_offset
            : desired_offset <= end_offset;
  } else {
    should_place_ellipsis_or_process_placed_ellipsis = false;
  }

  // If the flag is set, call DoPlaceEllipsisOrProcessPlacedEllipsis(), which
  // handles both determining the actual placement position and updating the
  // ellipsis-related box state. While the box meeting the placement requirement
  // is included in the initial check, it is not included in
  // DoPlaceEllipsisOrProcessPlacedEllipsis(), as
  // DoPlaceEllipsisOrProcessPlacedEllipsis() needs to know whether or not the
  // placement requirement was met in a previous box.
  if (should_place_ellipsis_or_process_placed_ellipsis) {
    DoPlaceEllipsisOrProcessPlacedEllipsis(base_direction, desired_offset,
                                           is_placement_requirement_met,
                                           is_placed, placed_offset);
  }

  // Update |is_placement_requirement_met| with whether or not this box met
  // the placement requirement, so that later boxes will know that they don't
  // need to fulfill it themselves.
  *is_placement_requirement_met = box_meets_placement_requirement;
}

void Box::AddBoxAndDescendantsToDrawOrderInStackingContext(
    ContainerBox* stacking_context) {
  DCHECK(stacking_context == GetStackingContext());
  draw_order_position_in_stacking_context_ =
      stacking_context->AddToDrawOrderInThisStackingContext();
}

void Box::RenderAndAnimate(
    CompositionNode::Builder* parent_content_node_builder,
    const math::Vector2dF& offset_from_parent_node,
    ContainerBox* stacking_context) {
  DCHECK(stacking_context);

  math::Vector2dF border_box_offset(left().toFloat() + margin_left().toFloat(),
                                    top().toFloat() + margin_top().toFloat());
  border_box_offset += offset_from_parent_node;

  // If there's a pre-existing cached render tree node that is located at the
  // border box offset, then simply use it. The only work that needs to be done
  // is adding the box and any ancestors that are contained within the stacking
  // context to the draw order of the stacking context.
  if (cached_render_tree_node_info_ &&
      cached_render_tree_node_info_->offset_ == border_box_offset) {
    if (cached_render_tree_node_info_->node_) {
      parent_content_node_builder->AddChild(
          cached_render_tree_node_info_->node_);
    }
    AddBoxAndDescendantsToDrawOrderInStackingContext(stacking_context);
    return;
  }

  draw_order_position_in_stacking_context_ =
      stacking_context->AddToDrawOrderInThisStackingContext();

  // If this point is reached, then the pre-existing cached render tree node is
  // not being used.
  layout_stat_tracker_->OnRenderAndAnimate();

  // Initialize the cached render tree node with the border box offset.
  cached_render_tree_node_info_ = CachedRenderTreeNodeInfo(border_box_offset);

  float opacity = base::polymorphic_downcast<const cssom::NumberValue*>(
                      computed_style()->opacity().get())
                      ->value();
  bool opacity_animated =
      animations()->IsPropertyAnimated(cssom::kOpacityProperty);
  if (opacity <= 0.0f && !opacity_animated) {
    // If the box has 0 opacity, and opacity is not animated, then we do not
    // need to proceed any farther, the box is invisible.
    return;
  }

  // If a box is hidden by an ellipsis, then it and its children are hidden:
  // Implementations must hide characters and atomic inline-level elements at
  // the applicable edge(s) of the line as necessary to fit the ellipsis.
  //   https://www.w3.org/TR/css3-ui/#propdef-text-overflow
  if (IsHiddenByEllipsis()) {
    return;
  }

  render_tree::CompositionNode::Builder border_node_builder(border_box_offset);
  AnimateNode::Builder animate_node_builder;

  const base::optional<RoundedCorners> rounded_corners =
      ComputeRoundedCorners();

  const base::optional<RoundedCorners> padding_rounded_corners =
      ComputePaddingRoundedCorners(rounded_corners);

  // The painting order is:
  // - background color.
  // - background image.
  // - border.
  //   https://www.w3.org/TR/CSS21/zindex.html
  //
  // TODO: Fully implement the stacking algorithm:
  //       https://www.w3.org/TR/CSS21/visuren.html#z-index and
  //       https://www.w3.org/TR/CSS21/zindex.html.

  // When an element has visibility:hidden, the generated box is invisible
  // (fully transparent, nothing is drawn), but still affects layout.
  // Furthermore, descendants of the element will be visible if they have
  // 'visibility: visible'.
  //   https://www.w3.org/TR/CSS21/visufx.html#propdef-visibility
  bool box_is_visible =
      computed_style()->visibility() == cssom::KeywordValue::GetVisible();
  if (box_is_visible) {
    RenderAndAnimateBackgroundImageResult background_image_result =
        RenderAndAnimateBackgroundImage(padding_rounded_corners);
    // If the background image is opaque, then it will occlude the background
    // color and so we do not need to render the background color.
    if (!background_image_result.is_opaque) {
      RenderAndAnimateBackgroundColor(
          padding_rounded_corners, &border_node_builder, &animate_node_builder);
    }
    if (background_image_result.node) {
      border_node_builder.AddChild(background_image_result.node);
    }
    RenderAndAnimateBorder(rounded_corners, &border_node_builder,
                           &animate_node_builder);
    RenderAndAnimateBoxShadow(rounded_corners, padding_rounded_corners,
                              &border_node_builder, &animate_node_builder);
  }

  const bool overflow_hidden =
      computed_style()->overflow() == cssom::KeywordValue::GetHidden();

  bool overflow_hidden_needs_to_be_applied = overflow_hidden;

  // If the outline is absent or transparent, there is no need to render it.
  bool outline_is_visible =
      box_is_visible &&
      (computed_style()->outline_style() != cssom::KeywordValue::GetNone() &&
       computed_style()->outline_style() != cssom::KeywordValue::GetHidden() &&
       (animations()->IsPropertyAnimated(cssom::kOutlineColorProperty) ||
        GetUsedColor(computed_style()->outline_color()).a() != 0.0f));

  // In order to avoid the creation of a superfluous CompositionNode, we first
  // check to see if there is a need to distinguish between content and
  // background.
  if (!overflow_hidden ||
      (!outline_is_visible &&
       computed_style()->box_shadow() == cssom::KeywordValue::GetNone() &&
       border_insets_.zero())) {
    // If there's no reason to distinguish between content and background,
    // just add them all to the same composition node.
    RenderAndAnimateContent(&border_node_builder, stacking_context);
  } else {
    CompositionNode::Builder content_node_builder;
    // Otherwise, deal with content specifically so that we can apply overflow:
    // hidden to the content but not the background.
    RenderAndAnimateContent(&content_node_builder, stacking_context);
    if (!content_node_builder.children().empty()) {
      border_node_builder.AddChild(RenderAndAnimateOverflow(
          padding_rounded_corners,
          new CompositionNode(content_node_builder.Pass()),
          &animate_node_builder, math::Vector2dF(0, 0)));
    }
    // We've already applied overflow hidden, no need to apply it again later.
    overflow_hidden_needs_to_be_applied = false;
  }

  if (outline_is_visible) {
    RenderAndAnimateOutline(&border_node_builder, &animate_node_builder);
  }

  if (!border_node_builder.children().empty()) {
    scoped_refptr<render_tree::Node> border_node =
        new CompositionNode(border_node_builder.Pass());
    if (overflow_hidden_needs_to_be_applied) {
      border_node =
          RenderAndAnimateOverflow(padding_rounded_corners, border_node,
                                   &animate_node_builder, border_box_offset);
    }
    border_node = RenderAndAnimateTransform(border_node, &animate_node_builder,
                                            border_box_offset);
    border_node = RenderAndAnimateOpacity(border_node, &animate_node_builder,
                                          opacity, opacity_animated);

    cached_render_tree_node_info_->node_ =
        animate_node_builder.empty()
            ? border_node
            : scoped_refptr<render_tree::Node>(
                  new AnimateNode(animate_node_builder, border_node));

    parent_content_node_builder->AddChild(cached_render_tree_node_info_->node_);
  }
}

Box::RenderSequence Box::GetRenderSequence() const {
  std::vector<size_t> render_sequence;
  const Box* ancestor_box = this;
  const Box* box = NULL;
  while (ancestor_box && (box != ancestor_box)) {
    box = ancestor_box;
    if (box->cached_render_tree_node_info_) {
      render_sequence.push_back(box->draw_order_position_in_stacking_context_);
      ancestor_box = box->GetStackingContext();
    }
  }
  return render_sequence;
}

bool Box::IsRenderedLater(RenderSequence render_sequence,
                          RenderSequence other_render_sequence) {
  for (size_t step = 1; step <= render_sequence.size(); ++step) {
    if (other_render_sequence.size() < step) {
      return true;
    }
    size_t idx = render_sequence.size() - step;
    size_t other_idx = other_render_sequence.size() - step;
    if (render_sequence[idx] != other_render_sequence[other_idx]) {
      return render_sequence[idx] > other_render_sequence[other_idx];
    }
  }
  return false;
}

AnonymousBlockBox* Box::AsAnonymousBlockBox() { return NULL; }
const AnonymousBlockBox* Box::AsAnonymousBlockBox() const { return NULL; }
ContainerBox* Box::AsContainerBox() { return NULL; }
const ContainerBox* Box::AsContainerBox() const { return NULL; }
TextBox* Box::AsTextBox() { return NULL; }
const TextBox* Box::AsTextBox() const { return NULL; }

#ifdef COBALT_BOX_DUMP_ENABLED

void Box::SetGeneratingNode(dom::Node* generating_node) {
  std::stringstream stream;
  dom::Serializer html_serializer(&stream);
  html_serializer.SerializeSelfOnly(generating_node);
  generating_html_ = stream.str();
}

void Box::DumpWithIndent(std::ostream* stream, int indent) const {
  if (!generating_html_.empty()) {
    DumpIndent(stream, indent);
    *stream << "# " << generating_html_ << "\n";
  }

  DumpIndent(stream, indent);
  DumpClassName(stream);
  DumpProperties(stream);
  *stream << "\n";

  static const int INDENT_SIZE = 2;
  DumpChildrenWithIndent(stream, indent + INDENT_SIZE);
}

#endif  // COBALT_BOX_DUMP_ENABLED

namespace {
void PopulateBaseStyleForBackgroundNode(
    const scoped_refptr<const cssom::CSSComputedStyleData>& source_style,
    const scoped_refptr<cssom::CSSComputedStyleData>& destination_style) {
  // NOTE: Properties set by PopulateBaseStyleForBackgroundNode() should match
  // the properties used by SetupBackgroundNodeFromStyle().
  destination_style->set_background_color(source_style->background_color());
}

void SetupBackgroundNodeFromStyle(
    const base::optional<RoundedCorners>& rounded_corners,
    const scoped_refptr<const cssom::CSSComputedStyleData>& style,
    RectNode::Builder* rect_node_builder) {
  rect_node_builder->background_brush =
      scoped_ptr<render_tree::Brush>(new render_tree::SolidColorBrush(
          GetUsedColor(style->background_color())));

  if (rounded_corners) {
    rect_node_builder->rounded_corners =
        scoped_ptr<RoundedCorners>(new RoundedCorners(*rounded_corners));
  }
}

bool IsBorderStyleNoneOrHidden(
    const scoped_refptr<cssom::PropertyValue>& border_style) {
  if (border_style == cssom::KeywordValue::GetNone() ||
      border_style == cssom::KeywordValue::GetHidden()) {
    return true;
  }
  return false;
}

render_tree::BorderStyle GetRenderTreeBorderStyle(
    const scoped_refptr<cssom::PropertyValue>& border_style) {
  render_tree::BorderStyle render_tree_border_style =
      render_tree::kBorderStyleNone;
  if (!IsBorderStyleNoneOrHidden(border_style)) {
    DCHECK_EQ(border_style, cssom::KeywordValue::GetSolid());
    render_tree_border_style = render_tree::kBorderStyleSolid;
  }

  return render_tree_border_style;
}

Border CreateBorderFromStyle(
    const scoped_refptr<const cssom::CSSComputedStyleData>& style) {
  render_tree::BorderSide left(
      GetUsedNonNegativeLength(style->border_left_width()).toFloat(),
      GetRenderTreeBorderStyle(style->border_left_style()),
      GetUsedColor(style->border_left_color()));

  render_tree::BorderSide right(
      GetUsedNonNegativeLength(style->border_right_width()).toFloat(),
      GetRenderTreeBorderStyle(style->border_right_style()),
      GetUsedColor(style->border_right_color()));

  render_tree::BorderSide top(
      GetUsedNonNegativeLength(style->border_top_width()).toFloat(),
      GetRenderTreeBorderStyle(style->border_top_style()),
      GetUsedColor(style->border_top_color()));

  render_tree::BorderSide bottom(
      GetUsedNonNegativeLength(style->border_bottom_width()).toFloat(),
      GetRenderTreeBorderStyle(style->border_bottom_style()),
      GetUsedColor(style->border_bottom_color()));

  return Border(left, right, top, bottom);
}

void PopulateBaseStyleForBorderNode(
    const scoped_refptr<const cssom::CSSComputedStyleData>& source_style,
    const scoped_refptr<cssom::CSSComputedStyleData>& destination_style) {
  // NOTE: Properties set by PopulateBaseStyleForBorderNode() should match the
  // properties used by SetupBorderNodeFromStyle().

  // Left
  destination_style->set_border_left_width(source_style->border_left_width());
  destination_style->set_border_left_style(source_style->border_left_style());
  destination_style->set_border_left_color(source_style->border_left_color());

  // Right
  destination_style->set_border_right_width(source_style->border_right_width());
  destination_style->set_border_right_style(source_style->border_right_style());
  destination_style->set_border_right_color(source_style->border_right_color());

  // Top
  destination_style->set_border_top_width(source_style->border_top_width());
  destination_style->set_border_top_style(source_style->border_top_style());
  destination_style->set_border_top_color(source_style->border_top_color());

  // Bottom
  destination_style->set_border_bottom_width(
      source_style->border_bottom_width());
  destination_style->set_border_bottom_style(
      source_style->border_bottom_style());
  destination_style->set_border_bottom_color(
      source_style->border_bottom_color());
}

void SetupBorderNodeFromStyle(
    const base::optional<RoundedCorners>& rounded_corners,
    const scoped_refptr<const cssom::CSSComputedStyleData>& style,
    RectNode::Builder* rect_node_builder) {
  rect_node_builder->border =
      scoped_ptr<Border>(new Border(CreateBorderFromStyle(style)));

  if (rounded_corners) {
    rect_node_builder->rounded_corners =
        scoped_ptr<RoundedCorners>(new RoundedCorners(*rounded_corners));
  }
}

void PopulateBaseStyleForOutlineNode(
    const scoped_refptr<const cssom::CSSComputedStyleData>& source_style,
    const scoped_refptr<cssom::CSSComputedStyleData>& destination_style) {
  // NOTE: Properties set by PopulateBaseStyleForOutlineNode() should match the
  // properties used by SetupOutlineNodeFromStyle().

  destination_style->set_outline_width(source_style->outline_width());
  destination_style->set_outline_style(source_style->outline_style());
  destination_style->set_outline_color(source_style->outline_color());
}

void SetupOutlineNodeFromStyleWithOutset(
    const math::RectF& rect,
    const scoped_refptr<const cssom::CSSComputedStyleData>& style,
    RectNode::Builder* rect_node_builder, float outset_width) {
  rect_node_builder->rect = rect;
  rect_node_builder->rect.Outset(outset_width, outset_width);
  if (outset_width != 0) {
    rect_node_builder->border =
        scoped_ptr<Border>(new Border(render_tree::BorderSide(
            outset_width, GetRenderTreeBorderStyle(style->outline_style()),
            GetUsedColor(style->outline_color()))));
  }
}

void SetupOutlineNodeFromStyle(
    const math::RectF& rect,
    const scoped_refptr<const cssom::CSSComputedStyleData>& style,
    RectNode::Builder* rect_node_builder) {
  SetupOutlineNodeFromStyleWithOutset(
      rect, style, rect_node_builder,
      GetUsedNonNegativeLength(style->outline_width()).toFloat());
}
}  // namespace

bool Box::HasNonZeroMarginOrBorderOrPadding() const {
  return width() != GetMarginBoxWidth() || height() != GetMarginBoxHeight();
}

#ifdef COBALT_BOX_DUMP_ENABLED

void Box::DumpIndent(std::ostream* stream, int indent) const {
  while (indent--) {
    *stream << " ";
  }
}

void Box::DumpProperties(std::ostream* stream) const {
  *stream << "left=" << left() << " "
          << "top=" << top() << " "
          << "width=" << width() << " "
          << "height=" << height() << " ";

  *stream << "margin=" << margin_insets_.ToString() << " ";
  *stream << "border_width=" << border_insets_.ToString() << " ";
  *stream << "padding=" << padding_insets_.ToString() << " ";

  *stream << "baseline=" << GetBaselineOffsetFromTopMarginEdge() << " ";
}

void Box::DumpChildrenWithIndent(std::ostream* /*stream*/,
                                 int /*indent*/) const {}

#endif  // COBALT_BOX_DUMP_ENABLED

const ContainerBox* Box::GetAbsoluteContainingBlock() const {
  // If the element has 'position: absolute', the containing block is
  // established by the nearest ancestor with a 'position' of 'absolute',
  // 'relative' or 'fixed'.
  if (!parent_) return AsContainerBox();
  ContainerBox* containing_block = parent_;
  while (!containing_block->IsContainingBlockForPositionAbsoluteElements()) {
    containing_block = containing_block->parent_;
  }
  return containing_block;
}

const ContainerBox* Box::GetFixedContainingBlock() const {
  // If the element has 'position: fixed', the containing block is established
  // by the viewport in the case of continuous media or the page area in the
  // case of paged media.
  // Transformed elements also act as a containing block for fixed positioned
  // descendants, as described at the bottom of this section:
  // https://www.w3.org/TR/css-transforms-1/#transform-rendering.
  if (!parent_) return AsContainerBox();
  ContainerBox* containing_block = parent_;
  while (!containing_block->IsContainingBlockForPositionFixedElements()) {
    containing_block = containing_block->parent_;
  }
  return containing_block;
}

const ContainerBox* Box::GetContainingBlock() const {
  // Establish the containing block, as described in
  // http://www.w3.org/TR/CSS21/visudet.html#containing-block-details.
  if (computed_style()->position() == cssom::KeywordValue::GetAbsolute()) {
    return GetAbsoluteContainingBlock();
  } else if (computed_style()->position() == cssom::KeywordValue::GetFixed()) {
    return GetFixedContainingBlock();
  }
  // If the element's position is "relative" or "static", the containing
  // block is formed by the content edge of the nearest block container
  // ancestor box.
  return parent_;
}

const ContainerBox* Box::GetStackingContext() const {
  if (!parent_) return AsContainerBox();

  // If the box is an in-flow, non-positioned element, then simply return the
  // parent as the stacking context.
  //   https://www.w3.org/TR/CSS21/visuren.html#z-index
  if (!IsPositioned() && !IsStackingContext()) {
    return parent_;
  }

  ContainerBox* ancestor = parent_;
  while (!ancestor->IsStackingContext()) {
    ancestor = ancestor->parent_;
  }
  return ancestor;
}

int Box::GetZIndex() const {
  if (computed_style()->z_index() == cssom::KeywordValue::GetAuto()) {
    return 0;
  } else {
    return base::polymorphic_downcast<cssom::IntegerValue*>(
               computed_style()->z_index().get())->value();
  }
}

bool Box::IsUnderCoordinate(const Vector2dLayoutUnit& coordinate) const {
  RectLayoutUnit rect = GetBorderBoxFromRoot(true /*transform_forms_root*/);
  bool res =
      coordinate.x() >= rect.x() && coordinate.x() <= rect.x() + rect.width() &&
      coordinate.y() >= rect.y() && coordinate.y() <= rect.y() + rect.height();
  return res;
}

void Box::UpdateCrossReferencesOfContainerBox(
    ContainerBox* source_box, RelationshipToBox nearest_containing_block,
    RelationshipToBox nearest_absolute_containing_block,
    RelationshipToBox nearest_fixed_containing_block,
    RelationshipToBox nearest_stacking_context,
    StackingContextContainerBoxStack* stacking_context_container_box_stack) {
  const scoped_refptr<cssom::PropertyValue>& position_property =
      computed_style()->position();
  const bool is_positioned =
      position_property != cssom::KeywordValue::GetStatic();

  RelationshipToBox my_nearest_containing_block = nearest_containing_block;

  // Establish the containing block, as described in
  // http://www.w3.org/TR/CSS21/visudet.html#containing-block-details.
  // Containing blocks only matter for descendant positioned boxes.
  if (is_positioned) {
    if (position_property == cssom::KeywordValue::GetFixed()) {
      // If the element has 'position: fixed', the containing block is
      // established by the viewport in the case of continuous media or the page
      // area in the case of paged media.
      my_nearest_containing_block = nearest_fixed_containing_block;
    } else if (position_property == cssom::KeywordValue::GetAbsolute()) {
      // If the element has 'position: absolute', the containing block is
      // established by the nearest ancestor with a 'position' of 'absolute',
      // 'relative' or 'fixed'.
      my_nearest_containing_block = nearest_absolute_containing_block;
    }
    // Otherwise, the element's position is "relative"; the containing block is
    // formed by the content edge of the nearest block container ancestor box,
    // which is the initial value of |my_nearest_containing_block|.

    if (my_nearest_containing_block == kIsBox) {
      source_box->AddContainingBlockChild(this);
    }
  }

  // Establish the stacking context, as described in
  // https://www.w3.org/TR/CSS21/visuren.html#z-index,
  // https://www.w3.org/TR/css3-color/#transparency, and
  // https://www.w3.org/TR/css3-transforms/#transform-rendering.
  // Stacking contexts only matter for descendant positioned boxes and child
  // stacking contexts.
  if (nearest_stacking_context == kIsBox &&
      (is_positioned || IsStackingContext())) {
    // Fixed position elements cannot have a containing block that is not also
    // a stacking context, so it is impossible for it to have a containing
    // block that is closer than the stacking context, although it can be
    // further away.
    DCHECK(my_nearest_containing_block != kIsBoxDescendant ||
           position_property != cssom::KeywordValue::GetFixed());

    // Default to using the stacking context itself as the nearest usable child
    // container. However, this may change if a usable container is found
    // further down in the container stack.
    ContainerBox* nearest_usable_child_container = source_box;
    RelationshipToBox containing_block_relationship_to_child_container =
        my_nearest_containing_block;
    ContainingBlocksWithOverflowHidden overflow_hidden_to_apply;

    int z_index = GetZIndex();
    // If a fixed position box is encountered that has a z-index of 0, then
    // all of the containers within the current container stack are no longer
    // usable as child containers. The reason for this is that the fixed
    // position box is being added directly to the stacking context and will
    // resultantly be drawn after all of the boxes in the current container
    // stack. Given that subsequent boxes with a z-index of 0 should be drawn
    // after this fixed position box, using any boxes within the current
    // container stack will produce an incorrect draw order.
    if (position_property == cssom::KeywordValue::GetFixed() && z_index == 0) {
      for (StackingContextContainerBoxStack::iterator iter =
               stacking_context_container_box_stack->begin();
           iter != stacking_context_container_box_stack->end(); ++iter) {
        iter->is_usable_as_child_container = false;
      }
    } else if (my_nearest_containing_block == kIsBoxDescendant) {
      bool passed_my_containing_block = false;
      bool next_containing_block_requires_absolute_containing_block =
          position_property == cssom::KeywordValue::GetAbsolute();

      // Walk up the container box stack looking for two things:
      //   1. The nearest usable child container (meaning that it guarantees the
      //      proper draw order).
      //   2. All containing blocks with overflow hidden that are passed during
      //      the walk. Because the box is being added to a child container
      //      higher in the tree than these blocks, the box's nodes will not be
      //      descendants of those containing blocks in the render tree and the
      //      overflow hidden from them will need to be applied manually.
      for (StackingContextContainerBoxStack::reverse_iterator iter =
               stacking_context_container_box_stack->rbegin();
           iter != stacking_context_container_box_stack->rend(); ++iter) {
        // Only check for a usable child container if the z_index is 0. If it
        // is not, then the stacking context must be used.
        if (z_index == 0 && iter->is_usable_as_child_container) {
          DCHECK(iter->is_absolute_containing_block);
          nearest_usable_child_container = iter->container_box;
          containing_block_relationship_to_child_container =
              passed_my_containing_block ? kIsBoxDescendant : kIsBox;
          break;
        }

        // Check for the current container box being the next containing block
        // in the walk. If it is, then this box's containing block is guaranteed
        // to have been passed during the walk (since it'll be the first
        // containing block encountered); additionally, the ancestor containing
        // block can potentially apply overflow hidden to this box.
        if (iter->is_absolute_containing_block ||
            !next_containing_block_requires_absolute_containing_block) {
          passed_my_containing_block = true;
          next_containing_block_requires_absolute_containing_block =
              iter->has_absolute_position;
          if (iter->has_overflow_hidden) {
            overflow_hidden_to_apply.push_back(iter->container_box);
          }
        }
      }

      // Reverse the containing blocks with overflow hidden, so that they'll
      // start with the ones nearest to the child container.
      std::reverse(overflow_hidden_to_apply.begin(),
                   overflow_hidden_to_apply.end());
    }

    nearest_usable_child_container->AddStackingContextChild(
        this, z_index, containing_block_relationship_to_child_container,
        overflow_hidden_to_apply);
  }
}

void Box::UpdateBorders() {
  if (IsBorderStyleNoneOrHidden(computed_style()->border_left_style()) &&
      IsBorderStyleNoneOrHidden(computed_style()->border_top_style()) &&
      IsBorderStyleNoneOrHidden(computed_style()->border_right_style()) &&
      IsBorderStyleNoneOrHidden(computed_style()->border_bottom_style())) {
    border_insets_ = InsetsLayoutUnit();
    return;
  }

  border_insets_.SetInsets(GetUsedBorderLeft(computed_style()),
                           GetUsedBorderTop(computed_style()),
                           GetUsedBorderRight(computed_style()),
                           GetUsedBorderBottom(computed_style()));
}

void Box::UpdatePaddings(const LayoutParams& layout_params) {
  padding_insets_.SetInsets(
      GetUsedPaddingLeft(computed_style(), layout_params.containing_block_size),
      GetUsedPaddingTop(computed_style(), layout_params.containing_block_size),
      GetUsedPaddingRight(computed_style(),
                          layout_params.containing_block_size),
      GetUsedPaddingBottom(computed_style(),
                           layout_params.containing_block_size));
}

namespace {

// Returns a matrix representing the transform on the object induced by its
// CSS transform style property.  If the object does not have a transform
// style property set, this will be the identity matrix.  Otherwise, it is
// calculated from the property value and returned.  The transform-origin
// style property will also be taken into account, and therefore the laid
// out size of the object is also required in order to resolve a
// percentage-based transform-origin.
math::Matrix3F GetCSSTransform(
    cssom::PropertyValue* transform_property_value,
    cssom::PropertyValue* transform_origin_property_value,
    const math::RectF& used_rect) {
  if (transform_property_value == cssom::KeywordValue::GetNone()) {
    return math::Matrix3F::Identity();
  }

  math::Matrix3F css_transform_matrix =
      GetTransformMatrix(transform_property_value).ToMatrix3F(used_rect.size());

  // Apply the CSS transformations, taking into account the CSS
  // transform-origin property.
  math::Vector2dF origin =
      GetTransformOrigin(used_rect, transform_origin_property_value);

  return math::TranslateMatrix(origin.x(), origin.y()) * css_transform_matrix *
         math::TranslateMatrix(-origin.x(), -origin.y());
}

void PopulateBaseStyleForMatrixTransformNode(
    const scoped_refptr<const cssom::CSSComputedStyleData>& source_style,
    const scoped_refptr<cssom::CSSComputedStyleData>& destination_style) {
  // NOTE: Properties set by PopulateBaseStyleForMatrixTransformNode() should
  // match the properties used by
  // SetupMatrixTransformNodeFromCSSSStyleTransform().
  destination_style->set_transform(source_style->transform());
  destination_style->set_transform_origin(source_style->transform_origin());
}

// Used within the animation callback for CSS transforms.  This will set the
// transform of a single-child matrix transform node to that specified by the
// CSS transform of the provided CSS Style Declaration.
void SetupMatrixTransformNodeFromCSSSStyleTransform(
    const math::RectF& used_rect,
    const scoped_refptr<const cssom::CSSComputedStyleData>& style,
    MatrixTransformNode::Builder* transform_node_builder) {
  transform_node_builder->transform =
      GetCSSTransform(style->transform(), style->transform_origin(), used_rect);
}

void PopulateBaseStyleForFilterNode(
    const scoped_refptr<const cssom::CSSComputedStyleData>& source_style,
    const scoped_refptr<cssom::CSSComputedStyleData>& destination_style) {
  // NOTE: Properties set by PopulateBaseStyleForFilterNode() should match the
  // properties used by SetupFilterNodeFromStyle().
  destination_style->set_opacity(source_style->opacity());
}

void SetupFilterNodeFromStyle(
    const scoped_refptr<const cssom::CSSComputedStyleData>& style,
    FilterNode::Builder* filter_node_builder) {
  float opacity = base::polymorphic_downcast<const cssom::NumberValue*>(
                      style->opacity().get())->value();

  if (opacity < 1.0f) {
    filter_node_builder->opacity_filter.emplace(std::max(0.0f, opacity));
  } else {
    // If opacity is 1, then no opacity filter should be applied, so the
    // source render tree should appear fully opaque.
    filter_node_builder->opacity_filter = base::nullopt;
  }
}

bool AreAllBordersTransparent(
    const scoped_refptr<const cssom::CSSComputedStyleData>& style) {
  return (GetUsedColor(style->border_left_color()).a() == 0.0f) &&
         (GetUsedColor(style->border_right_color()).a() == 0.0f) &&
         (GetUsedColor(style->border_top_color()).a() == 0.0f) &&
         (GetUsedColor(style->border_bottom_color()).a() == 0.0f);
}

bool HasAnimatedBorder(const web_animations::AnimationSet* animation_set) {
  return animation_set->IsPropertyAnimated(cssom::kBorderTopColorProperty) ||
         animation_set->IsPropertyAnimated(cssom::kBorderRightColorProperty) ||
         animation_set->IsPropertyAnimated(cssom::kBorderBottomColorProperty) ||
         animation_set->IsPropertyAnimated(cssom::kBorderLeftColorProperty);
}

bool HasAnimatedOutline(const web_animations::AnimationSet* animation_set) {
  return animation_set->IsPropertyAnimated(cssom::kOutlineColorProperty) ||
         animation_set->IsPropertyAnimated(cssom::kOutlineWidthProperty);
}

}  // namespace

base::optional<render_tree::RoundedCorners> Box::ComputeRoundedCorners() const {
  UsedBorderRadiusProvider border_radius_provider(GetBorderBoxSize());
  render_tree::RoundedCorner border_top_left_radius;
  render_tree::RoundedCorner border_top_right_radius;
  render_tree::RoundedCorner border_bottom_right_radius;
  render_tree::RoundedCorner border_bottom_left_radius;

  computed_style()->border_top_left_radius()->Accept(&border_radius_provider);
  if (border_radius_provider.rounded_corner()) {
    border_top_left_radius = render_tree::RoundedCorner(
        border_radius_provider.rounded_corner()->horizontal,
        border_radius_provider.rounded_corner()->vertical);
  }

  computed_style()->border_top_right_radius()->Accept(&border_radius_provider);
  if (border_radius_provider.rounded_corner()) {
    border_top_right_radius = render_tree::RoundedCorner(
        border_radius_provider.rounded_corner()->horizontal,
        border_radius_provider.rounded_corner()->vertical);
  }

  computed_style()->border_bottom_right_radius()->Accept(
      &border_radius_provider);
  if (border_radius_provider.rounded_corner()) {
    border_bottom_right_radius = render_tree::RoundedCorner(
        border_radius_provider.rounded_corner()->horizontal,
        border_radius_provider.rounded_corner()->vertical);
  }
  computed_style()->border_bottom_left_radius()->Accept(
      &border_radius_provider);
  if (border_radius_provider.rounded_corner()) {
    border_bottom_left_radius = render_tree::RoundedCorner(
        border_radius_provider.rounded_corner()->horizontal,
        border_radius_provider.rounded_corner()->vertical);
  }

  base::optional<RoundedCorners> rounded_corners;
  if (!border_top_left_radius.IsSquare() ||
      !border_top_right_radius.IsSquare() ||
      !border_bottom_right_radius.IsSquare() ||
      !border_bottom_left_radius.IsSquare()) {
    rounded_corners.emplace(border_top_left_radius, border_top_right_radius,
                            border_bottom_right_radius,
                            border_bottom_left_radius);
    rounded_corners =
        rounded_corners->Normalize(math::RectF(GetBorderBoxSize()));
  }

  return rounded_corners;
}

base::optional<render_tree::RoundedCorners> Box::ComputePaddingRoundedCorners(
    const base::optional<RoundedCorners>& rounded_corners) const {
  base::optional<RoundedCorners> padding_rounded_corners_if_different;

  if (rounded_corners && !border_insets_.zero()) {
    // If we have rounded corners and a non-zero border, then we need to
    // compute the "inner" rounded corners, as the ones specified by CSS apply
    // to the outer border edge.
    padding_rounded_corners_if_different = rounded_corners->Inset(math::InsetsF(
        border_insets_.left().toFloat(), border_insets_.top().toFloat(),
        border_insets_.right().toFloat(), border_insets_.bottom().toFloat()));
  }

  const base::optional<RoundedCorners>& padding_rounded_corners =
      padding_rounded_corners_if_different
          ? padding_rounded_corners_if_different
          : rounded_corners;
  return padding_rounded_corners;
}

void Box::RenderAndAnimateBoxShadow(
    const base::optional<RoundedCorners>& outer_rounded_corners,
    const base::optional<RoundedCorners>& inner_rounded_corners,
    CompositionNode::Builder* border_node_builder,
    AnimateNode::Builder* animate_node_builder) {
  UNREFERENCED_PARAMETER(animate_node_builder);

  if (computed_style()->box_shadow() != cssom::KeywordValue::GetNone()) {
    const cssom::PropertyListValue* box_shadow_list =
        base::polymorphic_downcast<const cssom::PropertyListValue*>(
            computed_style()->box_shadow().get());

    for (size_t i = 0; i < box_shadow_list->value().size(); ++i) {
      // According to the spec, shadows are layered front to back, so we render
      // each shadow in reverse list order.
      //   https://www.w3.org/TR/2014/CR-css3-background-20140909/#shadow-layers
      size_t shadow_index = box_shadow_list->value().size() - i - 1;
      const cssom::ShadowValue* shadow_value =
          base::polymorphic_downcast<const cssom::ShadowValue*>(
              box_shadow_list->value()[shadow_index].get());

      // Since most of a Gaussian fits within 3 standard deviations from the
      // mean, we setup here the Gaussian blur sigma to be a third of the blur
      // radius.
      float shadow_blur_sigma =
          shadow_value->blur_radius()
              ? GetUsedLength(shadow_value->blur_radius()).toFloat() / 3.0f
              : 0;

      // Setup the spread radius, defaulting it to 0 if it was never specified.
      float spread_radius =
          shadow_value->spread_radius()
              ? GetUsedLength(shadow_value->spread_radius()).toFloat()
              : 0;

      // Setup our shadow parameters.
      render_tree::Shadow shadow(
          math::Vector2dF(GetUsedLength(shadow_value->offset_x()).toFloat(),
                          GetUsedLength(shadow_value->offset_y()).toFloat()),
          shadow_blur_sigma, GetUsedColor(shadow_value->color()));

      math::SizeF shadow_rect_size =
          shadow_value->has_inset() ? GetPaddingBoxSize() : GetBorderBoxSize();

      // Inset nodes apply within the border, starting at the padding box.
      math::PointF rect_offset =
          shadow_value->has_inset()
              ? math::PointF(border_left_width().toFloat(),
                             border_top_width().toFloat())
              : math::PointF();

      render_tree::RectShadowNode::Builder shadow_builder(
          math::RectF(rect_offset, shadow_rect_size), shadow,
          shadow_value->has_inset(), spread_radius);

      if (outer_rounded_corners) {
        if (shadow_value->has_inset()) {
          shadow_builder.rounded_corners = inner_rounded_corners;
        } else {
          shadow_builder.rounded_corners = outer_rounded_corners;
        }
      }

      // Finally, create our shadow node.
      scoped_refptr<render_tree::RectShadowNode> shadow_node(
          new render_tree::RectShadowNode(shadow_builder));

      border_node_builder->AddChild(shadow_node);
    }
  }
}

namespace {
bool AllBorderSidesShareSameStyle(const Border& border) {
  return border.left.style == border.top.style &&
         border.left.style == border.right.style &&
         border.left.style == border.bottom.style;
}
}  // namespace

void Box::RenderAndAnimateBorder(
    const base::optional<RoundedCorners>& rounded_corners,
    CompositionNode::Builder* border_node_builder,
    AnimateNode::Builder* animate_node_builder) {
  bool has_animated_border = HasAnimatedBorder(animations());
  // If the border is absent or all borders are transparent, there is no need
  // to render border.
  if (border_insets_.zero() ||
      (!has_animated_border && AreAllBordersTransparent(computed_style()))) {
    return;
  }

  math::RectF rect(GetBorderBoxSize());
  RectNode::Builder rect_node_builder(rect);
  SetupBorderNodeFromStyle(rounded_corners, computed_style(),
                           &rect_node_builder);

  if (rounded_corners &&
      !AllBorderSidesShareSameStyle(*rect_node_builder.border)) {
    LOG(WARNING)
        << "Cobalt does not support rounded corners borders whose edges do not "
           "all share the same style.";
    return;
  }

  scoped_refptr<RectNode> border_node(new RectNode(rect_node_builder.Pass()));
  border_node_builder->AddChild(border_node);

  if (has_animated_border) {
    AddAnimations<RectNode>(
        base::Bind(&PopulateBaseStyleForBorderNode),
        base::Bind(&SetupBorderNodeFromStyle, rounded_corners),
        *css_computed_style_declaration(), border_node, animate_node_builder);
  }
}

void Box::RenderAndAnimateOutline(CompositionNode::Builder* border_node_builder,
                                  AnimateNode::Builder* animate_node_builder) {
  math::RectF rect(GetBorderBoxSize());
  RectNode::Builder rect_node_builder(rect);
  bool has_animated_outline = HasAnimatedOutline(animations());
  if (has_animated_outline) {
    SetupOutlineNodeFromStyleWithOutset(rect, computed_style(),
                                        &rect_node_builder, 0);
  } else {
    SetupOutlineNodeFromStyle(rect, computed_style(), &rect_node_builder);
  }

  scoped_refptr<RectNode> outline_node(new RectNode(rect_node_builder.Pass()));

  border_node_builder->AddChild(outline_node);

  if (has_animated_outline) {
    AddAnimations<RectNode>(base::Bind(&PopulateBaseStyleForOutlineNode),
                            base::Bind(&SetupOutlineNodeFromStyle, rect),
                            *css_computed_style_declaration(), outline_node,
                            animate_node_builder);
  }
}

void Box::RenderAndAnimateBackgroundColor(
    const base::optional<RoundedCorners>& rounded_corners,
    render_tree::CompositionNode::Builder* border_node_builder,
    AnimateNode::Builder* animate_node_builder) {
  // Only create the RectNode if the background color is not the initial value
  // (which we know is transparent) and not transparent.  If it's animated,
  // add it no matter what since its value may change over time to be
  // non-transparent.
  bool background_color_transparent =
      GetUsedColor(computed_style()->background_color()).a() == 0.0f;
  bool background_color_animated =
      animations()->IsPropertyAnimated(cssom::kBackgroundColorProperty);
  if (!background_color_transparent || background_color_animated) {
    RectNode::Builder rect_node_builder(
        math::RectF(math::PointF(border_left_width().toFloat(),
                                 border_top_width().toFloat()),
                    GetPaddingBoxSize()),
        scoped_ptr<Brush>());
    SetupBackgroundNodeFromStyle(rounded_corners, computed_style(),
                                 &rect_node_builder);
    if (!rect_node_builder.rect.IsEmpty()) {
      scoped_refptr<RectNode> rect_node(new RectNode(rect_node_builder.Pass()));
      border_node_builder->AddChild(rect_node);

      // TODO: Investigate if we could pass css_computed_style_declaration_
      // instead here.
      if (background_color_animated) {
        AddAnimations<RectNode>(
            base::Bind(&PopulateBaseStyleForBackgroundNode),
            base::Bind(&SetupBackgroundNodeFromStyle, rounded_corners),
            *css_computed_style_declaration(), rect_node, animate_node_builder);
      }
    }
  }
}

Box::RenderAndAnimateBackgroundImageResult Box::RenderAndAnimateBackgroundImage(
    const base::optional<RoundedCorners>& rounded_corners) {
  RenderAndAnimateBackgroundImageResult result;
  // We track a single render tree node because most of the time there will only
  // be one.  If there is more, we set |single_node| to NULL and instead
  // populate |composition|.  The code here tries to avoid using CompositionNode
  // if possible to avoid constructing an std::vector.
  scoped_refptr<render_tree::Node> single_node = NULL;
  base::optional<CompositionNode::Builder> composition;
  result.is_opaque = false;

  math::RectF image_frame(
      math::PointF(border_left_width().toFloat(), border_top_width().toFloat()),
      GetPaddingBoxSize());

  cssom::PropertyListValue* property_list =
      base::polymorphic_downcast<cssom::PropertyListValue*>(
          computed_style()->background_image().get());
  // The farthest image is added to |composition_node_builder| first.
  for (cssom::PropertyListValue::Builder::const_reverse_iterator
           image_iterator = property_list->value().rbegin();
       image_iterator != property_list->value().rend(); ++image_iterator) {
    // Skip this image if it is specified as none.
    if (*image_iterator == cssom::KeywordValue::GetNone()) {
      continue;
    }

    UsedBackgroundNodeProvider background_node_provider(
        image_frame, computed_style()->background_size(),
        computed_style()->background_position(),
        computed_style()->background_repeat(), used_style_provider_);
    (*image_iterator)->Accept(&background_node_provider);
    scoped_refptr<render_tree::Node> background_node =
        background_node_provider.background_node();

    if (background_node) {
      if (rounded_corners) {
        // Apply rounded viewport filter to the background image.
        FilterNode::Builder filter_node_builder(background_node);
        filter_node_builder.viewport_filter =
            ViewportFilter(image_frame, *rounded_corners);
        background_node = new FilterNode(filter_node_builder);
      }

      // If any of the background image layers are opaque, we set that the
      // background image is opaque.  This is used to avoid setting up the
      // background color if the background image is just going to cover it
      // anyway.
      result.is_opaque |= background_node_provider.is_opaque();

      // If this is not the first node to return, then our |single_node|
      // shortcut won't work, copy that single node into |composition| before
      // continuing.
      if (single_node) {
        composition.emplace();
        composition->AddChild(single_node);
        single_node = NULL;
      }
      if (!composition) {
        single_node = background_node;
      } else {
        composition->AddChild(background_node);
      }
    }
  }

  if (single_node) {
    result.node = single_node;
  } else if (composition) {
    result.node = new CompositionNode(composition->Pass());
  }
  return result;
}

scoped_refptr<render_tree::Node> Box::RenderAndAnimateOpacity(
    const scoped_refptr<render_tree::Node>& border_node,
    AnimateNode::Builder* animate_node_builder, float opacity,
    bool opacity_animated) {
  if (opacity < 1.0f || opacity_animated) {
    FilterNode::Builder filter_node_builder(border_node);

    if (opacity < 1.0f) {
      filter_node_builder.opacity_filter.emplace(std::max(0.0f, opacity));
    }

    scoped_refptr<FilterNode> filter_node = new FilterNode(filter_node_builder);

    if (opacity_animated) {
      // Possibly setup an animation for opacity.
      AddAnimations<FilterNode>(base::Bind(&PopulateBaseStyleForFilterNode),
                                base::Bind(&SetupFilterNodeFromStyle),
                                *css_computed_style_declaration(), filter_node,
                                animate_node_builder);
    }
    return filter_node;
  }

  return border_node;
}

scoped_refptr<render_tree::Node> Box::RenderAndAnimateOverflow(
    const scoped_refptr<render_tree::Node>& content_node,
    const math::Vector2dF& border_offset) {
  const base::optional<RoundedCorners> rounded_corners =
      ComputeRoundedCorners();

  const base::optional<RoundedCorners> padding_rounded_corners =
      ComputePaddingRoundedCorners(rounded_corners);

  return RenderAndAnimateOverflow(padding_rounded_corners, content_node, NULL,
                                  border_offset);
}

scoped_refptr<render_tree::Node> Box::RenderAndAnimateOverflow(
    const base::optional<render_tree::RoundedCorners>& rounded_corners,
    const scoped_refptr<render_tree::Node>& content_node,
    AnimateNode::Builder* /* animate_node_builder */,
    const math::Vector2dF& border_node_offset) {
  DCHECK_EQ(computed_style()->overflow(), cssom::KeywordValue::GetHidden());

  // The "overflow" property specifies whether a box is clipped to its padding
  // edge.  Use a render_tree viewport filter to implement it.
  // Note that while it is unintuitive that we clip to the padding box and
  // not the content box, this behavior is consistent with Chrome and IE.
  //   https://www.w3.org/TR/CSS21/visufx.html#overflow
  math::SizeF padding_size = GetPaddingBoxSize();
  FilterNode::Builder filter_node_builder(content_node);
  filter_node_builder.viewport_filter = ViewportFilter(
      math::RectF(border_node_offset.x() + border_left_width().toFloat(),
                  border_node_offset.y() + border_top_width().toFloat(),
                  padding_size.width(), padding_size.height()));
  if (rounded_corners) {
    filter_node_builder.viewport_filter->set_rounded_corners(*rounded_corners);
  }

  return scoped_refptr<render_tree::Node>(new FilterNode(filter_node_builder));
}

scoped_refptr<render_tree::Node> Box::RenderAndAnimateTransform(
    const scoped_refptr<render_tree::Node>& border_node,
    AnimateNode::Builder* animate_node_builder,
    const math::Vector2dF& border_node_offset) {
  if (IsTransformable() &&
      animations()->IsPropertyAnimated(cssom::kTransformProperty)) {
    // If the CSS transform is animated, we cannot flatten it into the layout
    // transform, thus we create a new matrix transform node to separate it and
    // animate that node only.
    scoped_refptr<MatrixTransformNode> css_transform_node =
        new MatrixTransformNode(border_node, math::Matrix3F::Identity());

    // Specifically animate only the matrix transform node with the CSS
    // transform.
    AddAnimations<MatrixTransformNode>(
        base::Bind(&PopulateBaseStyleForMatrixTransformNode),
        base::Bind(&SetupMatrixTransformNodeFromCSSSStyleTransform,
                   math::RectF(PointAtOffsetFromOrigin(border_node_offset),
                               GetBorderBoxSize())),
        *css_computed_style_declaration(), css_transform_node,
        animate_node_builder);

    return css_transform_node;
  }

  if (IsTransformed()) {
    math::Matrix3F matrix = GetCSSTransform(
        computed_style()->transform(), computed_style()->transform_origin(),
        math::RectF(PointAtOffsetFromOrigin(border_node_offset),
                    GetBorderBoxSize()));
    if (matrix.IsIdentity()) {
      return border_node;
    } else {
      // Combine layout transform and CSS transform.
      return new MatrixTransformNode(border_node, matrix);
    }
  }

  return border_node;
}

// Based on https://www.w3.org/TR/CSS21/visudet.html#blockwidth.
void Box::UpdateHorizontalMarginsAssumingBlockLevelInFlowBox(
    LayoutUnit containing_block_width, LayoutUnit border_box_width,
    const base::optional<LayoutUnit>& possibly_overconstrained_margin_left,
    const base::optional<LayoutUnit>& possibly_overconstrained_margin_right) {
  base::optional<LayoutUnit> maybe_margin_left =
      possibly_overconstrained_margin_left;
  base::optional<LayoutUnit> maybe_margin_right =
      possibly_overconstrained_margin_right;

  // If "border-left-width" + "padding-left" + "width" + "padding-right" +
  // "border-right-width" (plus any of "margin-left" or "margin-right" that are
  // not "auto") is larger than the width of the containing block, then any
  // "auto" values for "margin-left" or "margin-right" are, for the following
  // rules, treated as zero.
  if (maybe_margin_left.value_or(LayoutUnit()) + border_box_width +
          maybe_margin_right.value_or(LayoutUnit()) >
      containing_block_width) {
    maybe_margin_left = maybe_margin_left.value_or(LayoutUnit());
    maybe_margin_right = maybe_margin_right.value_or(LayoutUnit());
  }

  if (maybe_margin_left) {
    // If all of the above have a computed value other than "auto", the values
    // are said to be "over-constrained" and the specified value of
    // "margin-right" is ignored and the value is calculated so as to make
    // the equality true.
    //
    // If there is exactly one value specified as "auto", its used value
    // follows from the equality.
    set_margin_left(*maybe_margin_left);
    set_margin_right(containing_block_width - *maybe_margin_left -
                     border_box_width);
  } else if (maybe_margin_right) {
    // If there is exactly one value specified as "auto", its used value
    // follows from the equality.
    set_margin_left(containing_block_width - border_box_width -
                    *maybe_margin_right);
    set_margin_right(*maybe_margin_right);
  } else {
    // If both "margin-left" and "margin-right" are "auto", their used values
    // are equal.
    LayoutUnit horizontal_margin =
        (containing_block_width - border_box_width) / 2;
    set_margin_left(horizontal_margin);
    set_margin_right(horizontal_margin);
  }
}

bool Box::ApplyTransformActionToCoordinate(TransformAction action,
                                           math::Vector2dF* coordinate) const {
  std::vector<math::Vector2dF> coordinate_vector;
  coordinate_vector.push_back(*coordinate);
  bool result = ApplyTransformActionToCoordinates(action, &coordinate_vector);
  *coordinate = coordinate_vector[0];
  return result;
}

bool Box::ApplyTransformActionToCoordinates(
    TransformAction action, std::vector<math::Vector2dF>* coordinates) const {
  const scoped_refptr<cssom::PropertyValue>& transform =
      computed_style()->transform();
  if (transform == cssom::KeywordValue::GetNone()) {
    return true;
  }

  // The border box offset is calculated in two steps because we want to
  // stop at the second transform and not the first (which is this box).
  Vector2dLayoutUnit containing_block_offset_from_root =
      GetContainingBlockOffsetFromRoot(true /*transform_forms_root*/);

  // The transform rect always includes the offset from the containing block.
  // However, in the case where the action is entering the transform, the full
  // offset from the root needs to be included in the transform.
  Vector2dLayoutUnit transform_rect_offset =
      margin_box_offset_from_containing_block() +
      GetBorderBoxOffsetFromMarginBox();
  if (action == kEnterTransform) {
    transform_rect_offset += containing_block_offset_from_root;
  }

  // Transform the coordinates.
  math::Matrix3F matrix =
      GetCSSTransform(transform, computed_style()->transform_origin(),
                      math::RectF(transform_rect_offset.x().toFloat(),
                                  transform_rect_offset.y().toFloat(),
                                  GetBorderBoxWidth().toFloat(),
                                  GetBorderBoxHeight().toFloat()));
  if (!matrix.IsIdentity()) {
    if (action == kEnterTransform) {
      matrix = matrix.Inverse();
      // The matrix is not invertible. Return that applying the transform
      // failed.
      if (matrix.IsZeros()) {
        return false;
      }
    }

    for (std::vector<math::Vector2dF>::iterator coordinate_iter =
             coordinates->begin();
         coordinate_iter != coordinates->end(); ++coordinate_iter) {
      math::Vector2dF& coordinate = *coordinate_iter;
      math::PointF transformed_point =
          matrix * math::PointF(coordinate.x(), coordinate.y());
      coordinate.set_x(transformed_point.x());
      coordinate.set_y(transformed_point.y());
    }
  }

  // The transformed box forms a new coordinate system and its containing
  // block's offset is the origin within it. Update the coordinates for their
  // new origin.
  math::Vector2dF containing_block_offset_from_root_as_float(
      containing_block_offset_from_root.x().toFloat(),
      containing_block_offset_from_root.y().toFloat());
  for (std::vector<math::Vector2dF>::iterator coordinate_iter =
           coordinates->begin();
       coordinate_iter != coordinates->end(); ++coordinate_iter) {
    math::Vector2dF& coordinate = *coordinate_iter;
    if (action == kEnterTransform) {
      coordinate -= containing_block_offset_from_root_as_float;
    } else {
      coordinate += containing_block_offset_from_root_as_float;
    }
  }
  return true;
}

}  // namespace layout
}  // namespace cobalt

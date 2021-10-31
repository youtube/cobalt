// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/layout/flex_item.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/layout/block_container_box.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/container_box.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

// Class for Flex Items in a container with a horizontal main axis.
// Since Cobalt does not support vertical writing modes, this is used for row
// flex containers.
class MainAxisHorizontalFlexItem : public FlexItem {
 public:
  explicit MainAxisHorizontalFlexItem(Box* box) : FlexItem(box, true) {}
  ~MainAxisHorizontalFlexItem() override {}

  LayoutUnit GetContentToMarginMainAxis() const override;
  LayoutUnit GetContentToMarginCrossAxis() const override;
  base::Optional<LayoutUnit> GetUsedMainAxisSizeIfNotAuto(
      const SizeLayoutUnit& containing_block_size) const override;
  base::Optional<LayoutUnit> GetUsedMinMainAxisSizeIfNotAuto(
      const SizeLayoutUnit& containing_block_size) const override;
  base::Optional<LayoutUnit> GetUsedMinCrossAxisSizeIfNotAuto(
      const SizeLayoutUnit& containing_block_size) const override;
  base::Optional<LayoutUnit> GetUsedMaxMainAxisSizeIfNotNone(
      const SizeLayoutUnit& containing_block_size) const override;
  base::Optional<LayoutUnit> GetUsedMaxCrossAxisSizeIfNotNone(
      const SizeLayoutUnit& containing_block_size) const override;

  void DetermineHypotheticalCrossSize(
      const LayoutParams& layout_params) override;

  LayoutUnit GetContentBoxMainSize() const override;
  LayoutUnit GetMarginBoxMainSize() const override;
  LayoutUnit GetMarginBoxCrossSize() const override;

  bool CrossSizeIsAuto() const override;
  bool MarginMainStartIsAuto() const override;
  bool MarginMainEndIsAuto() const override;
  bool MarginCrossStartIsAuto() const override;
  bool MarginCrossEndIsAuto() const override;

  void SetCrossSize(LayoutUnit cross_size) override;
  void SetMainAxisStart(LayoutUnit position) override;
  void SetCrossAxisStart(LayoutUnit position) override;
};

LayoutUnit MainAxisHorizontalFlexItem::GetContentToMarginMainAxis() const {
  return box()->GetContentToMarginHorizontal();
}

LayoutUnit MainAxisHorizontalFlexItem::GetContentToMarginCrossAxis() const {
  return box()->GetContentToMarginVertical();
}

base::Optional<LayoutUnit>
MainAxisHorizontalFlexItem::GetUsedMainAxisSizeIfNotAuto(
    const SizeLayoutUnit& containing_block_size) const {
  return GetUsedWidthIfNotAuto(computed_style(), containing_block_size, NULL);
}

base::Optional<LayoutUnit>
MainAxisHorizontalFlexItem::GetUsedMinMainAxisSizeIfNotAuto(
    const SizeLayoutUnit& containing_block_size) const {
  base::Optional<LayoutUnit> maybe_used_min_space =
      GetUsedMinWidthIfNotAuto(computed_style(), containing_block_size, NULL);
  return maybe_used_min_space;
}

base::Optional<LayoutUnit>
MainAxisHorizontalFlexItem::GetUsedMinCrossAxisSizeIfNotAuto(
    const SizeLayoutUnit& containing_block_size) const {
  return GetUsedMinHeightIfNotAuto(computed_style(), containing_block_size);
}

base::Optional<LayoutUnit>
MainAxisHorizontalFlexItem::GetUsedMaxMainAxisSizeIfNotNone(
    const SizeLayoutUnit& containing_block_size) const {
  return GetUsedMaxWidthIfNotNone(computed_style(), containing_block_size,
                                  NULL);
}

base::Optional<LayoutUnit>
MainAxisHorizontalFlexItem::GetUsedMaxCrossAxisSizeIfNotNone(
    const SizeLayoutUnit& containing_block_size) const {
  return GetUsedMaxHeightIfNotNone(computed_style(), containing_block_size);
}

void MainAxisHorizontalFlexItem::DetermineHypotheticalCrossSize(
    const LayoutParams& layout_params) {
  // 5. Set each item's used main size to its target main size.
  //   https://www.w3.org/TR/css-flexbox-1/#resolve-flexible-lengths
  // Also, algorithm for Flex Layout continued from step 7:
  // Cross Size Determination:
  // 7. Determine the hypothetical cross size of each item
  // By performing layout with the used main size and the available space.
  //   https://www.w3.org/TR/css-flexbox-1/#algo-cross-item
  LayoutParams child_layout_params(layout_params);
  child_layout_params.shrink_to_fit_width_forced = false;
  child_layout_params.freeze_width = true;
  box()->set_width(target_main_size());
  box()->UpdateSize(child_layout_params);
}

LayoutUnit MainAxisHorizontalFlexItem::GetContentBoxMainSize() const {
  return box()->width();
}

LayoutUnit MainAxisHorizontalFlexItem::GetMarginBoxMainSize() const {
  return box()->GetMarginBoxWidth();
}

LayoutUnit MainAxisHorizontalFlexItem::GetMarginBoxCrossSize() const {
  return box()->GetMarginBoxHeight();
}

bool MainAxisHorizontalFlexItem::CrossSizeIsAuto() const {
  return computed_style()->height() == cssom::KeywordValue::GetAuto();
}

bool MainAxisHorizontalFlexItem::MarginMainStartIsAuto() const {
  return computed_style()->margin_left() == cssom::KeywordValue::GetAuto();
}

bool MainAxisHorizontalFlexItem::MarginMainEndIsAuto() const {
  return computed_style()->margin_right() == cssom::KeywordValue::GetAuto();
}

bool MainAxisHorizontalFlexItem::MarginCrossStartIsAuto() const {
  return computed_style()->margin_top() == cssom::KeywordValue::GetAuto();
}

bool MainAxisHorizontalFlexItem::MarginCrossEndIsAuto() const {
  return computed_style()->margin_bottom() == cssom::KeywordValue::GetAuto();
}

void MainAxisHorizontalFlexItem::SetCrossSize(LayoutUnit cross_size) {
  box()->set_height(cross_size);
}

void MainAxisHorizontalFlexItem::SetMainAxisStart(LayoutUnit position) {
  box()->set_left(position);
}

void MainAxisHorizontalFlexItem::SetCrossAxisStart(LayoutUnit position) {
  box()->set_top(position);
}

// Class for Flex Items in a container with a vertical main axis.
// Since Cobalt does not support vertical writing modes, this is used for column
// flex containers.
class MainAxisVerticalFlexItem : public FlexItem {
 public:
  explicit MainAxisVerticalFlexItem(Box* box) : FlexItem(box, false) {}
  ~MainAxisVerticalFlexItem() override {}

  LayoutUnit GetContentToMarginMainAxis() const override;
  LayoutUnit GetContentToMarginCrossAxis() const override;

  base::Optional<LayoutUnit> GetUsedMainAxisSizeIfNotAuto(
      const SizeLayoutUnit& containing_block_size) const override;
  base::Optional<LayoutUnit> GetUsedMinMainAxisSizeIfNotAuto(
      const SizeLayoutUnit& containing_block_size) const override;
  base::Optional<LayoutUnit> GetUsedMinCrossAxisSizeIfNotAuto(
      const SizeLayoutUnit& containing_block_size) const override;
  base::Optional<LayoutUnit> GetUsedMaxMainAxisSizeIfNotNone(
      const SizeLayoutUnit& containing_block_size) const override;
  base::Optional<LayoutUnit> GetUsedMaxCrossAxisSizeIfNotNone(
      const SizeLayoutUnit& containing_block_size) const override;

  void DetermineHypotheticalCrossSize(
      const LayoutParams& layout_params) override;

  LayoutUnit GetContentBoxMainSize() const override;
  LayoutUnit GetMarginBoxMainSize() const override;
  LayoutUnit GetMarginBoxCrossSize() const override;

  bool CrossSizeIsAuto() const override;
  bool MarginMainStartIsAuto() const override;
  bool MarginMainEndIsAuto() const override;
  bool MarginCrossStartIsAuto() const override;
  bool MarginCrossEndIsAuto() const override;

  void SetCrossSize(LayoutUnit cross_size) override;
  void SetMainAxisStart(LayoutUnit position) override;
  void SetCrossAxisStart(LayoutUnit position) override;
};

LayoutUnit MainAxisVerticalFlexItem::GetContentToMarginMainAxis() const {
  return box()->GetContentToMarginVertical();
}

LayoutUnit MainAxisVerticalFlexItem::GetContentToMarginCrossAxis() const {
  return box()->GetContentToMarginHorizontal();
}

base::Optional<LayoutUnit>
MainAxisVerticalFlexItem::GetUsedMainAxisSizeIfNotAuto(
    const SizeLayoutUnit& containing_block_size) const {
  return GetUsedHeightIfNotAuto(computed_style(), containing_block_size, NULL);
}

base::Optional<LayoutUnit>
MainAxisVerticalFlexItem::GetUsedMinMainAxisSizeIfNotAuto(
    const SizeLayoutUnit& containing_block_size) const {
  return GetUsedMinHeightIfNotAuto(computed_style(), containing_block_size);
}

base::Optional<LayoutUnit>
MainAxisVerticalFlexItem::GetUsedMinCrossAxisSizeIfNotAuto(
    const SizeLayoutUnit& containing_block_size) const {
  return GetUsedMinWidthIfNotAuto(computed_style(), containing_block_size,
                                  NULL);
}

base::Optional<LayoutUnit>
MainAxisVerticalFlexItem::GetUsedMaxMainAxisSizeIfNotNone(
    const SizeLayoutUnit& containing_block_size) const {
  return GetUsedMaxHeightIfNotNone(computed_style(), containing_block_size);
}

base::Optional<LayoutUnit>
MainAxisVerticalFlexItem::GetUsedMaxCrossAxisSizeIfNotNone(
    const SizeLayoutUnit& containing_block_size) const {
  return GetUsedMaxWidthIfNotNone(computed_style(), containing_block_size,
                                  NULL);
}

void MainAxisVerticalFlexItem::DetermineHypotheticalCrossSize(
    const LayoutParams& layout_params) {
  // 7. Determine the hypothetical cross size of each item
  // By performing layout with the used main size and the available space.
  //   https://www.w3.org/TR/css-flexbox-1/#algo-cross-item
  LayoutParams child_layout_params(layout_params);
  child_layout_params.shrink_to_fit_width_forced = true;
  box()->UpdateSize(child_layout_params);
  box()->set_height(target_main_size());
}

LayoutUnit MainAxisVerticalFlexItem::GetContentBoxMainSize() const {
  return box()->height();
}

LayoutUnit MainAxisVerticalFlexItem::GetMarginBoxMainSize() const {
  return box()->GetMarginBoxHeight();
}

LayoutUnit MainAxisVerticalFlexItem::GetMarginBoxCrossSize() const {
  return box()->GetMarginBoxWidth();
}

bool MainAxisVerticalFlexItem::CrossSizeIsAuto() const {
  return computed_style()->width() == cssom::KeywordValue::GetAuto();
}

bool MainAxisVerticalFlexItem::MarginMainStartIsAuto() const {
  return computed_style()->margin_top() == cssom::KeywordValue::GetAuto();
}

bool MainAxisVerticalFlexItem::MarginMainEndIsAuto() const {
  return computed_style()->margin_bottom() == cssom::KeywordValue::GetAuto();
}

bool MainAxisVerticalFlexItem::MarginCrossStartIsAuto() const {
  return computed_style()->margin_left() == cssom::KeywordValue::GetAuto();
}

bool MainAxisVerticalFlexItem::MarginCrossEndIsAuto() const {
  return computed_style()->margin_right() == cssom::KeywordValue::GetAuto();
}

void MainAxisVerticalFlexItem::SetCrossSize(LayoutUnit cross_size) {
  box()->set_width(cross_size);
}

void MainAxisVerticalFlexItem::SetMainAxisStart(LayoutUnit position) {
  box()->set_top(position);
}

void MainAxisVerticalFlexItem::SetCrossAxisStart(LayoutUnit position) {
  box()->set_left(position);
}

FlexItem::FlexItem(Box* box, bool main_direction_is_horizontal)
    : box_(box), main_direction_is_horizontal_(main_direction_is_horizontal) {}

std::unique_ptr<FlexItem> FlexItem::Create(Box* box,
                                           bool main_direction_is_horizontal) {
  if (main_direction_is_horizontal) {
    return base::WrapUnique(new MainAxisHorizontalFlexItem(box));
  } else {
    return base::WrapUnique(new MainAxisVerticalFlexItem(box));
  }
}

void FlexItem::DetermineFlexFactor(bool flex_factor_is_grow) {
  auto flex_factor_property = flex_factor_is_grow
                                  ? box_->computed_style()->flex_grow()
                                  : box_->computed_style()->flex_shrink();
  flex_factor_ = base::polymorphic_downcast<const cssom::NumberValue*>(
                     flex_factor_property.get())
                     ->value();
}

const scoped_refptr<cobalt::cssom::PropertyValue>&
FlexItem::GetUsedAlignSelfPropertyValue() {
  DCHECK(box()->parent());
  return GetUsedAlignSelf(computed_style(), box()->parent()->computed_style());
}

const scoped_refptr<cobalt::cssom::PropertyValue>&
FlexItem::GetUsedJustifyContentPropertyValue() {
  DCHECK(box()->parent());
  return box()->parent()->computed_style()->justify_content();
}

bool FlexItem::OverflowIsVisible() const {
  return computed_style()->overflow() == cssom::KeywordValue::GetVisible();
}

void FlexItem::DetermineFlexBaseSize(
    const base::Optional<LayoutUnit>& main_space,
    bool container_shrink_to_fit_width_forced) {
  // Absolutely positioned boxes are not flex items.
  DCHECK(!box()->IsAbsolutelyPositioned());

  // All flex items are block container boxes.
  DCHECK(box()->AsBlockContainerBox());

  // Algorithm for determine the flex base size and hypothetical main size of
  // each item.
  //   https://www.w3.org/TR/css-flexbox-1/#algo-main-item

  // A. If the item has a definite used flex basis, that's the flex base size.
  bool flex_basis_depends_on_available_space;
  base::Optional<LayoutUnit> flex_basis = GetUsedFlexBasisIfNotContent(
      computed_style(), main_direction_is_horizontal_,
      main_space.value_or(LayoutUnit()),
      &flex_basis_depends_on_available_space);
  bool flex_basis_is_definite =
      flex_basis && (!flex_basis_depends_on_available_space || main_space);
  if (flex_basis_is_definite) {
    flex_base_size_ = *flex_basis;
    return;
  }

  // B. If the flex item has an intrinsic aspect ratio, a used flex basis of
  //    content, and a definite cross size, then the flex base size is
  //    calculated from its inner cross size and the flex item's intrinsic
  //    aspect ratio.
  // Sizing from intrinsic ratio is not supported.

  bool flex_basis_is_content_or_depends_on_available_space =
      !flex_basis || flex_basis_depends_on_available_space;
  // C. If the used flex basis is content or depends on its available space, and
  //    the flex container is being sized under a min-content or max-content
  //    constraint, size the item under that constraint. The flex base size is
  //    the item's resulting main size.
  if (flex_basis_is_content_or_depends_on_available_space &&
      container_shrink_to_fit_width_forced) {
    flex_base_size_ =
        main_direction_is_horizontal_ ? box()->width() : box()->height();
    return;
  }

  // D. Otherwise, if the used flex basis is content or depends on its available
  //    space, the available main size is infinite, and the flex item's inline
  //    axis is parallel to the main axis, lay the item out using the rules for
  //    a box in an orthogonal flow. The flex base size is the item's
  //    max-content main size.
  if (flex_basis_is_content_or_depends_on_available_space &&
      main_direction_is_horizontal_ && !main_space) {
    flex_base_size_ = box()->width();
    return;
  }

  // E. Otherwise, size the item into the available space using its used flex
  //    basis in place of its main size, treating a value of content as
  //    max-content. If a cross size is needed to determine the main size (e.g.
  //    when the flex item's main size is in its block axis) and the flex item's
  //    cross size is auto and not definite, in this calculation use fit-content
  //    as the flex item's cross size. The flex base size is the item's
  //    resulting main size.
  // TODO: handle 'if (!main_direction_is_horizontal)' and auto
  // height, see above.
  flex_base_size_ =
      main_direction_is_horizontal_ ? box()->width() : box()->height();
}

void FlexItem::DetermineHypotheticalMainSize(
    const SizeLayoutUnit& available_space) {
  // The hypothetical main size is the item's flex base size clamped
  // according to its used min and max main sizes.
  base::Optional<LayoutUnit> maybe_min_main_size =
      GetUsedMinMainAxisSizeIfNotAuto(available_space);
  LayoutUnit main_size =
      std::max(maybe_min_main_size.value_or(LayoutUnit()), flex_base_size());
  base::Optional<LayoutUnit> maybe_max_main_size =
      GetUsedMaxMainAxisSizeIfNotNone(available_space);
  if (maybe_max_main_size) {
    main_size = std::min(*maybe_max_main_size, main_size);
  }
  hypothetical_main_size_ = main_size;
}

base::Optional<LayoutUnit> FlexItem::GetContentBasedMinimumSize(
    const SizeLayoutUnit& containing_block_size) const {
  // Automatic Minimum Size of Flex Items.
  //   https://www.w3.org/TR/css-flexbox-1/#min-size-auto

  // If the item's computed main size property is definite, then the specified
  // size suggestion is that size (clamped by its max main size property if
  // it's definite). It is otherwise undefined.
  //   https://www.w3.org/TR/css-flexbox-1/#specified-size-suggestion
  base::Optional<LayoutUnit> specified_size_suggestion =
      GetUsedMainAxisSizeIfNotAuto(containing_block_size);
  base::Optional<LayoutUnit> maybe_max_main_size =
      GetUsedMaxMainAxisSizeIfNotNone(containing_block_size);
  if (maybe_max_main_size.has_value() &&
      specified_size_suggestion.has_value()) {
    specified_size_suggestion =
        std::min(*maybe_max_main_size, *specified_size_suggestion);
  }

  // The content size suggestion is the min-content size in the main axis,
  // clamped by the max main size property if that is definite.
  //   https://www.w3.org/TR/css-flexbox-1/#content-size-suggestion
  base::Optional<LayoutUnit> content_size_suggestion;
  if (OverflowIsVisible()) {
    if (main_direction_is_horizontal_) {
      base::Optional<LayoutUnit> maybe_height =
          GetUsedHeightIfNotAuto(computed_style(), containing_block_size, NULL);
      content_size_suggestion =
          box()->AsBlockContainerBox()->GetShrinkToFitWidth(
              containing_block_size.width(), maybe_height);
    } else {
      content_size_suggestion = GetContentBoxMainSize();
    }
    if (maybe_max_main_size.has_value()) {
      content_size_suggestion =
          std::min(*maybe_max_main_size, *content_size_suggestion);
    }
  }

  // If the box has neither a specified size suggestion nor an aspect ratio,
  // its content-based minimum size is the content size suggestion.
  base::Optional<LayoutUnit> content_based_minimum_size =
      content_size_suggestion;
  // In general, the content-based minimum size of a flex item is the smaller
  // of its content size suggestion and its specified size suggestion.
  if (content_size_suggestion.has_value() &&
      specified_size_suggestion.has_value()) {
    content_based_minimum_size =
        std::min(*content_size_suggestion, *specified_size_suggestion);
  }

  base::Optional<LayoutUnit> specified_min_size_suggestion =
      GetUsedMinMainAxisSizeIfNotAuto(containing_block_size);
  if (specified_min_size_suggestion.has_value()) {
    content_based_minimum_size = specified_min_size_suggestion;
  }

  return content_based_minimum_size;
}

}  // namespace layout
}  // namespace cobalt

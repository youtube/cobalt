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

#include "base/memory/ptr_util.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/layout/container_box.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

// Class for Flex Items in a container with a horizontal main axis.
// Since Cobalt does not support vertical writing modes, this is used for row
// flex containers.
class MainAxisHorizontalFlexItem : public FlexItem {
 public:
  MainAxisHorizontalFlexItem(Box* box, LayoutUnit flex_base_size,
                             LayoutUnit hypothetical_main_size)
      : FlexItem(box, flex_base_size, hypothetical_main_size) {}

  LayoutUnit GetContentToMarginMainAxis() override;
  LayoutUnit GetContentToMarginCrossAxis() override;
  LayoutUnit GetUsedMinMainAxisSize(
      const SizeLayoutUnit& containing_block_size) override;
  LayoutUnit GetUsedMinCrossAxisSize(
      const SizeLayoutUnit& containing_block_size) override;
  base::Optional<LayoutUnit> GetUsedMaxMainAxisSizeIfNotNone(
      const SizeLayoutUnit& containing_block_size) override;
  base::Optional<LayoutUnit> GetUsedMaxCrossAxisSizeIfNotNone(
      const SizeLayoutUnit& containing_block_size) override;

  void DetermineHypotheticalCrossSize(
      const LayoutParams& layout_params) override;
  LayoutUnit GetMarginBoxMainSize() override;
  LayoutUnit GetMarginBoxCrossSize() override;

  bool CrossSizeIsAuto() override;
  bool MarginMainStartIsAuto() override;
  bool MarginMainEndIsAuto() override;
  bool MarginCrossStartIsAuto() override;
  bool MarginCrossEndIsAuto() override;

  void SetCrossSize(LayoutUnit cross_size) override;
  void SetMainAxisStart(LayoutUnit position) override;
  void SetCrossAxisStart(LayoutUnit position) override;
};

LayoutUnit MainAxisHorizontalFlexItem::GetContentToMarginMainAxis() {
  return box()->GetContentToMarginHorizontal();
}

LayoutUnit MainAxisHorizontalFlexItem::GetContentToMarginCrossAxis() {
  return box()->GetContentToMarginVertical();
}

LayoutUnit MainAxisHorizontalFlexItem::GetUsedMinMainAxisSize(
    const SizeLayoutUnit& containing_block_size) {
  return GetUsedMinWidth(box()->computed_style(), containing_block_size, NULL);
}

LayoutUnit MainAxisHorizontalFlexItem::GetUsedMinCrossAxisSize(
    const SizeLayoutUnit& containing_block_size) {
  return GetUsedMinHeight(box()->computed_style(), containing_block_size);
}

base::Optional<LayoutUnit>
MainAxisHorizontalFlexItem::GetUsedMaxMainAxisSizeIfNotNone(
    const SizeLayoutUnit& containing_block_size) {
  return GetUsedMaxWidthIfNotNone(box()->computed_style(),
                                  containing_block_size, NULL);
}

base::Optional<LayoutUnit>
MainAxisHorizontalFlexItem::GetUsedMaxCrossAxisSizeIfNotNone(
    const SizeLayoutUnit& containing_block_size) {
  return GetUsedMaxHeightIfNotNone(box()->computed_style(),
                                   containing_block_size);
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

LayoutUnit MainAxisHorizontalFlexItem::GetMarginBoxMainSize() {
  return box()->GetMarginBoxWidth();
}

LayoutUnit MainAxisHorizontalFlexItem::GetMarginBoxCrossSize() {
  return box()->GetMarginBoxHeight();
}

bool MainAxisHorizontalFlexItem::CrossSizeIsAuto() {
  return box()->computed_style()->height() == cssom::KeywordValue::GetAuto();
}

bool MainAxisHorizontalFlexItem::MarginMainStartIsAuto() {
  return box()->computed_style()->margin_left() ==
         cssom::KeywordValue::GetAuto();
}

bool MainAxisHorizontalFlexItem::MarginMainEndIsAuto() {
  return box()->computed_style()->margin_right() ==
         cssom::KeywordValue::GetAuto();
}

bool MainAxisHorizontalFlexItem::MarginCrossStartIsAuto() {
  return box()->computed_style()->margin_top() ==
         cssom::KeywordValue::GetAuto();
}

bool MainAxisHorizontalFlexItem::MarginCrossEndIsAuto() {
  return box()->computed_style()->margin_bottom() ==
         cssom::KeywordValue::GetAuto();
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
  MainAxisVerticalFlexItem(Box* box, LayoutUnit flex_base_size,
                           LayoutUnit hypothetical_main_size)
      : FlexItem(box, flex_base_size, hypothetical_main_size) {}

  LayoutUnit GetContentToMarginMainAxis() override;
  LayoutUnit GetContentToMarginCrossAxis() override;

  LayoutUnit GetUsedMinMainAxisSize(
      const SizeLayoutUnit& containing_block_size) override;
  LayoutUnit GetUsedMinCrossAxisSize(
      const SizeLayoutUnit& containing_block_size) override;
  base::Optional<LayoutUnit> GetUsedMaxMainAxisSizeIfNotNone(
      const SizeLayoutUnit& containing_block_size) override;
  base::Optional<LayoutUnit> GetUsedMaxCrossAxisSizeIfNotNone(
      const SizeLayoutUnit& containing_block_size) override;

  void DetermineHypotheticalCrossSize(
      const LayoutParams& layout_params) override;
  LayoutUnit GetMarginBoxMainSize() override;
  LayoutUnit GetMarginBoxCrossSize() override;

  bool CrossSizeIsAuto() override;
  bool MarginMainStartIsAuto() override;
  bool MarginMainEndIsAuto() override;
  bool MarginCrossStartIsAuto() override;
  bool MarginCrossEndIsAuto() override;

  void SetCrossSize(LayoutUnit cross_size) override;
  void SetMainAxisStart(LayoutUnit position) override;
  void SetCrossAxisStart(LayoutUnit position) override;
};

LayoutUnit MainAxisVerticalFlexItem::GetContentToMarginMainAxis() {
  return box()->GetContentToMarginVertical();
}

LayoutUnit MainAxisVerticalFlexItem::GetContentToMarginCrossAxis() {
  return box()->GetContentToMarginHorizontal();
}

LayoutUnit MainAxisVerticalFlexItem::GetUsedMinMainAxisSize(
    const SizeLayoutUnit& containing_block_size) {
  return GetUsedMinHeight(box()->computed_style(), containing_block_size);
}

LayoutUnit MainAxisVerticalFlexItem::GetUsedMinCrossAxisSize(
    const SizeLayoutUnit& containing_block_size) {
  return GetUsedMinWidth(box()->computed_style(), containing_block_size, NULL);
}

base::Optional<LayoutUnit>
MainAxisVerticalFlexItem::GetUsedMaxMainAxisSizeIfNotNone(
    const SizeLayoutUnit& containing_block_size) {
  return GetUsedMaxHeightIfNotNone(box()->computed_style(),
                                   containing_block_size);
}

base::Optional<LayoutUnit>
MainAxisVerticalFlexItem::GetUsedMaxCrossAxisSizeIfNotNone(
    const SizeLayoutUnit& containing_block_size) {
  return GetUsedMaxWidthIfNotNone(box()->computed_style(),
                                  containing_block_size, NULL);
}

void MainAxisVerticalFlexItem::DetermineHypotheticalCrossSize(
    const LayoutParams& layout_params) {
  // 7. Determine the hypothetical cross size of each item
  // By performing layout with the used main size and the available space.
  //   https://www.w3.org/TR/css-flexbox-1/#algo-cross-item
  LayoutParams child_layout_params(layout_params);
  child_layout_params.shrink_to_fit_width_forced = true;
  child_layout_params.freeze_height = true;
  box()->set_height(target_main_size());
  box()->UpdateSize(child_layout_params);
}

LayoutUnit MainAxisVerticalFlexItem::GetMarginBoxMainSize() {
  return box()->GetMarginBoxHeight();
}

LayoutUnit MainAxisVerticalFlexItem::GetMarginBoxCrossSize() {
  return box()->GetMarginBoxWidth();
}

bool MainAxisVerticalFlexItem::CrossSizeIsAuto() {
  return box()->computed_style()->width() == cssom::KeywordValue::GetAuto();
}

bool MainAxisVerticalFlexItem::MarginMainStartIsAuto() {
  return box()->computed_style()->margin_top() ==
         cssom::KeywordValue::GetAuto();
}

bool MainAxisVerticalFlexItem::MarginMainEndIsAuto() {
  return box()->computed_style()->margin_bottom() ==
         cssom::KeywordValue::GetAuto();
}

bool MainAxisVerticalFlexItem::MarginCrossStartIsAuto() {
  return box()->computed_style()->margin_left() ==
         cssom::KeywordValue::GetAuto();
}

bool MainAxisVerticalFlexItem::MarginCrossEndIsAuto() {
  return box()->computed_style()->margin_right() ==
         cssom::KeywordValue::GetAuto();
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

FlexItem::FlexItem(Box* box, LayoutUnit flex_base_size,
                   LayoutUnit hypothetical_main_size)
    : box_(box),
      flex_base_size_(flex_base_size),
      hypothetical_main_size_(hypothetical_main_size) {}

std::unique_ptr<FlexItem> FlexItem::Create(bool main_direction_is_horizontal,
                                           Box* box, LayoutUnit flex_base_size,
                                           LayoutUnit hypothetical_main_size) {
  if (main_direction_is_horizontal) {
    return base::WrapUnique(new MainAxisHorizontalFlexItem(
        box, flex_base_size, hypothetical_main_size));
  } else {
    return base::WrapUnique(new MainAxisVerticalFlexItem(
        box, flex_base_size, hypothetical_main_size));
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
  return GetUsedAlignSelf(box()->computed_style(),
                          box()->parent()->computed_style());
}

const scoped_refptr<cobalt::cssom::PropertyValue>&
FlexItem::GetUsedJustifyContentPropertyValue() {
  DCHECK(box()->parent());
  return box()->parent()->computed_style()->justify_content();
}


}  // namespace layout
}  // namespace cobalt

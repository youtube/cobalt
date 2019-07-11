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

#ifndef COBALT_LAYOUT_FLEX_ITEM_H_
#define COBALT_LAYOUT_FLEX_ITEM_H_

#include <vector>

#include "base/logging.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/layout_unit.h"

namespace cobalt {
namespace layout {

// Base class for all Flex Items.
class FlexItem {
 public:
  // Disallow Copy and Assign.
  FlexItem(const FlexItem&) = delete;
  FlexItem& operator=(const FlexItem&) = delete;

  // Default move constructor to make the class emplace constructible.
  FlexItem(FlexItem&& that) = default;

  FlexItem(Box* box, LayoutUnit flex_base_size,
           LayoutUnit hypothetical_main_size);

  static std::unique_ptr<FlexItem> Create(bool main_direction_is_horizontal_,
                                          Box* box, LayoutUnit flex_base_size,
                                          LayoutUnit hypothetical_main_size);

  Box* box() const { return box_; }
  LayoutUnit flex_base_size() const { return flex_base_size_; }
  LayoutUnit hypothetical_main_size() const { return hypothetical_main_size_; }

  void set_target_main_size(LayoutUnit target_main_size) {
    target_main_size_ = target_main_size;
  }
  LayoutUnit target_main_size() const { return target_main_size_; }

  void set_flex_space(LayoutUnit flex_space) { flex_space_ = flex_space; }
  LayoutUnit flex_space() const { return flex_space_; }

  void set_scaled_flex_shrink_factor(LayoutUnit scaled_flex_shrink_factor) {
    scaled_flex_shrink_factor_ = scaled_flex_shrink_factor;
  }
  LayoutUnit scaled_flex_shrink_factor() const {
    return scaled_flex_shrink_factor_;
  }

  void set_flex_factor(float flex_factor) { flex_factor_ = flex_factor; }
  void DetermineFlexFactor(bool flex_factor_is_grow);
  float flex_factor() const { return flex_factor_; }

  void set_max_violation(bool max_violation) { max_violation_ = max_violation; }
  bool max_violation() const { return max_violation_; }
  void set_min_violation(bool min_violation) { min_violation_ = min_violation; }
  bool min_violation() const { return min_violation_; }

  const scoped_refptr<cobalt::cssom::PropertyValue>&
  GetUsedAlignSelfPropertyValue();
  const scoped_refptr<cobalt::cssom::PropertyValue>&
  GetUsedJustifyContentPropertyValue();

  // Return the size difference between the content and margin box on the main
  // axis.
  virtual LayoutUnit GetContentToMarginMainAxis() = 0;

  // Return the size difference between the content and margin box on the cross
  // axis.
  virtual LayoutUnit GetContentToMarginCrossAxis() = 0;

  // Return the used style for the min size in the main axis.
  virtual LayoutUnit GetUsedMinMainAxisSize(
      const SizeLayoutUnit& containing_block_size) = 0;

  // Return the used style for the min size in the cross axis.
  virtual LayoutUnit GetUsedMinCrossAxisSize(
      const SizeLayoutUnit& containing_block_size) = 0;

  // Return the used style for the max size in the main axis.
  virtual base::Optional<LayoutUnit> GetUsedMaxMainAxisSizeIfNotNone(
      const SizeLayoutUnit& containing_block_size) = 0;

  // Return the used style for the max size in the cross axis.
  virtual base::Optional<LayoutUnit> GetUsedMaxCrossAxisSizeIfNotNone(
      const SizeLayoutUnit& containing_block_size) = 0;

  // Determine the hypothetical cross size.
  // https://www.w3.org/TR/css-flexbox-1/#algo-cross-item
  virtual void DetermineHypotheticalCrossSize(
      const LayoutParams& layout_params) = 0;

  virtual LayoutUnit GetMarginBoxMainSize() = 0;
  virtual LayoutUnit GetMarginBoxCrossSize() = 0;

  // Return true if the computed cross axis size is auto.
  virtual bool CrossSizeIsAuto() = 0;
  // Return true if the margin at the main axis start is auto.
  virtual bool MarginMainStartIsAuto() = 0;
  // Return true if the margin at the main axis end is auto.
  virtual bool MarginMainEndIsAuto() = 0;
  // Return true if the margin at the cross axis start is auto.
  virtual bool MarginCrossStartIsAuto() = 0;
  // Return true if the margin at the cross axis end is auto.
  virtual bool MarginCrossEndIsAuto() = 0;

  virtual void SetCrossSize(LayoutUnit cross_size) = 0;
  virtual void SetMainAxisStart(LayoutUnit position) = 0;
  virtual void SetCrossAxisStart(LayoutUnit position) = 0;

 private:
  Box* const box_ = nullptr;
  const LayoutUnit flex_base_size_ = LayoutUnit();
  const LayoutUnit hypothetical_main_size_ = LayoutUnit();

  LayoutUnit target_main_size_ = LayoutUnit();
  LayoutUnit flex_space_ = LayoutUnit();
  LayoutUnit scaled_flex_shrink_factor_ = LayoutUnit();
  float flex_factor_ = 0;
  bool max_violation_ = false;
  bool min_violation_ = false;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_FLEX_ITEM_H_

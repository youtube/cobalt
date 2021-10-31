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

#include "cobalt/layout/flex_line.h"

#include <algorithm>
#include <list>
#include <vector>

#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

FlexLine::FlexLine(const LayoutParams& layout_params,
                   bool main_direction_is_horizontal,
                   bool direction_is_reversed, LayoutUnit main_size)
    : layout_params_(layout_params),
      main_direction_is_horizontal_(main_direction_is_horizontal),
      direction_is_reversed_(direction_is_reversed),
      main_size_(main_size) {
  items_outer_main_size_ = LayoutUnit(0);
}

bool FlexLine::CanAddItem(const FlexItem& item) const {
  LayoutUnit outer_main_size =
      item.hypothetical_main_size() + item.GetContentToMarginMainAxis();
  LayoutUnit next_main_size = items_outer_main_size_ + outer_main_size;
  if (!items_.empty() && next_main_size > main_size_) {
    return false;
  }
  return true;
}

void FlexLine::AddItem(std::unique_ptr<FlexItem>&& item) {
  LayoutUnit outer_main_size =
      item->hypothetical_main_size() + item->GetContentToMarginMainAxis();

  items_outer_main_size_ += outer_main_size;
  items_.emplace_back(std::move(item));
}

void FlexLine::ResolveFlexibleLengthsAndCrossSize() {
  // Algorithm for resolving flexible lengths:
  //   https://www.w3.org/TR/css-flexbox-1/#resolve-flexible-lengths

  // 1. Determine the used flex factor.
  // If the sum is less than the flex container's inner main size, use the flex
  // grow factor.
  flex_factor_grow_ = items_outer_main_size_ < main_size_;

  // 2. Size inflexible items.
  // 3. Calculate initial free space.
  LayoutUnit flex_space = LayoutUnit();
  LayoutUnit frozen_space = LayoutUnit();
  LayoutUnit scaled_flex_shrink_factor_sum = LayoutUnit();

  // Items are removed from this container in random order.
  std::list<FlexItem*> flexible_items;
  for (auto& item : items_) {
    item->DetermineFlexFactor(flex_factor_grow_);
    // 2. Size inflexible items:
    // - any item that has a flex factor of zero.
    // - if using the flex grow factor: any item that has a flex base size
    //   greater than its hypothetical main size.
    // - if using the flex shrink factor: any item that has a flex base size
    //   smaller than its hypothetical main size.
    LayoutUnit hypothetical_main_size = item->hypothetical_main_size();

    if (0 == item->flex_factor() ||
        (flex_factor_grow_ &&
         (item->flex_base_size() > hypothetical_main_size)) ||
        (!flex_factor_grow_ &&
         (item->flex_base_size() < hypothetical_main_size))) {
      // Freeze, setting its target main size to its hypothetical main size.
      base::Optional<LayoutUnit> content_based_minimum_size =
          item->GetContentBasedMinimumSize(
              layout_params_.containing_block_size);
      if (content_based_minimum_size.has_value()) {
        hypothetical_main_size =
            std::max(hypothetical_main_size, *content_based_minimum_size);
      }
      item->set_target_main_size(hypothetical_main_size);

      // 3. Calculate initial free space.
      // For frozen items, use their outer target main size;
      frozen_space +=
          item->GetContentToMarginMainAxis() + item->target_main_size();
    } else {
      flexible_items.push_back(item.get());

      // for other items, use their outer flex base size.
      item->set_flex_space(item->GetContentToMarginMainAxis() +
                           item->flex_base_size());
      item->set_target_main_size(item->hypothetical_main_size());
      flex_space += item->flex_space();

      // Precalculate the scaled flex shrink factor.
      // If using the flex shrink factor, for every unfrozen item on the line,
      if (!flex_factor_grow_) {
        // multiply its flex shrink factor by its inner flex base size.
        item->set_scaled_flex_shrink_factor(item->flex_base_size() *
                                            item->flex_factor());
        scaled_flex_shrink_factor_sum += item->scaled_flex_shrink_factor();
      }
    }
  }

  LayoutUnit initial_free_space = main_size_ - frozen_space - flex_space;

  // 4. Loop
  while (true) {
    // a. Check for flexible items.
    if (flexible_items.empty()) {
      // If all the flex items on the line are frozen, free space has been
      // distributed; exit this loop.
      break;
    }

    // b. Calculate the remaining free space.
    LayoutUnit remaining_free_space = main_size_ - frozen_space - flex_space;

    // If the sum of the unfrozen flex items' flex factors ...
    float unfrozen_flex_factor_sum = 0;
    for (auto& item : flexible_items) {
      unfrozen_flex_factor_sum += item->flex_factor();
    }

    // ... is less than one, multiply the initial free space by this sum.
    if (unfrozen_flex_factor_sum < 1.0f) {
      LayoutUnit free_space_magnitude =
          initial_free_space * unfrozen_flex_factor_sum;
      // If the magnitude of this value is less than the magnitude of the
      // remaining free space, use this as the remaining free space.
      if ((free_space_magnitude >= LayoutUnit() &&
           free_space_magnitude < remaining_free_space) ||
          (free_space_magnitude < LayoutUnit() &&
           free_space_magnitude > remaining_free_space)) {
        remaining_free_space = free_space_magnitude;
      }
    }

    // c. Distribute free space proportional to the flex factors.
    // If the remaining free space is zero, do nothing.
    if (remaining_free_space != LayoutUnit()) {
      if (flex_factor_grow_) {
        // If using the flex grow factor.
        for (auto& item : flexible_items) {
          // Find the ratio of the item's flex grow factor to the sum of the
          // flex grow factors of all unfrozen items on the line.
          float ratio = item->flex_factor() / unfrozen_flex_factor_sum;
          // Set the item's target main size to its flex base size plus a
          // fraction of the remaining free space proportional to the ratio.
          item->set_target_main_size(item->flex_base_size() +
                                     remaining_free_space * ratio);
        }
      } else {
        // If using the flex shrink factor,
        for (auto& item : flexible_items) {
          // Find the ratio of the item's scaled flex shrink factor to the sum
          // of the scaled flex shrink factors.
          float ratio = item->scaled_flex_shrink_factor().toFloat() /
                        scaled_flex_shrink_factor_sum.toFloat();
          // Set the item's target main size to its flex base size minus a
          // fraction of the absolute value of the remaining free space
          // proportional to the ratio.
          item->set_target_main_size(item->flex_base_size() +
                                     remaining_free_space * ratio);
        }
      }
    }

    // d. Fix min/max violations.
    // Clamp each non-frozen item's target main size by its used min and max
    // main sizes. and floor its content-box size at zero.
    LayoutUnit unclamped_size = LayoutUnit();
    LayoutUnit clamped_size = LayoutUnit();
    for (auto& item : flexible_items) {
      base::Optional<LayoutUnit> maybe_used_min_space;
      if (flex_factor_grow_) {
        maybe_used_min_space = item->GetUsedMinMainAxisSizeIfNotAuto(
            layout_params_.containing_block_size);
      } else {
        maybe_used_min_space = item->GetContentBasedMinimumSize(
            layout_params_.containing_block_size);
      }
      base::Optional<LayoutUnit> used_max_space =
          item->GetUsedMaxMainAxisSizeIfNotNone(
              layout_params_.containing_block_size);

      unclamped_size += item->target_main_size();
      if (maybe_used_min_space &&
          (item->target_main_size() < *maybe_used_min_space)) {
        item->set_target_main_size(*maybe_used_min_space);
        item->set_min_violation(true);
      } else if (used_max_space && item->target_main_size() > *used_max_space) {
        item->set_target_main_size(*used_max_space);
        item->set_max_violation(true);
      } else if (item->target_main_size() < LayoutUnit()) {
        item->set_target_main_size(LayoutUnit());
        item->set_min_violation(true);
      }
      clamped_size += item->target_main_size();
    }

    // e. Freeze over-flexed items.

    // If the total violation is zero, freeze all items.
    bool freeze_all = clamped_size == unclamped_size;
    // If the total violation is positive, freeze all the items with min
    // violations.
    bool freeze_min_violations = clamped_size > unclamped_size;
    // If the total violation is negative, freeze all the items with max
    // violations.
    bool freeze_max_violations = clamped_size < unclamped_size;
    // Otherwise, do nothing.
    if (!(freeze_all || freeze_min_violations || freeze_max_violations)) {
      continue;
    }

    // Freeze the items by removing them from flexible_items and adding their
    // outer main size to the frozen space.
    for (auto item_iterator = flexible_items.begin();
         item_iterator != flexible_items.end();) {
      auto current_iterator = item_iterator++;
      auto& item = *current_iterator;
      if (freeze_all || (freeze_min_violations && item->min_violation()) ||
          (freeze_max_violations && item->max_violation())) {
        frozen_space +=
            item->GetContentToMarginMainAxis() + item->target_main_size();
        flex_space -= item->flex_space();
        if (!flex_factor_grow_) {
          scaled_flex_shrink_factor_sum -= item->scaled_flex_shrink_factor();
        }
        flexible_items.erase(current_iterator);
      }
    }
    // f. return to the start of this loop.
  }
  items_outer_main_size_ = frozen_space;

  // 5. Set each item's used main size to its target main size.
  //   https://www.w3.org/TR/css-flexbox-1/#resolve-flexible-lengths
  // Also, algorithm for Flex Layout continued from step 7:
  //   https://www.w3.org/TR/css-flexbox-1/#layout-algorithm
  // Cross Size Determination:
  // 7. Determine the hypothetical cross size of each item
  // By performing layout with the used main size and the available space.
  for (auto& item : items_) {
    item->DetermineHypotheticalCrossSize(layout_params_);
  }
}

void FlexLine::CalculateCrossSize() {
  // Algorithm for Calculate the cross size of each flex line.
  //   https://www.w3.org/TR/css-flexbox-1/#algo-cross-line

  LayoutUnit max_baseline_to_bottom = LayoutUnit();
  LayoutUnit max_hypothetical_cross_size = LayoutUnit();
  for (auto& item : items_) {
    // 1. Collect all the flex items whose inline-axis is parallel to the
    // main-axis, whose align-self is baseline, and whose cross-axis margins
    // are both non-auto.
    if (main_direction_is_horizontal_ &&
        item->GetUsedAlignSelfPropertyValue() ==
            cssom::KeywordValue::GetBaseline() &&
        !item->MarginCrossStartIsAuto() && !item->MarginCrossEndIsAuto()) {
      // Find the largest of the distances between each item's baseline and its
      // hypothetical outer cross-start edge,
      LayoutUnit baseline_to_top =
          item->box()->GetBaselineOffsetFromTopMarginEdge();
      max_baseline_to_top_ = std::max(
          max_baseline_to_top_.value_or(LayoutUnit()), baseline_to_top);
      // and the largest of the distances between each item's baseline and its
      // hypothetical outer cross-end edge,
      LayoutUnit baseline_to_bottom = item->box()->height() - baseline_to_top;
      max_baseline_to_bottom =
          std::max(max_baseline_to_bottom, baseline_to_bottom);
    } else {
      // 2. Among all the items not collected by the previous step, find the
      // largest outer hypothetical cross size.
      LayoutUnit hypothetical_cross_size = item->GetMarginBoxCrossSize();
      if (hypothetical_cross_size > max_hypothetical_cross_size) {
        max_hypothetical_cross_size = hypothetical_cross_size;
      }
    }
  }
  // ... and sum these two values.
  LayoutUnit max_baseline_cross_size =
      max_baseline_to_top_.value_or(LayoutUnit()) + max_baseline_to_bottom;

  // 3. The used cross-size of the flex line is the largest of the numbers
  // found in the previous two steps and zero.
  cross_size_ = std::max(max_hypothetical_cross_size, max_baseline_cross_size);
}

void FlexLine::DetermineUsedCrossSizes(LayoutUnit container_cross_size) {
  // 11. Determine the used cross size of each flex item->
  //   https://www.w3.org/TR/css-flexbox-1/#algo-stretch
  SizeLayoutUnit containing_block_size(LayoutUnit(), container_cross_size);
  for (auto& item : items_) {
    // If a flex item has align-self: stretch,
    // its computed cross size property is auto,
    // and neither of its cross-axis margins are auto,
    if (item->GetUsedAlignSelfPropertyValue() ==
            cssom::KeywordValue::GetStretch() &&
        item->CrossSizeIsAuto() && !item->MarginCrossStartIsAuto() &&
        !item->MarginCrossEndIsAuto()) {
      // The used outer cross size is the used cross size of its flex line,
      // clamped according to the item's used min and max cross sizes.
      LayoutUnit cross_size = cross_size_ - item->GetContentToMarginCrossAxis();
      base::Optional<LayoutUnit> min_cross_size =
          item->GetUsedMinCrossAxisSizeIfNotAuto(containing_block_size);
      if (min_cross_size && (*min_cross_size > cross_size)) {
        cross_size = *min_cross_size;
      }
      base::Optional<LayoutUnit> max_cross_size =
          item->GetUsedMaxCrossAxisSizeIfNotNone(containing_block_size);
      if (max_cross_size && *max_cross_size < cross_size) {
        cross_size = *max_cross_size;
      }
      item->SetCrossSize(cross_size);

      // TODO: If the flex item has align-self: stretch, redo layout for its
      // contents, treating this used size as its definite cross size so that
      // percentage-sized children can be resolved.
    }
  }
}

void FlexLine::SetMainAxisPosition(LayoutUnit pos, FlexItem* item) {
  if (direction_is_reversed_) {
    item->SetMainAxisStart(main_size_ - pos - item->GetMarginBoxMainSize());
  } else {
    item->SetMainAxisStart(pos);
  }
}

void FlexLine::DoMainAxisAlignment() {
  // Algorithm for main axis alignment:
  //   https://www.w3.org/TR/css-flexbox-1/#main-alignment
  // 12. Distribute any remaining free space.
  //   https://www.w3.org/TR/css-flexbox-1/#algo-main-align

  // If the remaining free space is positive and at least one main-axis margin
  // on this line is auto, distribute the free space equally among these
  // margins.
  std::vector<bool> auto_margins(items_.size() * 2);
  int auto_margin_count = 0;
  int margin_idx = 0;
  for (auto& item : items_) {
    bool auto_main_start = item->MarginMainStartIsAuto();
    bool auto_main_end = item->MarginMainEndIsAuto();
    auto_margins[margin_idx++] = auto_main_start;
    auto_margins[margin_idx++] = auto_main_end;
    auto_margin_count += (auto_main_start ? 1 : 0) + (auto_main_end ? 1 : 0);
  }
  LayoutUnit leftover_free_space = main_size_ - items_outer_main_size_;
  if (auto_margin_count > 0) {
    LayoutUnit free_space_between = leftover_free_space / auto_margin_count;

    margin_idx = 0;
    LayoutUnit pos = LayoutUnit();
    for (auto& item : items_) {
      pos += auto_margins[margin_idx++] ? free_space_between : LayoutUnit();
      LayoutUnit main_size = item->GetMarginBoxMainSize();
      SetMainAxisPosition(pos, item.get());
      pos += main_size;
      pos += auto_margins[margin_idx++] ? free_space_between : LayoutUnit();
    }

    return;
  }

  DCHECK(!items_.empty());

  // Align the items along the main-axis per justify-content.
  const scoped_refptr<cobalt::cssom::PropertyValue>& justify_content =
      items_.front()->GetUsedJustifyContentPropertyValue();

  bool leftover_free_space_is_negative = leftover_free_space < LayoutUnit();

  if (justify_content == cssom::KeywordValue::GetFlexStart() ||
      (leftover_free_space_is_negative &&
       justify_content == cssom::KeywordValue::GetSpaceBetween())) {
    // Flex items are packed toward the start of the line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-flex-start
    // If the leftover free-space is negative, space-between is identical to
    // flex-start.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-space-between
    LayoutUnit pos = LayoutUnit();
    for (auto& item : items_) {
      LayoutUnit main_size = item->GetMarginBoxMainSize();
      SetMainAxisPosition(pos, item.get());
      pos += main_size;
    }
  } else if (justify_content == cssom::KeywordValue::GetFlexEnd()) {
    // Flex items are packed toward the end of the line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-flex-end
    LayoutUnit pos = main_size_;
    for (auto item_iterator = items_.rbegin(); item_iterator != items_.rend();
         ++item_iterator) {
      auto& item = *item_iterator;
      LayoutUnit main_size = item->GetMarginBoxMainSize();
      pos -= main_size;
      SetMainAxisPosition(pos, item.get());
    }
  } else if (justify_content == cssom::KeywordValue::GetCenter() ||
             (leftover_free_space_is_negative &&
              justify_content == cssom::KeywordValue::GetSpaceAround())) {
    // Flex items are packed toward the center of the line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-center
    // If the leftover free-space is negative, space-around is identical to
    // center.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-space-around
    LayoutUnit pos = leftover_free_space / 2;
    for (auto& item : items_) {
      LayoutUnit main_size = item->GetMarginBoxMainSize();
      SetMainAxisPosition(pos, item.get());
      pos += main_size;
    }
  } else if (justify_content == cssom::KeywordValue::GetSpaceBetween()) {
    // Flex items are evenly distributed in the line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-space-between
    LayoutUnit free_space_between =
        (items_.size() < 2)
            ? LayoutUnit()
            : leftover_free_space / static_cast<int>(items_.size() - 1);
    LayoutUnit pos = LayoutUnit();
    for (auto& item : items_) {
      LayoutUnit main_size = item->GetMarginBoxMainSize();
      SetMainAxisPosition(pos, item.get());
      pos += main_size + free_space_between;
    }
  } else if (justify_content == cssom::KeywordValue::GetSpaceAround()) {
    // Flex items are evenly distributed in the line, with half-size spaces on
    // either end.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-space-around
    LayoutUnit free_space_between =
        leftover_free_space / static_cast<int>(items_.size());
    LayoutUnit free_space_before = free_space_between / 2;
    LayoutUnit pos = LayoutUnit();

    for (auto& item : items_) {
      LayoutUnit main_size = item->GetMarginBoxMainSize();
      SetMainAxisPosition(pos + free_space_before, item.get());
      pos += main_size + free_space_between;
    }
  } else {
    // Added as sanity check for unsupported values.
    NOTREACHED();
  }
}

void FlexLine::DoCrossAxisAlignment(LayoutUnit line_cross_axis_start) {
  // Algorithm for cross axis alignment:
  //   https://www.w3.org/TR/css-flexbox-1/#cross-alignment
  // 13. Resolve cross-axis auto margins.
  // 14. Align all flex items along the cross-axis per align-self.
  for (auto& item : items_) {
    LayoutUnit cross_axis_start = LayoutUnit();

    bool auto_margin_cross_start = item->MarginCrossStartIsAuto();
    bool auto_margin_cross_end = item->MarginCrossEndIsAuto();
    LayoutUnit cross_size = item->GetMarginBoxCrossSize();
    if (auto_margin_cross_start || auto_margin_cross_end) {
      if (auto_margin_cross_start) {
        cross_axis_start =
            (cross_size_ - cross_size) / (auto_margin_cross_end ? 2 : 1);
      }
    } else {
      const auto& align_self = item->GetUsedAlignSelfPropertyValue();
      // Only flex-end, center, and baseline can result in a cross_axis_start
      // that is not aligned to the line cross start edge.
      if (align_self == cssom::KeywordValue::GetFlexEnd()) {
        cross_axis_start = cross_size_ - cross_size;
      } else if (align_self == cssom::KeywordValue::GetCenter()) {
        cross_axis_start = (cross_size_ - cross_size) / 2;
      } else if (align_self == cssom::KeywordValue::GetBaseline()) {
        if (main_direction_is_horizontal_ && max_baseline_to_top_.has_value()) {
          LayoutUnit baseline_to_top =
              item->box()->GetBaselineOffsetFromTopMarginEdge();
          cross_axis_start = *max_baseline_to_top_ - baseline_to_top;
        }
      } else {
        DCHECK((align_self == cssom::KeywordValue::GetFlexStart()) ||
               (align_self == cssom::KeywordValue::GetStretch()));
      }
    }
    item->SetCrossAxisStart(line_cross_axis_start + cross_axis_start);
  }
}

LayoutUnit FlexLine::GetBaseline() {
  if (max_baseline_to_top_.has_value()) {
    return *max_baseline_to_top_;
  }
  LayoutUnit baseline = cross_size_;
  if (!items_.empty()) {
    Box* box = items_.front()->box();
    baseline = box->top() + box->GetBaselineOffsetFromTopMarginEdge();
  }
  return baseline;
}

}  // namespace layout
}  // namespace cobalt

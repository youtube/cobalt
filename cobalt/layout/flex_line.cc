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
#include "cobalt/cssom/number_value.h"
#include "cobalt/layout/container_box.h"
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

LayoutUnit FlexLine::GetOuterMainSizeOfBox(Box* box,
                                           LayoutUnit content_main_size) const {
  if (main_direction_is_horizontal_) {
    return content_main_size + box->GetContentToMarginHorizontal();
  }
  return content_main_size + box->GetContentToMarginVertical();
}

bool FlexLine::TryAddItem(Box* item, LayoutUnit flex_base_size,
                          LayoutUnit hypothetical_main_size) {
  LayoutUnit outer_main_size =
      GetOuterMainSizeOfBox(item, hypothetical_main_size);

  LayoutUnit next_main_size = items_outer_main_size_ + outer_main_size;

  if (!items_.empty() && next_main_size > main_size_) {
    return false;
  }

  items_outer_main_size_ = next_main_size;
  items_.emplace_back(
      Item(item, flex_base_size, hypothetical_main_size, outer_main_size));
  return true;
}

void FlexLine::AddItem(Box* item, LayoutUnit flex_base_size,
                       LayoutUnit hypothetical_main_size) {
  LayoutUnit outer_main_size =
      GetOuterMainSizeOfBox(item, hypothetical_main_size);

  items_outer_main_size_ += outer_main_size;

  items_.emplace_back(
      Item(item, flex_base_size, hypothetical_main_size, outer_main_size));
}

float FlexLine::GetFlexFactor(Box* box) {
  auto flex_factor_property = flex_factor_grow_
                                  ? box->computed_style()->flex_grow()
                                  : box->computed_style()->flex_shrink();
  return base::polymorphic_downcast<const cssom::NumberValue*>(
             flex_factor_property.get())
      ->value();
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
  std::list<Item*> flexible_items;
  for (auto& item : items_) {
    item.flex_factor = GetFlexFactor(item.box);
    // any item that has a flex factor of zero
    // if using the flex grow factor: any item that has a flex base size greater
    // than its hypothetical main size if using the flex shrink factor: any item
    // that has a flex base size smaller than its hypothetical main size
    if (0 == item.flex_factor ||
        (flex_factor_grow_ &&
         (item.flex_base_size > item.hypothetical_main_size)) ||
        (!flex_factor_grow_ &&
         (item.flex_base_size < item.hypothetical_main_size))) {
      // Freeze, setting its target main size to its hypothetical main size.
      item.target_main_size = item.hypothetical_main_size;

      // For frozen items, use their outer target main size;
      frozen_space += GetOuterMainSizeOfBox(item.box, item.target_main_size);
    } else {
      flexible_items.push_back(&item);
      // for other items, use their outer flex base size.
      item.flex_space = GetOuterMainSizeOfBox(item.box, item.flex_base_size);
      item.target_main_size = item.hypothetical_main_size;
      flex_space += item.flex_space;

      // Precalculate the scaled flex shrink factor.
      // If using the flex shrink factor, for every unfrozen item on the line,
      if (!flex_factor_grow_) {
        // multiply its flex shrink factor by its inner flex base size.
        item.scaled_flex_shrink_factor = item.flex_base_size * item.flex_factor;
        scaled_flex_shrink_factor_sum += item.scaled_flex_shrink_factor;
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
      unfrozen_flex_factor_sum += item->flex_factor;
    }

    // ... is less than one, multiply the initial free space by this sum.
    if (unfrozen_flex_factor_sum < 1.0f) {
      LayoutUnit free_space_magnitude =
          initial_free_space * unfrozen_flex_factor_sum;
      // If the magnitude of this value is less than the magnitude of the
      // remaining free space, use this as the remaining free space.
      if (free_space_magnitude < remaining_free_space) {
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
          float ratio = item->flex_factor / unfrozen_flex_factor_sum;
          // Set the item's target main size to its flex base size plus a
          // fraction of the remaining free space proportional to the ratio.
          item->target_main_size =
              item->flex_base_size + remaining_free_space * ratio;
        }
      } else {
        // If using the flex shrink factor,
        for (auto& item : flexible_items) {
          // Find the ratio of the item's scaled flex shrink factor to the sum
          // of the scaled flex shrink factors.
          float ratio = item->scaled_flex_shrink_factor.toFloat() /
                        scaled_flex_shrink_factor_sum.toFloat();
          // Set the item's target main size to its flex base size minus a
          // fraction of the absolute value of the remaining free space
          // proportional to the ratio.
          item->target_main_size =
              item->flex_base_size + remaining_free_space * ratio;
        }
      }
    }

    // d. Fix min/max violations.
    // Clamp each non-frozen item's target main size by its used min and max
    // main sizes. and floor its content-box size at zero.
    LayoutUnit unclamped_size = LayoutUnit();
    LayoutUnit clamped_size = LayoutUnit();
    for (auto& item : flexible_items) {
      LayoutUnit used_min_space;
      base::Optional<LayoutUnit> used_max_space;
      if (main_direction_is_horizontal_) {
        used_min_space =
            GetUsedMinWidth(item->box->computed_style(),
                            layout_params_.containing_block_size, NULL);
        used_max_space = GetUsedMaxWidthIfNotNone(
            item->box->computed_style(), layout_params_.containing_block_size,
            NULL);
      } else {
        used_min_space = GetUsedMinHeight(item->box->computed_style(),
                                          layout_params_.containing_block_size);
        used_max_space = GetUsedMaxHeightIfNotNone(
            item->box->computed_style(), layout_params_.containing_block_size);
      }
      unclamped_size += item->target_main_size;
      if (item->target_main_size < used_min_space) {
        item->target_main_size = used_min_space;
        item->min_violation = true;
      } else if (used_max_space && item->target_main_size > *used_max_space) {
        item->target_main_size = *used_max_space;
        item->max_violation = true;
      } else if (item->target_main_size < LayoutUnit()) {
        item->target_main_size = LayoutUnit();
        item->min_violation = true;
      }
      clamped_size += item->target_main_size;
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
      auto item = item_iterator++;
      if (freeze_all || (freeze_min_violations && (*item)->min_violation) ||
          (freeze_max_violations && (*item)->max_violation)) {
        frozen_space +=
            GetOuterMainSizeOfBox((*item)->box, (*item)->target_main_size);
        flex_space -= (*item)->flex_space;
        if (!flex_factor_grow_) {
          scaled_flex_shrink_factor_sum -= (*item)->scaled_flex_shrink_factor;
        }
        flexible_items.erase(item);
      }
    }
    // f. return to the start of this loop.
  }
  items_outer_main_size_ = frozen_space;

  // 5. Set each item's used main size to its target main size.

  // Also, algorithm for Flex Layout continued from step 7:
  //   https://www.w3.org/TR/css-flexbox-1/#layout-algorithm
  // Cross Size Determination:
  // 7. Determine the hypothetical cross size of each item.
  // By performing layout with the used main size and the available space.
  if (main_direction_is_horizontal_) {
    for (auto& item : items_) {
      LayoutParams child_layout_params(layout_params_);
      child_layout_params.shrink_to_fit_width_forced = false;
      child_layout_params.freeze_width = true;
      item.box->set_width(item.target_main_size);
      item.box->UpdateSize(child_layout_params);
    }
  } else {
    for (auto& item : items_) {
      LayoutParams child_layout_params(layout_params_);
      child_layout_params.shrink_to_fit_width_forced = false;
      child_layout_params.freeze_height = true;
      item.box->set_height(item.target_main_size);
      item.box->UpdateSize(child_layout_params);
    }
  }
}

void FlexLine::CalculateCrossSize() {
  // Algorithm for Calculate the cross size of each flex line.
  //   https://www.w3.org/TR/css-flexbox-1/#algo-cross-line

  LayoutUnit max_baseline_to_top = LayoutUnit();
  LayoutUnit max_baseline_to_bottom = LayoutUnit();
  LayoutUnit max_hypothetical_cross_size = LayoutUnit();
  for (auto& item : items_) {
    // 1. Collect all the flex items whose inline-axis is parallel to the
    // main-axis, whose align-self is baseline, and whose cross-axis margins
    // are both non-auto.
    auto style = item.box->computed_style();
    const scoped_refptr<cobalt::cssom::PropertyValue>& align_self =
        GetUsedAlignSelf(style, item.box->parent()->computed_style());
    if (main_direction_is_horizontal_ &&
        align_self == cssom::KeywordValue::GetBaseline() &&
        style->margin_top() != cssom::KeywordValue::GetAuto() &&
        style->margin_bottom() != cssom::KeywordValue::GetAuto()) {
      // Find the largest of the distances between each item's baseline and its
      // hypothetical outer cross-start edge,
      LayoutUnit baseline_to_top =
          item.box->GetBaselineOffsetFromTopMarginEdge();
      if (baseline_to_top > max_baseline_to_top) {
        max_baseline_to_top = baseline_to_top;
      }
      // and the largest of the distances between each item's baseline and its
      // hypothetical outer cross-end edge,
      LayoutUnit baseline_to_bottom = item.box->height() - baseline_to_top;
      if (baseline_to_bottom > max_baseline_to_bottom) {
        max_baseline_to_bottom = baseline_to_bottom;
      }
    } else {
      // 2. Among all the items not collected by the previous step, find the
      // largest outer hypothetical cross size.
      LayoutUnit hypothetical_cross_size = item.box->GetMarginBoxHeight();
      if (hypothetical_cross_size > max_hypothetical_cross_size) {
        max_hypothetical_cross_size = hypothetical_cross_size;
      }
    }
  }
  // ... and sum these two values.
  LayoutUnit max_baseline_cross_size =
      max_baseline_to_top + max_baseline_to_bottom;

  // 3. The used cross-size of the flex line is the largest of the numbers
  // found in the previous two steps and zero.
  cross_size_ = std::max(max_hypothetical_cross_size, max_baseline_cross_size);
}

void FlexLine::DetermineUsedCrossSizes(LayoutUnit container_cross_size) {
  // 11. Determine the used cross size of each flex item.
  //   https://www.w3.org/TR/css-flexbox-1/#algo-stretch
  if (main_direction_is_horizontal_) {
    SizeLayoutUnit containing_block_size(LayoutUnit(), container_cross_size);
    for (auto& item : items_) {
      DCHECK(item.box->parent());
      const scoped_refptr<cobalt::cssom::PropertyValue>& align_self =
          GetUsedAlignSelf(item.box->computed_style(),
                           item.box->parent()->computed_style());
      // If a flex item has align-self: stretch,
      // its computed cross size property is auto,
      // and neither of its cross-axis margins are auto,
      if (align_self == cssom::KeywordValue::GetStretch() &&
          item.box->computed_style()->height() ==
              cssom::KeywordValue::GetAuto() &&
          item.box->computed_style()->margin_top() !=
              cssom::KeywordValue::GetAuto() &&
          item.box->computed_style()->margin_bottom() !=
              cssom::KeywordValue::GetAuto()) {
        // The used outer cross size is the used cross size of its flex line,
        // clamped according to the item's used min and max cross sizes.
        LayoutUnit cross_size =
            cross_size_ - item.box->GetContentToMarginVertical();
        LayoutUnit min_height =
            GetUsedMinHeight(item.box->computed_style(), containing_block_size);
        if (min_height > cross_size) {
          cross_size = min_height;
        }
        base::Optional<LayoutUnit> max_height = GetUsedMaxHeightIfNotNone(
            item.box->computed_style(), containing_block_size);
        if (max_height && *max_height < cross_size) {
          cross_size = *max_height;
        }
        item.box->set_height(cross_size);

        // TODO: If the flex item has align-self: stretch, redo layout for its
        // contents, treating this used size as its definite cross size so that
        // percentage-sized children can be resolved.
      }
    }
  } else {
    // TODO: implement this for column flex containers.
    NOTIMPLEMENTED() << "Column flex boxes not yet implemented.";
  }
}

void FlexLine::SetBoxPosition(LayoutUnit pos, Box* box) {
  if (direction_is_reversed_) {
    box->set_left(main_size_ - pos - box->GetMarginBoxWidth());
  } else {
    box->set_left(pos);
  }
}

void FlexLine::DoMainAxisAlignment() {
  // Algorithm for main axis alignment:
  //   https://www.w3.org/TR/css-flexbox-1/#main-alignment
  // 12. Distribute any remaining free space.
  //   https://www.w3.org/TR/css-flexbox-1/#algo-main-align

  // TODO implement for row-reverse, column, and column-reverse.
  if (!main_direction_is_horizontal_) {
    // TODO: implement this for column flex containers.
    NOTIMPLEMENTED() << "Column flex boxes not yet implemented.";
  }

  // If the remaining free space is positive and at least one main-axis margin
  // on this line is auto, distribute the free space equally among these
  // margins.
  std::vector<bool> auto_margins(items_.size());
  int auto_margin_count = 0;
  int margin_idx = 0;
  for (auto& item : items_) {
    auto style = item.box->computed_style();
    bool left = style->margin_left() == cssom::KeywordValue::GetAuto();
    bool right = style->margin_right() == cssom::KeywordValue::GetAuto();
    auto_margins[margin_idx++] = left;
    auto_margins[margin_idx++] = right;
    auto_margin_count += (left ? 1 : 0) + (right ? 1 : 0);
  }
  if (auto_margin_count > 0) {
    LayoutUnit leftover_free_space = main_size_ - items_outer_main_size_;
    LayoutUnit free_space_between = leftover_free_space / auto_margin_count;

    margin_idx = 0;
    LayoutUnit pos = LayoutUnit();
    for (auto& item : items_) {
      pos += auto_margins[margin_idx++] ? free_space_between : LayoutUnit();
      LayoutUnit width = item.box->GetMarginBoxWidth();
      SetBoxPosition(pos, item.box);
      pos += width;
      pos += auto_margins[margin_idx++] ? free_space_between : LayoutUnit();
    }

    return;
  }

  DCHECK(!items_.empty());

  // Align the items along the main-axis per justify-content.
  const scoped_refptr<cobalt::cssom::PropertyValue>& justify_content =
      items_.front().box->parent()->computed_style()->justify_content();

  if (justify_content == cssom::KeywordValue::GetFlexStart()) {
    // Flex items are packed toward the start of the line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-flex-start
    LayoutUnit pos = LayoutUnit();
    for (auto& item : items_) {
      LayoutUnit width = item.box->GetMarginBoxWidth();
      SetBoxPosition(pos, item.box);
      pos += width;
    }
  } else if (justify_content == cssom::KeywordValue::GetFlexEnd()) {
    // Flex items are packed toward the end of the line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-flex-end
    LayoutUnit pos = main_size_;
    for (auto item = items_.rbegin(); item != items_.rend(); ++item) {
      LayoutUnit width = item->box->GetMarginBoxWidth();
      pos -= width;
      SetBoxPosition(pos, item->box);
    }
  } else if (justify_content == cssom::KeywordValue::GetCenter()) {
    // Flex items are packed toward the center of the line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-center
    LayoutUnit pos = (main_size_ - items_outer_main_size_) / 2;
    for (auto& item : items_) {
      LayoutUnit width = item.box->GetMarginBoxWidth();
      SetBoxPosition(pos, item.box);
      pos += width;
    }
  } else if (justify_content == cssom::KeywordValue::GetSpaceBetween()) {
    // Flex items are evenly distributed in the line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-space-between
    LayoutUnit leftover_free_space = main_size_ - items_outer_main_size_;
    LayoutUnit free_space_between =
        (items_.size() < 2)
            ? LayoutUnit()
            : leftover_free_space / static_cast<int>(items_.size() - 1);
    LayoutUnit pos = LayoutUnit();
    for (auto& item : items_) {
      LayoutUnit width = item.box->GetMarginBoxWidth();
      SetBoxPosition(pos, item.box);
      pos += width + free_space_between;
    }
  } else if (justify_content == cssom::KeywordValue::GetSpaceAround()) {
    // Flex items are evenly distributed in the line, with half-size spaces on
    // either end.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-space-around
    LayoutUnit leftover_free_space = main_size_ - items_outer_main_size_;
    LayoutUnit free_space_between =
        leftover_free_space / static_cast<int>(items_.size());
    LayoutUnit free_space_before = free_space_between / 2;
    LayoutUnit pos = LayoutUnit();

    for (auto& item : items_) {
      LayoutUnit width = item.box->GetMarginBoxWidth();
      SetBoxPosition(pos + free_space_before, item.box);
      pos += width + free_space_between;
    }
  } else {
    // Added as sanity check for unsupported values.
    NOTREACHED();
  }
}

void FlexLine::DoCrossAxisAlignment(LayoutUnit line_top) {
  if (!main_direction_is_horizontal_) {
    NOTIMPLEMENTED() << "Column flex boxes not yet implemented.";
  }

  line_top_ = line_top;
  // Algorithm for cross axis alignment:
  //   https://www.w3.org/TR/css-flexbox-1/#cross-alignment
  // 13. Resolve cross-axis auto margins.
  // 14. Align all flex items along the cross-axis per align-self.
  for (auto& item : items_) {
    LayoutUnit top = LayoutUnit();

    auto style = item.box->computed_style();
    const scoped_refptr<cobalt::cssom::PropertyValue>& align_self =
        GetUsedAlignSelf(style, item.box->parent()->computed_style());
    bool auto_margin_top =
        style->margin_top() == cssom::KeywordValue::GetAuto();
    bool auto_margin_bottom =
        style->margin_bottom() == cssom::KeywordValue::GetAuto();
    LayoutUnit cross_size = item.box->GetMarginBoxHeight();
    if (auto_margin_top || auto_margin_bottom) {
      if (auto_margin_top) {
        top = (cross_size_ - cross_size) / (auto_margin_bottom ? 2 : 1);
      }
    } else {
      // Only flex-end, center, and baseline can result in a top that is not
      // aligned to the line cross start edge.
      if (align_self == cssom::KeywordValue::GetFlexEnd()) {
        top = cross_size_ - cross_size;
      } else if (align_self == cssom::KeywordValue::GetCenter()) {
        top = (cross_size_ - cross_size) / 2;
      } else if (align_self == cssom::KeywordValue::GetBaseline()) {
      } else {
        DCHECK((align_self == cssom::KeywordValue::GetFlexStart()) ||
               (align_self == cssom::KeywordValue::GetStretch()));
      }
    }
    item.box->set_top(line_top + top);
  }
}

LayoutUnit FlexLine::GetBaseline() {
  // TODO: Complete implementation of flex container baselines.
  //   https://www.w3.org/TR/css-flexbox-1/#flex-baselines

  LayoutUnit baseline = LayoutUnit();
  if (!items_.empty()) {
    baseline = items_.front().box->GetBaselineOffsetFromTopMarginEdge();
  }
  return line_top_ + baseline;
}

}  // namespace layout
}  // namespace cobalt

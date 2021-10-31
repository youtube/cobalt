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

#include "cobalt/layout/flex_formatting_context.h"

#include <algorithm>

#include "base/logging.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/line_box.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

FlexFormattingContext::FlexFormattingContext(const LayoutParams& layout_params,
                                             bool main_direction_is_horizontal,
                                             bool direction_is_reversed)
    : layout_params_(layout_params),
      main_direction_is_horizontal_(main_direction_is_horizontal),
      direction_is_reversed_(direction_is_reversed),
      fit_content_main_size_(LayoutUnit()) {}

FlexFormattingContext::~FlexFormattingContext() {}

void FlexFormattingContext::UpdateRect(Box* child_box) {
  DCHECK(!child_box->IsAbsolutelyPositioned());
  {
    LayoutParams child_layout_params(layout_params_);
    child_layout_params.shrink_to_fit_width_forced = true;
    child_box->UpdateSize(child_layout_params);
  }

  // Shrink-to-fit doesn't exists anymore by itself in CSS3. It is called the
  // fit-content size, which is derived from the 'min-content' and 'max-content'
  // sizes and the 'stretch-fit' size.
  //   https://www.w3.org/TR/css-sizing-3/#fit-content-size
  // which depends on the intrinsic sizes:
  //   https://www.w3.org/TR/css-flexbox-1/#intrinsic-sizes
  // Note that for column flex-direction, this is the intrinsic cross size.

  if (main_direction_is_horizontal_) {
    fit_content_main_size_ = shrink_to_fit_width() + child_box->width() +
                             child_box->GetContentToMarginHorizontal();
    set_shrink_to_fit_width(fit_content_main_size_);
  } else {
    fit_content_main_size_ +=
        child_box->height() + child_box->GetContentToMarginVertical();
  }
  set_auto_height(child_box->height());
}

void FlexFormattingContext::CollectItemIntoLine(
    LayoutUnit main_space, std::unique_ptr<FlexItem>&& item) {
  // Collect flex items into flex lines:
  //   https://www.w3.org/TR/css-flexbox-1/#algo-line-break
  if (lines_.empty()) {
    lines_.emplace_back(new FlexLine(layout_params_,
                                     main_direction_is_horizontal_,
                                     direction_is_reversed_, main_space));
    fit_content_main_size_ = LayoutUnit();
  }

  DCHECK(!lines_.empty());
  if (multi_line_ && !lines_.back()->CanAddItem(*item)) {
    lines_.emplace_back(new FlexLine(layout_params_,
                                     main_direction_is_horizontal_,
                                     direction_is_reversed_, main_space));
  }

  lines_.back()->AddItem(std::move(item));
  fit_content_main_size_ =
      std::max(fit_content_main_size_, lines_.back()->items_outer_main_size());
}

void FlexFormattingContext::ResolveFlexibleLengthsAndCrossSizes(
    const base::Optional<LayoutUnit>& cross_space,
    const base::Optional<LayoutUnit>& min_cross_space,
    const base::Optional<LayoutUnit>& max_cross_space,
    const scoped_refptr<cssom::PropertyValue>& align_content) {
  // Algorithm for Flex Layout:
  //   https://www.w3.org/TR/css-flexbox-1/#layout-algorithm

  // 6. Resolve the flexible lengths.
  // Cross Size Determination:
  // 7. Determine the hypothetical cross size of each item.
  for (auto& line : lines_) {
    line->ResolveFlexibleLengthsAndCrossSize();
  }

  // 8. Calculate the cross size of each flex line.

  // If the flex container is single-line and has a definite cross size, the
  // cross size of the flex line is the flex container's inner cross size.
  if (!multi_line_ && cross_space && !lines_.empty()) {
    lines_.front()->set_cross_size(*cross_space);
  } else {
    // Otherwise, for each flex line:
    for (auto& line : lines_) {
      line->CalculateCrossSize();
    }
  }

  // If the flex container is single-line, then clamp the line's cross-size
  // to be within the container's computed min and max cross sizes.
  //   https://www.w3.org/TR/css-flexbox-1/#change-201403-clamp-single-line
  if (!multi_line_ && !lines_.empty()) {
    LayoutUnit line_cross_size = lines_.front()->cross_size();
    if (min_cross_space && line_cross_size < *min_cross_space) {
      lines_.front()->set_cross_size(*min_cross_space);
    } else if (max_cross_space && line_cross_size > *max_cross_space) {
      lines_.front()->set_cross_size(*max_cross_space);
    }
  }

  // Note, this is numbered as step 15 in the spec, however it does not rely on
  // any calculations done during step 9..14, and the result is needed for
  // step 9 below.
  // 15. Determine the flex container's used cross size.
  LayoutUnit total_cross_size = LayoutUnit();
  for (auto& line : lines_) {
    total_cross_size += line->cross_size();
  }

  if (cross_space) {
    // If the cross size property is a definite size, use that.
    cross_size_ = *cross_space;
  } else {
    // Otherwise, use the sum of the flex lines' cross sizes.
    cross_size_ = total_cross_size;
  }
  // Clamped by the used min and max cross sizes of the flex container.
  if (min_cross_space && cross_size_ < *min_cross_space) {
    cross_size_ = *min_cross_space;
  } else if (max_cross_space && cross_size_ > *max_cross_space) {
    cross_size_ = *max_cross_space;
  }

  if (lines_.empty()) {
    return;
  }

  LayoutUnit leftover_cross_size = cross_size_ - total_cross_size;
  // 9. Handle 'align-content: stretch'.
  if (align_content == cssom::KeywordValue::GetStretch()) {
    if (leftover_cross_size > LayoutUnit()) {
      LayoutUnit leftover_cross_size_per_line =
          leftover_cross_size / static_cast<int>(lines_.size());
      for (auto& line : lines_) {
        line->set_cross_size(line->cross_size() + leftover_cross_size_per_line);
      }
      leftover_cross_size = LayoutUnit();
    }
  }

  // 10. Collapse visibility:collapse items.
  // Cobalt does not implement visibility:collapse.

  // 11. Determine the used cross size of each flex item.
  for (auto& line : lines_) {
    line->DetermineUsedCrossSizes(cross_size_);
  }

  // Main-Axis Alignment:
  // 12. Distribute any remaining free space.
  for (auto& line : lines_) {
    line->DoMainAxisAlignment();
  }

  // Cross-Axis Alignment:
  // 13. Resolve cross-axis auto margins.
  // 14. Align all flex items along the cross-axis.
  // 16. Align all flex lines per 'align-content'.

  LayoutUnit line_cross_axis_start = LayoutUnit();
  LayoutUnit leftover_cross_size_per_line = LayoutUnit();
  if (leftover_cross_size > LayoutUnit()) {
    if (align_content == cssom::KeywordValue::GetCenter()) {
      line_cross_axis_start = leftover_cross_size / 2;
    } else if (align_content == cssom::KeywordValue::GetFlexEnd()) {
      line_cross_axis_start = leftover_cross_size;
    } else if (align_content == cssom::KeywordValue::GetSpaceBetween()) {
      leftover_cross_size_per_line =
          (lines_.size() < 2)
              ? LayoutUnit()
              : leftover_cross_size / static_cast<int>(lines_.size() - 1);
    } else if (align_content == cssom::KeywordValue::GetSpaceAround()) {
      leftover_cross_size_per_line =
          leftover_cross_size / static_cast<int>(lines_.size());
      line_cross_axis_start = leftover_cross_size_per_line / 2;
    } else {
      DCHECK((align_content == cssom::KeywordValue::GetFlexStart()) ||
             (align_content == cssom::KeywordValue::GetStretch()));
    }
  }
  for (auto& line : lines_) {
    line->DoCrossAxisAlignment(line_cross_axis_start);
    line_cross_axis_start += line->cross_size() + leftover_cross_size_per_line;
  }
}

LayoutUnit FlexFormattingContext::GetBaseline() {
  LayoutUnit baseline = cross_size_;
  if (!lines_.empty()) {
    if (direction_is_reversed_ && !main_direction_is_horizontal_) {
      baseline = lines_.back()->GetBaseline();
    } else {
      baseline = lines_.front()->GetBaseline();
    }
  }
  return baseline;
}

}  // namespace layout
}  // namespace cobalt

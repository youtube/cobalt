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
      direction_is_reversed_(direction_is_reversed) {}

FlexFormattingContext::~FlexFormattingContext() {}

void FlexFormattingContext::UpdateRect(Box* child_box) {
  DCHECK(!child_box->IsAbsolutelyPositioned());
  child_box->UpdateSize(layout_params_);

  // Shrink-to-fit doesn't exists anymore by itself in CSS3. It is called the
  // fit-content size, which is derived from the 'min-content' and 'max-content'
  // sizes and the 'stretch-fit' size.
  //   https://www.w3.org/TR/css-sizing-3/#fit-content-size
  // which depends on the intrinsic sizes:
  //   https://www.w3.org/TR/css-flexbox-1/#intrinsic-sizes
  // Note that for column flex-direction, this is the intrinsic cross size.

  // TODO handle !main_direction_is_horizontal_
  set_shrink_to_fit_width(shrink_to_fit_width() + child_box->width() +
                          child_box->GetContentToMarginHorizontal());
  set_auto_height(child_box->height());
}

void FlexFormattingContext::EstimateStaticPosition(Box* child_box) {
  DCHECK(child_box->IsAbsolutelyPositioned());
  child_box->UpdateSize(layout_params_);
  // TODO set static position? Also, memoize because there may be multiple...
}

void FlexFormattingContext::CollectItemIntoLine(Box* item) {
  // Collect flex items into flex lines:
  //   https://www.w3.org/TR/css-flexbox-1/#algo-line-break
  if (lines_.empty()) {
    lines_.emplace_back(new FlexLine(layout_params_,
                                     main_direction_is_horizontal_,
                                     direction_is_reversed_, main_size_));
  }
  ItemParameters parameters = GetItemParameters(item);

  if (multi_line_) {
    if (lines_.back()->TryAddItem(item, parameters.flex_base_size,
                                  parameters.hypothetical_main_size)) {
      return;
    } else {
      lines_.emplace_back(new FlexLine(layout_params_,
                                       main_direction_is_horizontal_,
                                       direction_is_reversed_, main_size_));
    }
  }

  lines_.back()->AddItem(item, parameters.flex_base_size,
                         parameters.hypothetical_main_size);
}

void FlexFormattingContext::ResolveFlexibleLengthsAndCrossSizes(
    const base::Optional<LayoutUnit>& cross_space, LayoutUnit min_cross_space,
    const base::Optional<LayoutUnit>& max_cross_space,
    const scoped_refptr<cssom::PropertyValue>& align_content) {
  if (lines_.empty()) {
    return;
  }

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
  if (!multi_line_ && cross_space) {
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
  if (!multi_line_) {
    LayoutUnit line_cross_size = lines_.front()->cross_size();
    if (line_cross_size < min_cross_space) {
      lines_.front()->set_cross_size(min_cross_space);
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
  if (cross_size_ < min_cross_space) {
    cross_size_ = min_cross_space;
  } else if (max_cross_space && cross_size_ > *max_cross_space) {
    cross_size_ = *max_cross_space;
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

  LayoutUnit line_top = LayoutUnit();
  LayoutUnit leftover_cross_size_per_line = LayoutUnit();
  if (leftover_cross_size > LayoutUnit()) {
    if (align_content == cssom::KeywordValue::GetCenter()) {
      line_top = leftover_cross_size / 2;
    } else if (align_content == cssom::KeywordValue::GetFlexEnd()) {
      line_top = leftover_cross_size;
    } else if (align_content == cssom::KeywordValue::GetSpaceBetween()) {
      leftover_cross_size_per_line =
          (lines_.size() < 2)
              ? LayoutUnit()
              : leftover_cross_size / static_cast<int>(lines_.size() - 1);
    } else if (align_content == cssom::KeywordValue::GetSpaceAround()) {
      leftover_cross_size_per_line =
          leftover_cross_size / static_cast<int>(lines_.size());
      line_top = leftover_cross_size_per_line / 2;
    } else {
      DCHECK((align_content == cssom::KeywordValue::GetFlexStart()) ||
             (align_content == cssom::KeywordValue::GetStretch()));
    }
  }
  for (auto& line : lines_) {
    line->DoCrossAxisAlignment(line_top);
    line_top += line->cross_size() + leftover_cross_size_per_line;
  }
}

LayoutUnit FlexFormattingContext::GetBaseline() {
  if (!main_direction_is_horizontal_) {
    // TODO: implement this for column flex containers.
    NOTIMPLEMENTED() << "Column flex boxes not yet implemented.";
  }

  // TODO: Complete implementation of flex container baselines.
  //   https://www.w3.org/TR/css-flexbox-1/#flex-baselines

  LayoutUnit baseline = LayoutUnit();
  if (!lines_.empty()) {
    baseline = lines_.front()->GetBaseline();
  }
  return baseline;
}

}  // namespace layout
}  // namespace cobalt

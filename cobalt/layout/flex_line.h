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

#ifndef COBALT_LAYOUT_FLEX_LINE_H_
#define COBALT_LAYOUT_FLEX_LINE_H_

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/optional.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/flex_item.h"
#include "cobalt/layout/layout_unit.h"

namespace cobalt {
namespace layout {

class FlexLine {
 public:
  // Disallow Copy and Assign.
  FlexLine(const FlexLine&) = delete;
  FlexLine& operator=(const FlexLine&) = delete;

  FlexLine(const LayoutParams& layout_params, bool main_direction_is_horizontal,
           bool direction_is_reversed, LayoutUnit main_size);

  // Return whether the item to can be added to the line. Returns false if the
  // item does not fit.
  bool CanAddItem(const FlexItem& item) const;

  // Add the item to the line.
  void AddItem(std::unique_ptr<FlexItem>&& item);

  void ResolveFlexibleLengthsAndCrossSize();
  void CalculateCrossSize();
  void DetermineUsedCrossSizes(LayoutUnit container_cross_size);
  void DoMainAxisAlignment();
  void DoCrossAxisAlignment(LayoutUnit line_cross_axis_start);

  LayoutUnit cross_size() const { return cross_size_; }
  void set_cross_size(LayoutUnit value) { cross_size_ = value; }

  LayoutUnit GetBaseline();

  LayoutUnit items_outer_main_size() { return items_outer_main_size_; }

 private:
  LayoutUnit GetOuterMainSizeOfBox(Box* box,
                                   LayoutUnit content_main_size) const;
  float GetFlexFactor(Box* box);

  void SetMainAxisPosition(LayoutUnit pos, FlexItem* item);

  const LayoutParams layout_params_;
  const bool main_direction_is_horizontal_;
  const bool direction_is_reversed_;
  const LayoutUnit main_size_;

  bool flex_factor_grow_;
  LayoutUnit items_outer_main_size_;
  std::vector<std::unique_ptr<FlexItem>> items_;

  LayoutUnit cross_size_;
  base::Optional<LayoutUnit> max_baseline_to_top_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_FLEX_LINE_H_

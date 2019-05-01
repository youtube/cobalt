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

#include <vector>

#include "base/logging.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/layout_unit.h"

namespace cobalt {
namespace layout {

class FlexLine {
 public:
  struct Item {
    Box* box = nullptr;
    LayoutUnit flex_base_size = LayoutUnit();
    LayoutUnit hypothetical_main_size = LayoutUnit();
    LayoutUnit outer_main_size = LayoutUnit();
    LayoutUnit target_main_size = LayoutUnit();
    LayoutUnit flex_space = LayoutUnit();
    LayoutUnit scaled_flex_shrink_factor = LayoutUnit();
    float flex_factor = 0;
    bool max_violation = false;
    bool min_violation = false;
    Item(Box* box, LayoutUnit flex_base_size, LayoutUnit hypothetical_main_size,
         LayoutUnit outer_main_size)
        : box(box),
          flex_base_size(flex_base_size),
          hypothetical_main_size(hypothetical_main_size),
          outer_main_size(outer_main_size) {}
  };

  FlexLine(const LayoutParams& layout_params, bool main_direction_is_horizontal,
           bool direction_is_reversed, LayoutUnit main_size);

  // Attempt to add the item to the line. Returns true if the box was
  // added to the line.
  bool TryAddItem(Box* item, LayoutUnit flex_base_size,
                  LayoutUnit hypothetical_main_size);

  // Add the item to the line.
  void AddItem(Box* item, LayoutUnit flex_base_size,
               LayoutUnit hypothetical_main_size);

  void ResolveFlexibleLengthsAndCrossSize();
  void CalculateCrossSize();
  void DetermineUsedCrossSizes(LayoutUnit container_cross_size);
  void DoMainAxisAlignment();
  void DoCrossAxisAlignment(LayoutUnit line_top);

  LayoutUnit cross_size() const { return cross_size_; }
  void set_cross_size(LayoutUnit value) { cross_size_ = value; }

  LayoutUnit GetBaseline();

 private:
  LayoutUnit GetOuterMainSizeOfBox(Box* box,
                                   LayoutUnit content_main_size) const;
  float GetFlexFactor(Box* box);

  void SetBoxPosition(LayoutUnit pos, Box* box);

  const LayoutParams layout_params_;
  const bool main_direction_is_horizontal_;
  const bool direction_is_reversed_;
  const LayoutUnit main_size_;

  bool flex_factor_grow_;
  LayoutUnit items_outer_main_size_;
  LayoutUnit line_top_;
  std::vector<Item> items_;

  LayoutUnit cross_size_;

  DISALLOW_COPY_AND_ASSIGN(FlexLine);
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_FLEX_LINE_H_

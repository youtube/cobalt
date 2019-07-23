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

#ifndef COBALT_LAYOUT_FLEX_FORMATTING_CONTEXT_H_
#define COBALT_LAYOUT_FLEX_FORMATTING_CONTEXT_H_

#include <memory>
#include <vector>

#include "base/containers/hash_tables.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/flex_line.h"
#include "cobalt/layout/formatting_context.h"

namespace cobalt {
namespace layout {

class FlexFormattingContext : public FormattingContext {
 public:
  struct ItemParameters {
    LayoutUnit flex_base_size;
    LayoutUnit hypothetical_main_size;
  };

  FlexFormattingContext(const LayoutParams& layout_params,
                        bool main_direction_is_horizontal,
                        bool direction_is_reversed);
  ~FlexFormattingContext() override;

  // Calculates the position and size of the given child box and updates
  // the internal state in the preparation for the next child.
  void UpdateRect(Box* child_box);

  // Estimates the static position of the given child box. In CSS 2.1 the static
  // position is only defined for absolutely positioned boxes.
  void EstimateStaticPosition(Box* child_box);

  // Collects the flex item into a flex line.
  void CollectItemIntoLine(Box* item);

  // layout flex items and determine cross size.
  void ResolveFlexibleLengthsAndCrossSizes(
      const base::Optional<LayoutUnit>& cross_space, LayoutUnit min_cross_space,
      const base::Optional<LayoutUnit>& max_cross_space,
      const scoped_refptr<cssom::PropertyValue>& align_content);

  LayoutUnit main_size() const { return main_size_; }
  void set_main_size(LayoutUnit value) { main_size_ = value; }
  LayoutUnit cross_size() const { return cross_size_; }

  const LayoutParams& layout_params() const { return layout_params_; }

  const SizeLayoutUnit& containing_block_size() const {
    return layout_params_.containing_block_size;
  }

  const ItemParameters& GetItemParameters(Box* item) {
    return item_parameters_.find(item)->second;
  }

  void SetItemParameters(Box* item, const ItemParameters& parameters) {
    item_parameters_.insert(std::make_pair(item, parameters));
  }

  LayoutUnit GetFlexBaseSize(Box* item) {
    return GetItemParameters(item).flex_base_size;
  }

  LayoutUnit GetHypotheticalMainSize(Box* item) {
    return GetItemParameters(item).hypothetical_main_size;
  }

  void set_multi_line(bool val) { multi_line_ = val; }

  LayoutUnit GetBaseline();

 private:
  const LayoutParams layout_params_;
  const bool main_direction_is_horizontal_;
  const bool direction_is_reversed_;
  bool multi_line_ = false;

  LayoutUnit main_size_;
  LayoutUnit cross_size_;

  base::hash_map<Box*, ItemParameters> item_parameters_;
  std::vector<std::unique_ptr<FlexLine>> lines_;

  DISALLOW_COPY_AND_ASSIGN(FlexFormattingContext);
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_FLEX_FORMATTING_CONTEXT_H_

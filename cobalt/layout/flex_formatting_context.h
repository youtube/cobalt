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
  FlexFormattingContext(const LayoutParams& layout_params,
                        bool main_direction_is_horizontal,
                        bool direction_is_reversed);
  ~FlexFormattingContext() override;

  // Calculates the position and size of the given child box and updates
  // the internal state in the preparation for the next child.
  void UpdateRect(Box* child_box);

  // Collects the flex item into a flex line.
  void CollectItemIntoLine(LayoutUnit main_space,
                           std::unique_ptr<FlexItem>&& item);

  // layout flex items and determine cross size.
  void ResolveFlexibleLengthsAndCrossSizes(
      const base::Optional<LayoutUnit>& cross_space,
      const base::Optional<LayoutUnit>& min_cross_space,
      const base::Optional<LayoutUnit>& max_cross_space,
      const scoped_refptr<cssom::PropertyValue>& align_content);

  LayoutUnit cross_size() const { return cross_size_; }

  const LayoutParams& layout_params() const { return layout_params_; }

  const SizeLayoutUnit& containing_block_size() const {
    return layout_params_.containing_block_size;
  }

  void SetContainerMainSize(LayoutUnit size) {
    if (main_direction_is_horizontal_) {
      layout_params_.containing_block_size.set_width(size);
    } else {
      layout_params_.containing_block_size.set_height(size);
    }
  }

  void set_multi_line(bool val) { multi_line_ = val; }

  LayoutUnit GetBaseline();

  // Used to calculate the "auto" size in the main direction of the box that
  // establishes this formatting context.
  LayoutUnit fit_content_main_size() const { return fit_content_main_size_; }

 private:
  LayoutParams layout_params_;
  const bool main_direction_is_horizontal_;
  const bool direction_is_reversed_;
  bool multi_line_ = false;

  LayoutUnit cross_size_;
  LayoutUnit fit_content_main_size_;

  std::vector<std::unique_ptr<FlexLine>> lines_;

  DISALLOW_COPY_AND_ASSIGN(FlexFormattingContext);
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_FLEX_FORMATTING_CONTEXT_H_

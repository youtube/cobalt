// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_BLOCK_FORMATTING_CONTEXT_H_
#define COBALT_LAYOUT_BLOCK_FORMATTING_CONTEXT_H_

#include "base/optional.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/formatting_context.h"
#include "cobalt/layout/layout_unit.h"
#include "cobalt/math/point_f.h"

namespace cobalt {
namespace layout {

// In a block formatting context, boxes are laid out one after the other,
// vertically, beginning at the top of a containing block.
//   https://www.w3.org/TR/CSS21/visuren.html#block-formatting
//
// A block formatting context is a short-lived object that is constructed
// and destroyed during the layout. The block formatting context does not own
// child boxes nor triggers their layout - it is a responsibility of the box
// that establishes this formatting context. This class merely knows how
// to update the position of the subsequent children passed to it.
class BlockFormattingContext : public FormattingContext {
 public:
  explicit BlockFormattingContext(const LayoutParams& layout_params);
  ~BlockFormattingContext() override;

  // Calculates the position and size of the given child box and updates
  // the internal state in the preparation for the next child.
  void UpdateRect(Box* child_box);

  // Estimates the static position of the given child box. In CSS 2.1 the static
  // position is only defined for absolutely positioned boxes.
  void EstimateStaticPosition(Box* child_box);

 private:
  void UpdatePosition(Box* child_box);

  const LayoutParams layout_params_;
  LayoutUnit collapsing_margin_;

  DISALLOW_COPY_AND_ASSIGN(BlockFormattingContext);
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_BLOCK_FORMATTING_CONTEXT_H_

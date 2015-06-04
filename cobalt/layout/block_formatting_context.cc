/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/layout/block_formatting_context.h"

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/box.h"

namespace cobalt {
namespace layout {

BlockFormattingContext::BlockFormattingContext(
    const LayoutParams& layout_params)
    : layout_params_(layout_params), shrink_to_fit_width_(0) {}

BlockFormattingContext::~BlockFormattingContext() {}

void BlockFormattingContext::UpdateUsedRect(Box* child_box) {
  DCHECK_EQ(Box::kBlockLevel, child_box->GetLevel());

  child_box->UpdateUsedSizeIfInvalid(layout_params_);

  // In a block formatting context, boxes are laid out one after the other,
  // vertically, beginning at the top of a containing block.
  //   http://www.w3.org/TR/CSS21/visuren.html#block-formatting
  child_box->set_used_left(next_child_box_used_position_.x());
  child_box->set_used_top(next_child_box_used_position_.y());

  // If the position is absolute, then it should not affect the layout of
  // its siblings and thus we should not update the block formatting context's
  // state.
  //   http://www.w3.org/TR/CSS21/visuren.html#absolute-positioning
  // TODO(***REMOVED***): A position of "fixed" is also out-of-flow and should not
  //               affect the block formatting context's state.
  if (child_box->computed_style()->position() !=
          cssom::KeywordValue::GetAbsolute()) {
    next_child_box_used_position_.Offset(0, child_box->used_height());

    // The vertical distance between two sibling boxes is determined by
    // the "margin" properties. Vertical margins between adjacent block-level
    // boxes collapse.
    //   http://www.w3.org/TR/CSS21/visuren.html#block-formatting
    // TODO(***REMOVED***): Implement the above.

    // Shrink-to-fit width cannot be less than the width of the widest child.
    //   http://www.w3.org/TR/CSS21/visudet.html#float-width
    shrink_to_fit_width_ =
        std::max(shrink_to_fit_width_, child_box->used_width());

    // The baseline of an "inline-block" is the baseline of its last line box
    // in the normal flow, unless it has no in-flow line boxes.
    //   http://www.w3.org/TR/CSS21/visudet.html#line-height
    if (child_box->AffectsBaselineInBlockFormattingContext()) {
      set_height_above_baseline(child_box->used_top() +
                                child_box->GetHeightAboveBaseline());
    }
  }
}

}  // namespace layout
}  // namespace cobalt

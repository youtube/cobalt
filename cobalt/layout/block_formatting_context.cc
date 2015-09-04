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
    : layout_params_(layout_params) {}

BlockFormattingContext::~BlockFormattingContext() {}

void BlockFormattingContext::UpdateUsedRect(Box* child_box) {
  DCHECK_EQ(Box::kBlockLevel, child_box->GetLevel());

  // In a block formatting context, boxes are laid out one after the other,
  // vertically, beginning at the top of a containing block.
  //   http://www.w3.org/TR/CSS21/visuren.html#block-formatting
  child_box->set_left(0);
  child_box->set_top(used_height());

  // If the position is absolute, then it should not affect the layout of
  // its siblings and thus we should not update the block formatting context's
  // state.
  //   http://www.w3.org/TR/CSS21/visuren.html#absolute-positioning
  // TODO(***REMOVED***): A position of "fixed" is also out-of-flow and should not
  //               affect the block formatting context's state.
  if (child_box->computed_style()->position() !=
          cssom::KeywordValue::GetAbsolute()) {
    child_box->UpdateSize(layout_params_);

    bounding_box_of_used_children_.set_height(used_height() +
                                              child_box->height());

    // The vertical distance between two sibling boxes is determined by
    // the "margin" properties. Vertical margins between adjacent block-level
    // boxes collapse.
    //   http://www.w3.org/TR/CSS21/visuren.html#block-formatting
    // TODO(***REMOVED***): Implement the above.

    // Shrink-to-fit width cannot be less than the width of the widest child.
    //   http://www.w3.org/TR/CSS21/visudet.html#float-width
    bounding_box_of_used_children_.set_width(
        std::max(used_width(), child_box->width()));

    // The baseline of an "inline-block" is the baseline of its last line box
    // in the normal flow, unless it has no in-flow line boxes.
    //   http://www.w3.org/TR/CSS21/visudet.html#line-height
    if (child_box->AffectsBaselineInBlockFormattingContext()) {
      set_baseline_offset_from_top_content_edge(
          child_box->top() + child_box->GetBaselineOffsetFromTopMarginEdge());
    }
  }
}

}  // namespace layout
}  // namespace cobalt

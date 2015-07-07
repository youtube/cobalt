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

#include "cobalt/layout/inline_formatting_context.h"

#include <algorithm>

#include "base/logging.h"
#include "cobalt/layout/line_box.h"

namespace cobalt {
namespace layout {

InlineFormattingContext::InlineFormattingContext(
    const LayoutParams& layout_params, float x_height,
    const scoped_refptr<cssom::PropertyValue>& text_align)
    : layout_params_(layout_params),
      line_count_(0),
      x_height_(x_height),
      text_align_(text_align) {}

InlineFormattingContext::~InlineFormattingContext() {}

scoped_ptr<Box> InlineFormattingContext::QueryUsedRectAndMaybeSplit(
    Box* child_box) {
  DCHECK_EQ(Box::kInlineLevel, child_box->GetLevel());

  scoped_ptr<Box> child_box_after_split;
  if (line_box_ &&
      line_box_->TryQueryUsedRectAndMaybeSplit(child_box,
                                               &child_box_after_split)) {
    return child_box_after_split.Pass();
  }

  // When an inline box exceeds the width of a line box, it is split into
  // several boxes and these boxes are distributed across several line boxes.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  //
  // We tackle this problem one split (and one line) at the time.

  // Create the new line box and overwrite the previous one.
  //
  // Line boxes are stacked with no vertical separation and they never
  // overlap.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  OnLineBoxDestroying();
  line_box_ = make_scoped_ptr(new LineBox(used_height(), x_height_,
                                          LineBox::kShouldTrimWhiteSpace,
                                          text_align_, layout_params_));

  // A sequence of collapsible spaces at the beginning of a line is removed.
  //   http://www.w3.org/TR/css3-text/#white-space-phase-2
  child_box->CollapseLeadingWhiteSpace();

  if (!line_box_->TryQueryUsedRectAndMaybeSplit(child_box,
                                                &child_box_after_split)) {
    // If an inline box cannot be split (e.g., if the inline box contains
    // a single character, or language specific word breaking rules disallow
    // a break within the inline box), then the inline box overflows the line
    // box.
    //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
    line_box_->QueryUsedRectAndMaybeOverflow(child_box);
  }

  return child_box_after_split.Pass();
}

void InlineFormattingContext::EndQueries() {
  // Treat the end of child boxes almost as an explicit line break,
  // but don't create the new line box.
  OnLineBoxDestroying();
}

void InlineFormattingContext::OnLineBoxDestroying() {
  if (line_box_) {
    line_box_->EndQueries();

    // The baseline of an "inline-block" is the baseline of its last line box
    // in the normal flow, unless it has no in-flow line boxes.
    //   http://www.w3.org/TR/CSS21/visudet.html#line-height
    if (line_box_->line_exists()) {
      ++line_count_;

      // Set height as the bottom edge of the last line box
      // http://www.w3.org/TR/CSS21/visudet.html#normal-block
      // TODO(***REMOVED***): Handle margins and line spacing correctly.
      bounding_box_of_used_children_.set_height(line_box_->used_top() +
                                                line_box_->used_height());

      set_height_above_baseline(line_box_->used_top() +
                                line_box_->height_above_baseline());

      // A width of the block container box when all possible line breaks are
      // made.
      float preferred_min_width =
          std::max(used_width(), line_box_->GetShrinkToFitWidth());
      float shrink_to_fit_width = std::max(
          preferred_min_width,
          line_count_ > 1 ? layout_params_.containing_block_size.width() : 0);
      bounding_box_of_used_children_.set_width(shrink_to_fit_width);
    }
  }
}

}  // namespace layout
}  // namespace cobalt

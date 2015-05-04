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

#include "base/logging.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/line_box.h"

namespace cobalt {
namespace layout {

InlineFormattingContext::InlineFormattingContext(float containing_block_width)
    : containing_block_width_(containing_block_width),
      preferred_min_width_(0),
      line_count_(0) {}

InlineFormattingContext::~InlineFormattingContext() {}

scoped_ptr<Box> InlineFormattingContext::QueryUsedPositionAndMaybeSplit(
    Box* child_box) {
  DCHECK_EQ(Box::kInlineLevel, child_box->GetLevel());

  scoped_ptr<Box> child_box_after_split;
  if (line_box_ &&
      line_box_->TryQueryUsedPositionAndMaybeSplit(child_box,
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
  line_box_ = make_scoped_ptr(
      new LineBox(GetLastLineBoxUsedBottom(), containing_block_width_));

  // A sequence of collapsible spaces at the beginning of a line is removed.
  //   http://www.w3.org/TR/css3-text/#white-space-phase-2
  child_box->CollapseLeadingWhiteSpace();

  if (!line_box_->TryQueryUsedPositionAndMaybeSplit(child_box,
                                                    &child_box_after_split)) {
    // If an inline box cannot be split (e.g., if the inline box contains
    // a single character, or language specific word breaking rules disallow
    // a break within the inline box), then the inline box overflows the line
    // box.
    //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
    line_box_->QueryUsedPositionAndMaybeOverflow(child_box);
  }
  return child_box_after_split.Pass();
}

void InlineFormattingContext::EndQueries() {
  // Treat the end of child boxes almost as an explicit line break,
  // but don't create the new line box.
  OnLineBoxDestroying();
}

float InlineFormattingContext::GetShrinkToFitWidth() const {
  // The shrink-to-fit width is:
  // min(max(preferred minimum width, available width), preferred width).
  //   http://www.w3.org/TR/CSS21/visudet.html#float-width
  //
  // Naive solution of the above expression would require two layout passes:
  // one to calculate the "preferred minimum width" and another one to
  // calculate the "preferred width". It is possible to save one layout pass
  // taking into account that:
  //   - an exact value of "preferred width" does not matter if "available
  //     width" cannot acommodate it;
  //   - the inline formatting context has more than one line if and only if
  //     the "preferred width" is greater than the "available width";
  //   - "preferred minimum" and "preferred" widths are equal when an inline
  //     formatting context has only one line.
  return std::max(preferred_min_width_,
                  line_count_ > 1 ? containing_block_width_ : 0);
}

float InlineFormattingContext::GetLastLineBoxUsedBottom() const {
  return line_box_
             ? line_box_->used_top() +
                   (line_box_->line_exists() ? line_box_->used_height() : 0)
             : 0;
}

void InlineFormattingContext::OnLineBoxDestroying() {
  if (line_box_) {
    line_box_->EndQueries();

    // The baseline of an "inline-block" is the baseline of its last line box
    // in the normal flow, unless it has no in-flow line boxes.
    //   http://www.w3.org/TR/CSS21/visudet.html#line-height
    if (line_box_->line_exists()) {
      ++line_count_;

      height_above_baseline_ =
          line_box_->used_top() + line_box_->height_above_baseline();

      preferred_min_width_ =
          std::max(preferred_min_width_, line_box_->GetShrinkToFitWidth());
    }
  }
}

}  // namespace layout
}  // namespace cobalt

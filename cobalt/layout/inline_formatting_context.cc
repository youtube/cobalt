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
    const scoped_refptr<cssom::PropertyValue>& line_height,
    const render_tree::FontMetrics& font_metrics,
    const LayoutParams& layout_params,
    const scoped_refptr<cssom::PropertyValue>& text_align)
    : line_height_(line_height),
      font_metrics_(font_metrics),
      layout_params_(layout_params),
      text_align_(text_align),
      line_count_(0),
      preferred_min_width_(0) {
  CreateLineBox();
}

InlineFormattingContext::~InlineFormattingContext() {}

scoped_refptr<Box> InlineFormattingContext::BeginUpdateRectAndMaybeSplit(
    Box* child_box) {
  DCHECK(child_box->GetLevel() == Box::kInlineLevel ||
         child_box->IsAbsolutelyPositioned());

  // When an inline box exceeds the width of a line box, it is split into
  // several boxes and these boxes are distributed across several line boxes.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  //
  // We tackle this problem one split (and one line) at the time.
  scoped_refptr<Box> child_box_after_split;
  if (!TryBeginUpdateRectAndMaybeCreateLineBox(child_box,
                                               &child_box_after_split)) {
    DCHECK(!child_box_after_split) << "A split should only occur when a child "
                                      "before the split is accepted by a line "
                                      "box.";
    bool child_box_update_began = TryBeginUpdateRectAndMaybeCreateLineBox(
        child_box, &child_box_after_split);
    DCHECK(child_box_update_began)
        << "A line box should never reject the first child.";
  }
  return child_box_after_split;
}

void InlineFormattingContext::BeginEstimateStaticPosition(Box* child_box) {
  line_box_->BeginEstimateStaticPosition(child_box);
}

void InlineFormattingContext::EndUpdates() {
  // Treat the end of child boxes almost as an explicit line break,
  // but don't create the new line box.
  DestroyLineBox();

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
  set_shrink_to_fit_width(std::max(
      preferred_min_width_,
      line_count_ > 1 ? layout_params_.containing_block_size.width() : 0));
}

bool InlineFormattingContext::TryBeginUpdateRectAndMaybeCreateLineBox(
    Box* child_box, scoped_refptr<Box>* child_box_after_split) {
  bool child_box_update_began = line_box_->TryBeginUpdateRectAndMaybeSplit(
      child_box, child_box_after_split);
  DCHECK(child_box_update_began || *child_box_after_split == NULL);

  if (!child_box_update_began) {
    CreateLineBox();
  }
  return child_box_update_began;
}

void InlineFormattingContext::CreateLineBox() {
  if (line_box_) {
    DestroyLineBox();
  }

  // Line boxes are stacked with no vertical separation and they never
  // overlap.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  line_box_ =
      make_scoped_ptr(new LineBox(auto_height(), line_height_, font_metrics_,
                                  true, true, layout_params_, text_align_));
}

void InlineFormattingContext::DestroyLineBox() {
  line_box_->EndUpdates();

  // The baseline of an "inline-block" is the baseline of its last line box
  // in the normal flow, unless it has no in-flow line boxes.
  //   http://www.w3.org/TR/CSS21/visudet.html#line-height
  if (line_box_->line_exists()) {
    ++line_count_;

    preferred_min_width_ =
        std::max(preferred_min_width_, line_box_->shrink_to_fit_width());

    // If "height" is "auto", the used value is the distance from box's top
    // content edge to the bottom edge of the last line box, if the box
    // establishes an inline formatting context with one or more lines.
    //   http://www.w3.org/TR/CSS21/visudet.html#normal-block
    set_auto_height(line_box_->top() + line_box_->height());

    set_baseline_offset_from_top_content_edge(
        line_box_->top() + line_box_->baseline_offset_from_top());
  }

  line_box_.reset();
}

}  // namespace layout
}  // namespace cobalt

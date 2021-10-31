// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/layout/inline_formatting_context.h"

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/line_box.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

InlineFormattingContext::InlineFormattingContext(
    const scoped_refptr<cssom::PropertyValue>& line_height,
    const render_tree::FontMetrics& font_metrics,
    const LayoutParams& layout_params, BaseDirection base_direction,
    const scoped_refptr<cssom::PropertyValue>& text_align,
    const scoped_refptr<cssom::PropertyValue>& font_size,
    LayoutUnit text_indent_offset, LayoutUnit ellipsis_width)
    : line_height_(line_height),
      font_metrics_(font_metrics),
      layout_params_(layout_params),
      base_direction_(base_direction),
      text_align_(text_align),
      font_size_(font_size),
      text_indent_offset_(text_indent_offset),
      ellipsis_width_(ellipsis_width),
      line_count_(0) {
  CreateLineBox();
}

InlineFormattingContext::~InlineFormattingContext() {}

Box* InlineFormattingContext::TryAddChildAndMaybeWrap(Box* child_box) {
  // When an inline box exceeds the width of a line box, it is split into
  // several boxes and these boxes are distributed across several line boxes.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  //
  // We tackle this problem one line at a time.

  Box* child_box_before_wrap = line_box_->TryAddChildAndMaybeWrap(child_box);
  // If |child_box_before_wrap| is non-NULL, then a line wrap has occurred and a
  // new line box must be created.
  if (child_box_before_wrap) {
    CreateLineBox();
  }
  return child_box_before_wrap;
}

void InlineFormattingContext::EndUpdates() {
  // Treat the end of child boxes almost as an explicit line break,
  // but don't create the new line box.
  DestroyLineBox();

  // The shrink-to-fit width is:
  // min(max(preferred minimum width, available width), preferred width).
  //   https://www.w3.org/TR/CSS21/visudet.html#float-width
  //
  // Naive solution of the above expression would require two layout passes:
  // one to calculate the "preferred minimum width" and another one to
  // calculate the "preferred width". It is possible to save one layout pass
  // taking into account that:
  //   - an exact value of "preferred width" does not matter if "available
  //     width" cannot accommodate it;
  //   - the inline formatting context has more than one line if and only if
  //     the "preferred width" is greater than the "available width";
  //   - "preferred minimum" and "preferred" widths are equal when an inline
  //     formatting context has only one line.
  set_shrink_to_fit_width(std::max(
      preferred_min_width_, line_count_ > 1
                                ? layout_params_.containing_block_size.width()
                                : LayoutUnit()));
}

const std::vector<math::Vector2dF>&
InlineFormattingContext::GetEllipsesCoordinates() const {
  return ellipses_coordinates_;
}

void InlineFormattingContext::CreateLineBox() {
  if (line_box_) {
    DestroyLineBox();
  }

  // "'Text-indent' only affects a line if it is the first formatted line of an
  // element."
  //   https://www.w3.org/TR/CSS21/text.html#propdef-text-indent
  LayoutUnit line_indent_offset =
      line_count_ == 0 ? text_indent_offset_ : LayoutUnit();

  // Line boxes are stacked with no vertical separation and they never
  // overlap.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  line_box_ = base::WrapUnique(
      new LineBox(auto_height(), false, line_height_, font_metrics_, true, true,
                  layout_params_, base_direction_, text_align_, font_size_,
                  line_indent_offset, ellipsis_width_));
}

void InlineFormattingContext::DestroyLineBox() {
  line_box_->EndUpdates();

  // The baseline of an "inline-block" is the baseline of its last line box
  // in the normal flow, unless it has no in-flow line boxes.
  //   https://www.w3.org/TR/CSS21/visudet.html#line-height
  if (line_box_->LineExists()) {
    ++line_count_;

    preferred_min_width_ =
        std::max(preferred_min_width_, line_box_->shrink_to_fit_width());

    // If "height" is "auto", the used value is the distance from box's top
    // content edge to the bottom edge of the last line box, if the box
    // establishes an inline formatting context with one or more lines.
    //   https://www.w3.org/TR/CSS21/visudet.html#normal-block
    set_auto_height(line_box_->top() + line_box_->height());

    set_baseline_offset_from_top_content_edge(
        line_box_->top() + line_box_->baseline_offset_from_top());

    if (line_box_->IsEllipsisPlaced()) {
      ellipses_coordinates_.push_back(line_box_->GetEllipsisCoordinates());
    }
  }

  line_box_.reset();
}

}  // namespace layout
}  // namespace cobalt

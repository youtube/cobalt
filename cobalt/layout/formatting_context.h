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

#ifndef LAYOUT_FORMATTING_CONTEXT_H_
#define LAYOUT_FORMATTING_CONTEXT_H_

#include "base/optional.h"
#include "cobalt/math/size_f.h"

namespace cobalt {
namespace layout {

// A base class for block and inline formatting contexts.
class FormattingContext {
 public:
  FormattingContext() : shrink_to_fit_width_(0), auto_height_(0) {}
  virtual ~FormattingContext() {}

  // Used to calculate the "auto" width of the box that establishes this
  // formatting context.
  float shrink_to_fit_width() const { return shrink_to_fit_width_; }

  // Used to calculate the "auto" height of the box that establishes this
  // formatting context.
  float auto_height() const { return auto_height_; }

  // A vertical offset of the baseline relatively to the origin of the block
  // container box.
  //
  // In a block formatting context this is the baseline of the last child box
  // that has one. Disengaged, if none of the child boxes have a baseline.
  //
  // In an inline formatting context this is the baseline of the last line box.
  // Disengaged, if there are no line boxes that affect the layout (for example,
  // empty line boxes are discounted).
  const base::optional<float>& maybe_baseline_offset_from_top_content_edge()
      const {
    return maybe_baseline_offset_from_top_content_edge_;
  }

 protected:
  void set_shrink_to_fit_width(float shrink_to_fit_width) {
    shrink_to_fit_width_ = shrink_to_fit_width;
  }

  void set_auto_height(float auto_height) { auto_height_ = auto_height; }

  void set_baseline_offset_from_top_content_edge(
      float baseline_offset_from_top_content_edge) {
    maybe_baseline_offset_from_top_content_edge_ =
        baseline_offset_from_top_content_edge;
  }

 private:
  float shrink_to_fit_width_;
  float auto_height_;
  base::optional<float> maybe_baseline_offset_from_top_content_edge_;

  DISALLOW_COPY_AND_ASSIGN(FormattingContext);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_FORMATTING_CONTEXT_H_

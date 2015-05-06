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

namespace cobalt {
namespace layout {

// A base class for block and inline formatting contexts.
class FormattingContext {
 public:
  FormattingContext() {}
  virtual ~FormattingContext() {}

  // A vertical offset of the baseline relatively to the origin of the block
  // container box.
  //
  // In a block formatting context this is the baseline of the last child box
  // that has one. Disengaged, if none of the child boxes have a baseline.
  //
  // In an inline formatting context this is the baseline of the last line box.
  // Disengaged, if there are no line boxes that affect the layout (for example,
  // empty line boxes are discounted).
  base::optional<float> maybe_height_above_baseline() const {
    return maybe_height_above_baseline_;
  }

 protected:
  void set_height_above_baseline(float height_above_baseline) {
    maybe_height_above_baseline_ = height_above_baseline;
  }

 private:
  base::optional<float> maybe_height_above_baseline_;

  DISALLOW_COPY_AND_ASSIGN(FormattingContext);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_FORMATTING_CONTEXT_H_

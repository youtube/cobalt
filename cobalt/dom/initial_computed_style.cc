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

#include "cobalt/dom/initial_computed_style.h"

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/rgba_color_value.h"

namespace cobalt {
namespace dom {

scoped_refptr<cssom::CSSComputedStyleData> CreateInitialComputedStyle(
    const math::Size& viewport_size) {
  scoped_refptr<cssom::MutableCSSComputedStyleData>
      initial_containing_block_computed_style =
          new cssom::MutableCSSComputedStyleData();
  initial_containing_block_computed_style->set_background_color(
      new cssom::RGBAColorValue(0x00000000));
  initial_containing_block_computed_style->set_display(
      cssom::KeywordValue::GetBlock());
  initial_containing_block_computed_style->set_is_inline_before_blockification(
      false);
  initial_containing_block_computed_style->set_width(new cssom::LengthValue(
      static_cast<float>(viewport_size.width()), cssom::kPixelsUnit));
  initial_containing_block_computed_style->set_height(new cssom::LengthValue(
      static_cast<float>(viewport_size.height()), cssom::kPixelsUnit));

  return initial_containing_block_computed_style;
}

}  // namespace dom
}  // namespace cobalt

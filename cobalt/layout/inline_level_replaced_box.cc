/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/layout/inline_level_replaced_box.h"

#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

InlineLevelReplacedBox::InlineLevelReplacedBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider,
    const ReplaceImageCB& replace_image_cb,
    const scoped_refptr<Paragraph>& paragraph, int32 text_position,
    const base::optional<float>& maybe_intrinsic_width,
    const base::optional<float>& maybe_intrinsic_height,
    const base::optional<float>& maybe_intrinsic_ratio)
    : ReplacedBox(computed_style, transitions, used_style_provider,
                  replace_image_cb, paragraph, text_position,
                  maybe_intrinsic_width, maybe_intrinsic_height,
                  maybe_intrinsic_ratio) {}

Box::Level InlineLevelReplacedBox::GetLevel() const { return kInlineLevel; }

void InlineLevelReplacedBox::UpdateHorizontalMargins(
    float containing_block_width, float border_box_width,
    const base::optional<float>& maybe_margin_left,
    const base::optional<float>& maybe_margin_right) {
  UNREFERENCED_PARAMETER(containing_block_width);
  UNREFERENCED_PARAMETER(border_box_width);

  // A computed value of "auto" for "margin-left" or "margin-right" becomes
  // a used value of "0".
  //   http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
  set_margin_left(maybe_margin_left.value_or(0.0f));
  set_margin_right(maybe_margin_right.value_or(0.0f));
}

#ifdef COBALT_BOX_DUMP_ENABLED

void InlineLevelReplacedBox::DumpClassName(std::ostream* stream) const {
  *stream << "InlineLevelReplacedBox ";
}

#endif  // COBALT_BOX_DUMP_ENABLED

}  // namespace layout
}  // namespace cobalt

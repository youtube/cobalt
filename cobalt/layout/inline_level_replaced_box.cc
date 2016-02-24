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
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        css_computed_style_declaration,
    const ReplaceImageCB& replace_image_cb,
    const scoped_refptr<Paragraph>& paragraph, int32 text_position,
    const base::optional<float>& maybe_intrinsic_width,
    const base::optional<float>& maybe_intrinsic_height,
    const base::optional<float>& maybe_intrinsic_ratio,
    UsedStyleProvider* used_style_provider)
    : ReplacedBox(css_computed_style_declaration, replace_image_cb, paragraph,
                  text_position, maybe_intrinsic_width, maybe_intrinsic_height,
                  maybe_intrinsic_ratio, used_style_provider),
      is_hidden_by_ellipsis_(false) {}

Box::Level InlineLevelReplacedBox::GetLevel() const { return kInlineLevel; }

bool InlineLevelReplacedBox::DoesFulfillEllipsisPlacementRequirement() const {
  // This box fulfills the requirement that the first character or inline-level
  // element must appear on the line before ellipsing can occur
  // (https://www.w3.org/TR/css3-ui/#propdef-text-overflow).
  return true;
}

void InlineLevelReplacedBox::ResetEllipses() { is_hidden_by_ellipsis_ = false; }

bool InlineLevelReplacedBox::IsHiddenByEllipsis() const {
  return is_hidden_by_ellipsis_;
}

void InlineLevelReplacedBox::UpdateHorizontalMargins(
    float containing_block_width, float border_box_width,
    const base::optional<float>& maybe_margin_left,
    const base::optional<float>& maybe_margin_right) {
  UNREFERENCED_PARAMETER(containing_block_width);
  UNREFERENCED_PARAMETER(border_box_width);

  // A computed value of "auto" for "margin-left" or "margin-right" becomes
  // a used value of "0".
  //   https://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
  set_margin_left(maybe_margin_left.value_or(0.0f));
  set_margin_right(maybe_margin_right.value_or(0.0f));
}

#ifdef COBALT_BOX_DUMP_ENABLED

void InlineLevelReplacedBox::DumpClassName(std::ostream* stream) const {
  *stream << "InlineLevelReplacedBox ";
}

#endif  // COBALT_BOX_DUMP_ENABLED

void InlineLevelReplacedBox::DoPlaceEllipsisOrProcessPlacedEllipsis(
    float /*desired_offset*/, bool* is_placement_requirement_met,
    bool* is_placed, float* placed_offset) {
  // If the ellipsis is already placed, then simply mark the box as hidden by
  // the ellipsis: "Implementations must hide characters and atomic inline-level
  // elements at the applicable edge(s) of the line as necessary to fit the
  // ellipsis."
  //   https://www.w3.org/TR/css3-ui/#propdef-text-overflow
  if (*is_placed) {
    is_hidden_by_ellipsis_ = true;
    // Otherwise, the box is placing the ellipsis.
  } else {
    *is_placed = true;

    // The first character or atomic inline-level element on a line must be
    // clipped rather than ellipsed.
    //   https://www.w3.org/TR/css3-ui/#propdef-text-overflow
    // If this requirement has been met, then place the ellipsis to the left of
    // the atomic inline-level element, as it should be fully hidden.
    if (*is_placement_requirement_met) {
      *placed_offset = left();
      is_hidden_by_ellipsis_ = true;
      // Otherwise, this box is fulfilling the required first inline-level
      // element and the ellipsis must be added after the box, rather than
      // before it.
    } else {
      *placed_offset = GetMarginBoxRightEdgeOffsetFromContainingBlock();
    }
  }
}

}  // namespace layout
}  // namespace cobalt

// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/layout/block_level_replaced_box.h"

#include "cobalt/layout/used_style.h"
#include "cobalt/math/size_f.h"

namespace cobalt {
namespace layout {

BlockLevelReplacedBox::BlockLevelReplacedBox(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        css_computed_style_declaration,
    const ReplaceImageCB& replace_image_cb, const SetBoundsCB& set_bounds_cb,
    const scoped_refptr<Paragraph>& paragraph, int32 text_position,
    const base::optional<LayoutUnit>& maybe_intrinsic_width,
    const base::optional<LayoutUnit>& maybe_intrinsic_height,
    const base::optional<float>& maybe_intrinsic_ratio,
    UsedStyleProvider* used_style_provider,
    base::optional<bool> is_video_punched_out, const math::SizeF& content_size,
    LayoutStatTracker* layout_stat_tracker)
    : ReplacedBox(css_computed_style_declaration, replace_image_cb,
                  set_bounds_cb, paragraph, text_position,
                  maybe_intrinsic_width, maybe_intrinsic_height,
                  maybe_intrinsic_ratio, used_style_provider,
                  is_video_punched_out, content_size, layout_stat_tracker) {}

Box::Level BlockLevelReplacedBox::GetLevel() const { return kBlockLevel; }

void BlockLevelReplacedBox::UpdateHorizontalMargins(
    LayoutUnit containing_block_width, LayoutUnit border_box_width,
    const base::optional<LayoutUnit>& maybe_margin_left,
    const base::optional<LayoutUnit>& maybe_margin_right) {
  // Calculate the horizonal margins for block-level, replaced elements in
  // normal flow.
  //   https://www.w3.org/TR/CSS21/visudet.html#block-replaced-width
  UpdateHorizontalMarginsAssumingBlockLevelInFlowBox(
      containing_block_width, border_box_width, maybe_margin_left,
      maybe_margin_right);
}

#ifdef COBALT_BOX_DUMP_ENABLED

void BlockLevelReplacedBox::DumpClassName(std::ostream* stream) const {
  *stream << "BlockLevelReplacedBox ";
}

#endif  // COBALT_BOX_DUMP_ENABLED

}  // namespace layout
}  // namespace cobalt

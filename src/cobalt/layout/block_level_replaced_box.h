// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_BLOCK_LEVEL_REPLACED_BOX_H_
#define COBALT_LAYOUT_BLOCK_LEVEL_REPLACED_BOX_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/css_transition_set.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/paragraph.h"
#include "cobalt/layout/replaced_box.h"

namespace cobalt {
namespace layout {

class BlockLevelReplacedBox : public ReplacedBox {
 public:
  BlockLevelReplacedBox(
      const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
          css_computed_style_declaration,
      const ReplaceImageCB& replace_image_cb, const SetBoundsCB& set_bounds_cb,
      const scoped_refptr<Paragraph>& paragraph, int32 text_position,
      const base::optional<LayoutUnit>& maybe_intrinsic_width,
      const base::optional<LayoutUnit>& maybe_intrinsic_height,
      const base::optional<float>& maybe_intrinsic_ratio,
      UsedStyleProvider* used_style_provider,
      base::optional<bool> is_video_punched_out,
      LayoutStatTracker* layout_stat_tracker);

  // From |Box|.
  Level GetLevel() const OVERRIDE;

 protected:
  // From |Box|.
#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpClassName(std::ostream* stream) const OVERRIDE;
#endif  // COBALT_BOX_DUMP_ENABLED

  // From |ReplacedBox|.
  void UpdateHorizontalMargins(
      LayoutUnit containing_block_width, LayoutUnit border_box_width,
      const base::optional<LayoutUnit>& maybe_margin_left,
      const base::optional<LayoutUnit>& maybe_margin_right) OVERRIDE;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_BLOCK_LEVEL_REPLACED_BOX_H_

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

#ifndef LAYOUT_INLINE_LEVEL_REPLACED_BOX_H_
#define LAYOUT_INLINE_LEVEL_REPLACED_BOX_H_

#include "cobalt/layout/replaced_box.h"

namespace cobalt {
namespace layout {

class InlineLevelReplacedBox : public ReplacedBox {
 public:
  InlineLevelReplacedBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider,
      const ReplaceImageCB& replace_image_cb,
      const scoped_refptr<Paragraph>& paragraph, int32 text_position,
      const base::optional<float>& maybe_intrinsic_width,
      const base::optional<float>& maybe_intrinsic_height,
      const base::optional<float>& maybe_intrinsic_ratio);

  // From |Box|.
  Level GetLevel() const OVERRIDE;

 protected:
  // From |Box|.
  void UpdateContentSizeAndMargins(const LayoutParams& layout_params) OVERRIDE;

  void DumpClassName(std::ostream* stream) const OVERRIDE;
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_INLINE_LEVEL_REPLACED_BOX_H_

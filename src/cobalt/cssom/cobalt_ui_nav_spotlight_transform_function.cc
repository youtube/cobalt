// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/cobalt_ui_nav_spotlight_transform_function.h"

#include "cobalt/cssom/transform_function_visitor.h"

namespace cobalt {
namespace cssom {

CobaltUiNavSpotlightTransformFunction::CobaltUiNavSpotlightTransformFunction(
    float progress_to_identity)
    : progress_to_identity_(progress_to_identity) {
  traits_ = kTraitIsDynamic | kTraitUsesUiNavFocus;
}

void CobaltUiNavSpotlightTransformFunction::Accept(
    TransformFunctionVisitor* visitor) const {
  visitor->VisitCobaltUiNavSpotlightTransform(this);
}

std::string CobaltUiNavSpotlightTransformFunction::ToString() const {
  return "-cobalt-ui-nav-spotlight-transform()";
}

math::Matrix3F CobaltUiNavSpotlightTransformFunction::ToMatrix(
    const math::SizeF& used_size,
    const scoped_refptr<ui_navigation::NavItem>& used_ui_nav_focus) const {
  float scale = progress_to_identity_;
  float focus_x = 0.0f, focus_y = 0.0f;
  if ((used_ui_nav_focus &&
       used_ui_nav_focus->GetFocusVector(&focus_x, &focus_y)) ||
      (!used_ui_nav_focus &&
       ui_navigation::NavItem::GetGlobalFocusVector(&focus_x, &focus_y))) {
    // Translation must be mapped from [-1,+1] to [-50%,+50%].
    focus_x *= (1.0f - progress_to_identity_) * 0.5f * used_size.width();
    focus_y *= (1.0f - progress_to_identity_) * 0.5f * used_size.height();
    scale = 1.0f;
  }
  return math::Matrix3F::FromValues(scale, 0.0f, focus_x, 0.0f, scale, focus_y,
                                    0.0f, 0.0f, 1.0f);
}

}  // namespace cssom
}  // namespace cobalt

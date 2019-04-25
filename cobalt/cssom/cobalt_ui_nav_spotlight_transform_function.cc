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

#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/translate_function.h"

namespace cobalt {
namespace cssom {

CobaltUiNavSpotlightTransformFunction::CobaltUiNavSpotlightTransformFunction()
    : zero_scale_(0.0f, 0.0f) {}

CobaltUiNavSpotlightTransformFunction::CobaltUiNavSpotlightTransformFunction(
    const scoped_refptr<ui_navigation::NavItem>& focus_item)
    : zero_scale_(0.0f, 0.0f),
      ui_nav_item_(focus_item) {}

std::string CobaltUiNavSpotlightTransformFunction::ToString() const {
  return "-cobalt-ui-nav-spotlight-transform";
}

void CobaltUiNavSpotlightTransformFunction::Accept(
    TransformFunctionVisitor* visitor) const {
  float focus_x, focus_y;
  if (!ui_nav_item_ || !ui_nav_item_->GetFocusVector(&focus_x, &focus_y)) {
    // There is no focus vector. Emulate the zero scale.
    zero_scale_.Accept(visitor);
  } else {
    // There is a valid focus vector. Emulate percentage translation.
    // Map the focus vector values from [-1, +1] to [-50%, +50%].
    TranslateFunction translate_x(TranslateFunction::kXAxis,
        new PercentageValue(focus_x * 0.5f));
    TranslateFunction translate_y(TranslateFunction::kYAxis,
        new PercentageValue(focus_y * 0.5f));
    translate_x.Accept(visitor);
    translate_y.Accept(visitor);
  }
}

}  // namespace cssom
}  // namespace cobalt

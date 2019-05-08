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

#include "cobalt/cssom/interpolated_transform_property_value.h"

#include "base/logging.h"
#include "cobalt/math/matrix_interpolation.h"

namespace cobalt {
namespace cssom {

InterpolatedTransformPropertyValue::InterpolatedTransformPropertyValue(
    TransformPropertyValue* start_value, TransformPropertyValue* end_value,
    float progress)
    : start_value_(start_value),
      end_value_(end_value),
      progress_(progress) {
  DCHECK(start_value);
  DCHECK(end_value);
}

std::string InterpolatedTransformPropertyValue::ToString() const {
  // This method should not ever be called, as it is not possible for
  // JavaScript APIs to ever query for this value.
  // It can be implemented if this functionality is required for debugging
  // purposes.
  NOTREACHED();
  return "InterpolatedTransformPropertyValue";
}

math::Matrix3F InterpolatedTransformPropertyValue::ToMatrix(
    const math::SizeF& used_size,
    const scoped_refptr<ui_navigation::NavItem>& used_ui_nav_focus) const {
  return math::InterpolateMatrices(
      start_value_->ToMatrix(used_size, used_ui_nav_focus),
      end_value_->ToMatrix(used_size, used_ui_nav_focus),
      progress_);
}

}  // namespace cssom
}  // namespace cobalt

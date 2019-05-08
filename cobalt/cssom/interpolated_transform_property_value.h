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

#ifndef COBALT_CSSOM_INTERPOLATED_TRANSFORM_PROPERTY_VALUE_H_
#define COBALT_CSSOM_INTERPOLATED_TRANSFORM_PROPERTY_VALUE_H_

#include <string>

#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/transform_property_value.h"

namespace cobalt {
namespace cssom {

// This represents an interpolation between two TransformPropertyValues.
class InterpolatedTransformPropertyValue : public TransformPropertyValue {
 public:
  InterpolatedTransformPropertyValue(TransformPropertyValue* start_value,
                                     TransformPropertyValue* end_value,
                                     float progress);

  std::string ToString() const override;

  bool HasTrait(TransformFunction::Trait trait) const override {
    return start_value_->HasTrait(trait) || end_value_->HasTrait(trait);
  }

  math::Matrix3F ToMatrix(const math::SizeF& used_size,
      const scoped_refptr<ui_navigation::NavItem>& used_ui_nav_focus)
      const override;

  bool operator==(const InterpolatedTransformPropertyValue& other) const {
    return progress_ == other.progress_ &&
           start_value_->Equals(*other.start_value_) &&
           end_value_->Equals(*other.end_value_);
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(InterpolatedTransformPropertyValue);

 private:
  ~InterpolatedTransformPropertyValue() override {}

  scoped_refptr<TransformPropertyValue> start_value_;
  scoped_refptr<TransformPropertyValue> end_value_;
  const float progress_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_INTERPOLATED_TRANSFORM_PROPERTY_VALUE_H_

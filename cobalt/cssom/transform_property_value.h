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

#ifndef COBALT_CSSOM_TRANSFORM_PROPERTY_VALUE_H_
#define COBALT_CSSOM_TRANSFORM_PROPERTY_VALUE_H_

#include <string>

#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/transform_function.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/size_f.h"
#include "cobalt/ui_navigation/nav_item.h"

namespace cobalt {
namespace cssom {

// A base class for all CSS transform property values.
class TransformPropertyValue : public PropertyValue {
 public:
  void Accept(PropertyValueVisitor* visitor) override {
    visitor->VisitTransformPropertyValue(this);
  }

  // Returns whether the transform has any functions with the specified trait.
  virtual bool HasTrait(TransformFunction::Trait trait) const = 0;

  virtual math::Matrix3F ToMatrix(const math::SizeF& used_size,
      const scoped_refptr<ui_navigation::NavItem>& used_ui_nav_focus) const = 0;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_TRANSFORM_PROPERTY_VALUE_H_

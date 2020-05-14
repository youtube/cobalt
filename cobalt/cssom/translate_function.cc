// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/translate_function.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/calc_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/transform_function_visitor.h"
#include "cobalt/math/transform_2d.h"

namespace cobalt {
namespace cssom {

TranslateFunction::TranslateFunction(Axis axis,
    const scoped_refptr<PropertyValue>& offset)
    : axis_(axis), offset_(offset) {
  DCHECK(offset);
  if (offset_type() == kLength && offset_as_length()->IsUnitRelative()) {
    traits_ = kTraitUsesRelativeUnits;
  }
}

TranslateFunction::OffsetType TranslateFunction::offset_type() const {
  if (offset_->GetTypeId() == base::GetTypeId<LengthValue>()) {
    return kLength;
  } else if (offset_->GetTypeId() == base::GetTypeId<PercentageValue>()) {
    return kPercentage;
  } else if (offset_->GetTypeId() == base::GetTypeId<CalcValue>()) {
    return kCalc;
  } else {
    NOTREACHED();
    return kLength;
  }
}

scoped_refptr<LengthValue> TranslateFunction::offset_as_length() const {
  DCHECK_EQ(kLength, offset_type());
  return scoped_refptr<LengthValue>(
      base::polymorphic_downcast<LengthValue*>(offset_.get()));
}

scoped_refptr<PercentageValue> TranslateFunction::offset_as_percentage() const {
  DCHECK_EQ(kPercentage, offset_type());
  return scoped_refptr<PercentageValue>(
      base::polymorphic_downcast<PercentageValue*>(offset_.get()));
}

scoped_refptr<CalcValue> TranslateFunction::offset_as_calc() const {
  DCHECK_EQ(kCalc, offset_type());
  return scoped_refptr<CalcValue>(
      base::polymorphic_downcast<CalcValue*>(offset_.get()));
}

float TranslateFunction::length_component_in_pixels() const {
  switch (offset_type()) {
    case kLength:
      DCHECK_EQ(kPixelsUnit, offset_as_length()->unit());
      return offset_as_length()->value();
    case kPercentage:
      return 0.0f;
    case kCalc:
      DCHECK_EQ(kPixelsUnit, offset_as_calc()->length_value()->unit());
      return offset_as_calc()->length_value()->value();
  }
  NOTREACHED();
  return 0.0f;
}

float TranslateFunction::percentage_component() const {
  switch (offset_type()) {
    case kLength:
      return 0.0f;
    case kPercentage:
      return offset_as_percentage()->value();
    case kCalc:
      return offset_as_calc()->percentage_value()->value();
  }
  NOTREACHED();
  return 0.0f;
}

void TranslateFunction::Accept(TransformFunctionVisitor* visitor) const {
  visitor->VisitTranslate(this);
}

std::string TranslateFunction::ToString() const {
  char axis = ' ';
  switch (axis_) {
    case kXAxis:
      axis = 'X';
      break;
    case kYAxis:
      axis = 'Y';
      break;
    case kZAxis:
      axis = 'Z';
      break;
  }
  std::string result = "translate";
  result.push_back(axis);
  result.push_back('(');
  if (offset_) {
    result.append(offset_->ToString());
  }
  result.push_back(')');
  return result;
}

math::Matrix3F TranslateFunction::ToMatrix(
    const math::SizeF& used_size,
    const scoped_refptr<ui_navigation::NavItem>& used_ui_nav_focus) const {
  switch (axis_) {
    case kXAxis:
      return math::TranslateMatrix(length_component_in_pixels() +
                                   percentage_component() * used_size.width(),
                                   0.0f);
    case kYAxis:
      return math::TranslateMatrix(0.0f,
                                   length_component_in_pixels() +
                                   percentage_component() * used_size.height());
    case kZAxis:
      if (length_component_in_pixels() != 0 ||
          percentage_component() != 0) {
        LOG(ERROR) << "translateZ is currently a noop in Cobalt.";
      }
      break;
  }
  return math::Matrix3F::Identity();
}

}  // namespace cssom
}  // namespace cobalt

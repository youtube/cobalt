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

#ifndef COBALT_CSSOM_TRANSLATE_FUNCTION_H_
#define COBALT_CSSOM_TRANSLATE_FUNCTION_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/transform_function.h"

namespace cobalt {
namespace cssom {

class CalcValue;
class LengthValue;
class PercentageValue;

// Specifies a translation by the given amount in the X, Y or Z direction.
//   https://www.w3.org/TR/css-transforms-1/#funcdef-translatex
//   https://www.w3.org/TR/css-transforms-1/#funcdef-translatey
//   https://www.w3.org/TR/css-transforms-1/#funcdef-translatez
class TranslateFunction : public TransformFunction {
 public:
  enum Axis {
    kXAxis,
    kYAxis,
    kZAxis,
  };
  enum OffsetType {
    kCalc,
    kLength,
    kPercentage,
  };

  TranslateFunction(Axis axis, const scoped_refptr<PropertyValue>& offset)
      : axis_(axis), offset_(offset) {
    DCHECK(offset);
  }

  void Accept(TransformFunctionVisitor* visitor) const override;

  OffsetType offset_type() const;

  scoped_refptr<LengthValue> offset_as_length() const;
  scoped_refptr<PercentageValue> offset_as_percentage() const;
  scoped_refptr<CalcValue> offset_as_calc() const;

  // The following two functions will return the length/percentage components
  // of the translation, regardless of whether it is a LengthValue,
  // PercentageValue or CalcValue.
  // length_component_in_pixels() can only be called if length units are in
  // pixels.
  float length_component_in_pixels() const;
  float percentage_component() const;

  Axis axis() const { return axis_; }

  std::string ToString() const override;

  bool operator==(const TranslateFunction& other) const {
    return offset_->Equals(*other.offset_) && axis_ == other.axis_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(TranslateFunction);

 private:
  const Axis axis_;
  const scoped_refptr<PropertyValue> offset_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_TRANSLATE_FUNCTION_H_

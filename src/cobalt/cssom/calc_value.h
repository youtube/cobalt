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

#ifndef COBALT_CSSOM_CALC_VALUE_H_
#define COBALT_CSSOM_CALC_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// Represets the result of the mathematical calculation it contains, using
// standard operator precedence rules.
// TODO: Implement the complete version of CalcValue. The
// current CalcValue is just a simplified implementation.
//  https://www.w3.org/TR/css3-values/#calc-notation
class CalcValue : public PropertyValue {
 public:
  CalcValue(const scoped_refptr<LengthValue>& length_value,
            const scoped_refptr<PercentageValue>& percentage_value)
      : length_value_(length_value), percentage_value_(percentage_value) {}

  explicit CalcValue(const scoped_refptr<LengthValue>& length_value)
      : length_value_(length_value),
        percentage_value_(new PercentageValue(0.0f)) {}

  explicit CalcValue(const scoped_refptr<PercentageValue>& percentage_value)
      : length_value_(new LengthValue(0.0f, kPixelsUnit)),
        percentage_value_(percentage_value) {}

  void Accept(PropertyValueVisitor* visitor) override;

  const scoped_refptr<LengthValue>& length_value() const {
    return length_value_;
  }

  const scoped_refptr<PercentageValue> percentage_value() const {
    return percentage_value_.get();
  }

  std::string ToString() const override;

  bool operator==(const CalcValue& other) const {
    return *length_value_ == *other.length_value_ &&
           *percentage_value_ == *other.percentage_value_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(CalcValue);

 private:
  ~CalcValue() override {}

  const scoped_refptr<LengthValue> length_value_;
  const scoped_refptr<PercentageValue> percentage_value_;

  DISALLOW_COPY_AND_ASSIGN(CalcValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CALC_VALUE_H_

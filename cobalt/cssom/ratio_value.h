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

#ifndef COBALT_CSSOM_RATIO_VALUE_H_
#define COBALT_CSSOM_RATIO_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/math/rational.h"

namespace cobalt {
namespace cssom {

class PropertyValueVisitor;

// Represents a ratio value.
//   https://www.w3.org/TR/css3-mediaqueries/#values
// Applies to aspect-ratio and device-aspect-ratio media feature.
//   https://www.w3.org/TR/css3-mediaqueries/#aspect-ratio
class RatioValue : public PropertyValue {
 public:
  explicit RatioValue(const math::Rational& value) : value_(value) {
    DCHECK_LE(1, value_.numerator());
    DCHECK_LE(1, value_.denominator());
  }

  void Accept(PropertyValueVisitor* visitor) override;

  const math::Rational& value() const { return value_; }

  std::string ToString() const override {
    return base::StringPrintf("%d/%d", value_.numerator(),
                              value_.denominator());
  }

  bool operator==(const RatioValue& other) const {
    return value_ == other.value_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(RatioValue);

 private:
  ~RatioValue() override {}

  const math::Rational value_;

  DISALLOW_COPY_AND_ASSIGN(RatioValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_RATIO_VALUE_H_

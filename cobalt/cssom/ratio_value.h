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

#ifndef CSSOM_RATIO_VALUE_H_
#define CSSOM_RATIO_VALUE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/stringprintf.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// Represents a ratio value.
// Applies to aspect-ratio and device-aspect-ratio media feature.
// See http://www.w3.org/TR/css3-mediaqueries/#values and
// http://www.w3.org/TR/css3-mediaqueries/#aspect-ratio for details.
class RatioValue : public PropertyValue {
 public:
  RatioValue(int numerator, int denominator)
      : numerator_(numerator), denominator_(denominator) {}

  void Accept(PropertyValueVisitor* visitor) OVERRIDE;

  int numerator() const { return numerator_; }
  int denominator() const { return denominator_; }

  std::string ToString() const OVERRIDE {
    return base::StringPrintf("%d/%d", numerator_, denominator_);
  }

  bool operator==(const RatioValue& other) const {
    return numerator_ == other.numerator_ && denominator_ == other.denominator_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(RatioValue);

 private:
  ~RatioValue() OVERRIDE {}

  const int numerator_;
  const int denominator_;

  DISALLOW_COPY_AND_ASSIGN(RatioValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_RATIO_VALUE_H_

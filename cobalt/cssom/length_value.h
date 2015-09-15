/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef CSSOM_LENGTH_VALUE_H_
#define CSSOM_LENGTH_VALUE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// TODO(***REMOVED***): Add more units.
enum LengthUnit {
  kFontSizesAkaEmUnit,
  kPixelsUnit,
};

// Represents distance or size.
// Applies to properties such as left, width, font-size, etc.
// See http://www.w3.org/TR/css3-values/#lengths for details.
class LengthValue : public PropertyValue {
 public:
  LengthValue(float value, LengthUnit unit) : value_(value), unit_(unit) {}

  void Accept(PropertyValueVisitor* visitor) OVERRIDE;

  float value() const { return value_; }
  LengthUnit unit() const { return unit_; }

  std::string ToString() const OVERRIDE;

  bool operator==(const LengthValue& other) const {
    return value_ == other.value_ && unit_ == other.unit_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(LengthValue);

 private:
  ~LengthValue() OVERRIDE {}

  const float value_;
  const LengthUnit unit_;

  DISALLOW_COPY_AND_ASSIGN(LengthValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_LENGTH_VALUE_H_

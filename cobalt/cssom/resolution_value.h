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

#ifndef COBALT_CSSOM_RESOLUTION_VALUE_H_
#define COBALT_CSSOM_RESOLUTION_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

class PropertyValueVisitor;

enum ResolutionUnit {
  kDPIUnit,
  kDPCMUnit,
};

// Represents a resolution value.
// Applies to resolution media feature.
// See https://www.w3.org/TR/css3-mediaqueries/#resolution and
// https://www.w3.org/TR/css3-mediaqueries/#resolution0 for details.
class ResolutionValue : public PropertyValue {
 public:
  ResolutionValue(float value, ResolutionUnit unit)
      : value_(value), unit_(unit) {}

  void Accept(PropertyValueVisitor* visitor) override;

  float value() const { return value_; }
  ResolutionUnit unit() const { return unit_; }

  // Returns the value in dpi.
  float dpi_value() const;

  std::string ToString() const override;

  bool operator==(const ResolutionValue& other) const {
    return value_ == other.value_ && unit_ == other.unit_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(ResolutionValue);

 private:
  ~ResolutionValue() override {}

  const float value_;
  const ResolutionUnit unit_;

  DISALLOW_COPY_AND_ASSIGN(ResolutionValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_RESOLUTION_VALUE_H_

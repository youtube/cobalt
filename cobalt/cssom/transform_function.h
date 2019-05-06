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

#ifndef COBALT_CSSOM_TRANSFORM_FUNCTION_H_
#define COBALT_CSSOM_TRANSFORM_FUNCTION_H_

#include <string>

#include "cobalt/base/polymorphic_equatable.h"

namespace cobalt {
namespace cssom {

class TransformFunctionVisitor;

// A base class for all transform functions.
// Transform functions define how transformation is applied to the coordinate
// system an HTML element renders in.
//   https://www.w3.org/TR/css-transforms-1/#transform-functions
class TransformFunction : public base::PolymorphicEquatable {
 public:
  enum Trait {
    // The value of this transform function changes over time. Custom transform
    // functions (e.g. those that work with UI navigation) may have this trait.
    // Standard transform functions do not have this trait.
    kTraitIsDynamic = 1 << 0,

    // This function uses LengthValue and LengthValue::IsUnitRelative() is true.
    // Use of PercentageValue does not equate to having this trait.
    kTraitUsesRelativeUnits = 1 << 1,
  };

  virtual void Accept(TransformFunctionVisitor* visitor) const = 0;

  virtual std::string ToString() const = 0;

  virtual ~TransformFunction() {}

  uint32 Traits() const { return traits_; }
  bool HasTrait(Trait trait) const { return (traits_ & trait) != 0; }

 protected:
  uint32 traits_ = 0;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_TRANSFORM_FUNCTION_H_

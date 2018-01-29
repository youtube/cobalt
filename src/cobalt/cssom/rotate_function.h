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

#ifndef COBALT_CSSOM_ROTATE_FUNCTION_H_
#define COBALT_CSSOM_ROTATE_FUNCTION_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/stringprintf.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/transform_function.h"

namespace cobalt {
namespace cssom {

class TransformFunctionVisitor;

// The rotate function specifies a 2D rotation operation by the the specified
// angle.
//   https://www.w3.org/TR/css-transforms-1/#funcdef-rotate
class RotateFunction : public TransformFunction {
 public:
  explicit RotateFunction(float clockwise_angle_in_radians)
      : clockwise_angle_in_radians_(clockwise_angle_in_radians) {}

  void Accept(TransformFunctionVisitor* visitor) const override;

  float clockwise_angle_in_radians() const {
    return clockwise_angle_in_radians_;
  }

  std::string ToString() const override {
    return base::StringPrintf("rotate(%.7grad)", clockwise_angle_in_radians_);
  }

  bool operator==(const RotateFunction& other) const {
    return clockwise_angle_in_radians_ == other.clockwise_angle_in_radians_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(RotateFunction);

 private:
  const float clockwise_angle_in_radians_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_ROTATE_FUNCTION_H_

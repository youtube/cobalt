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

#ifndef CSSOM_ROTATE_FUNCTION_H_
#define CSSOM_ROTATE_FUNCTION_H_

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/transform_function.h"

namespace cobalt {
namespace cssom {

// Scale function specifies a 2D scale operation by the scaling vector.
//   http://www.w3.org/TR/css-transforms-1/#funcdef-scale
class RotateFunction : public TransformFunction {
 public:
  explicit RotateFunction(float angle_in_radians)
      : angle_in_radians_(angle_in_radians) {}

  void Accept(TransformFunctionVisitor* visitor) OVERRIDE;

  float angle_in_radians() const {
    return angle_in_radians_;
  }

  bool operator==(const RotateFunction& other) const {
    return angle_in_radians_ == other.angle_in_radians_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(RotateFunction);

 private:
  const float angle_in_radians_;

  DISALLOW_COPY_AND_ASSIGN(RotateFunction);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_ROTATE_FUNCTION_H_

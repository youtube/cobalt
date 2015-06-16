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

#ifndef CSSOM_MATRIX_FUNCTION_H_
#define CSSOM_MATRIX_FUNCTION_H_

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/transform_function.h"

namespace cobalt {
namespace cssom {

// The matrix function allows one to specify a 2D 2x3 affine transformation
// as a matrix.
//   http://www.w3.org/TR/css-transforms-1/#funcdef-matrix
class MatrixFunction : public TransformFunction {
 public:
  MatrixFunction(float m00, float m10, float m01, float m11,
                 float m02, float m12);

  void Accept(TransformFunctionVisitor* visitor) OVERRIDE;

  const math::Matrix3F& value() const { return value_; }

  bool operator==(const MatrixFunction& other) const {
    return value_ == other.value_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(MatrixFunction);

 private:
  const math::Matrix3F value_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_MATRIX_FUNCTION_H_

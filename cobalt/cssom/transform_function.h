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

#ifndef CSSOM_TRANSFORM_FUNCTION_H_
#define CSSOM_TRANSFORM_FUNCTION_H_

#include "cobalt/math/matrix3_f.h"

namespace cobalt {
namespace cssom {

class TransformFunctionVisitor;

// A base class for all transform functions.
// Transform functions define how transformation is applied to the coordinate
// system an HTML element renders in.
//   http://www.w3.org/TR/css-transforms-1/#transform-functions
class TransformFunction {
 public:
  virtual void Accept(TransformFunctionVisitor* visitor) = 0;

  virtual bool IsEqual(const TransformFunction* other) const = 0;
  virtual base::TypeId GetTypeId() const = 0;

  virtual ~TransformFunction() {}
};

// Used to provide type-safe equality checking even when the exact
// TransformFunction type is unknown.  For any class T that is intended to be
// a descendant of TransformFunction, it should call this macro in the public
// section of its class declaration.
#define DEFINE_TRANSFORM_FUNCTION_TYPE(CLASS_NAME)                            \
  bool IsEqual(const TransformFunction* other) const OVERRIDE {               \
    return base::GetTypeId<CLASS_NAME>() == other->GetTypeId() &&             \
           *static_cast<const CLASS_NAME*>(this) ==                           \
           *static_cast<const CLASS_NAME*>(other);                            \
  }                                                                           \
                                                                              \
  base::TypeId GetTypeId() const OVERRIDE {                                   \
    return base::GetTypeId<CLASS_NAME>();                                     \
  }                                                                           \

// Applies the specified transformation to the in/out matrix parameter.
// The transform function is converted to a matrix and then appended to
// the passed in matrix.
void PostMultiplyMatrixByTransform(TransformFunction* function,
                                   math::Matrix3F* matrix);

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_TRANSFORM_FUNCTION_H_

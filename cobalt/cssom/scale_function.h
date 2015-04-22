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

#ifndef CSSOM_SCALE_FUNCTION_H_
#define CSSOM_SCALE_FUNCTION_H_

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/transform_function.h"

namespace cobalt {
namespace cssom {

// Scale function specifies a 2D scale operation by the scaling vector.
//   http://www.w3.org/TR/css-transforms-1/#funcdef-scale
class ScaleFunction : public TransformFunction {
 public:
  ScaleFunction(float x_factor, float y_factor)
      : x_factor_(x_factor), y_factor_(y_factor) {}

  void Accept(TransformFunctionVisitor* visitor) OVERRIDE;

  float x_factor() const { return x_factor_; }
  float y_factor() const { return y_factor_; }

  bool operator==(const ScaleFunction& other) const {
    return x_factor_ == other.x_factor_ && y_factor_ == other.y_factor_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(ScaleFunction);

 private:
  const float x_factor_;
  const float y_factor_;

  DISALLOW_COPY_AND_ASSIGN(ScaleFunction);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_SCALE_FUNCTION_H_

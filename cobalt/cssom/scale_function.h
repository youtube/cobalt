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
  ScaleFunction(const scoped_refptr<PropertyValue>& x_factor,
                const scoped_refptr<PropertyValue>& y_factor)
      : x_factor_(x_factor), y_factor_(y_factor) {}

  void Accept(TransformFunctionVisitor* visitor) OVERRIDE;

  const scoped_refptr<PropertyValue>& x_factor() const { return x_factor_; }
  const scoped_refptr<PropertyValue>& y_factor() const { return y_factor_; }

 private:
  const scoped_refptr<PropertyValue> x_factor_;
  const scoped_refptr<PropertyValue> y_factor_;

  DISALLOW_COPY_AND_ASSIGN(ScaleFunction);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_SCALE_FUNCTION_H_

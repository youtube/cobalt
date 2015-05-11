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

#ifndef CSSOM_TRANSFORM_FUNCTION_LIST_VALUE_H_
#define CSSOM_TRANSFORM_FUNCTION_LIST_VALUE_H_

#include "cobalt/cssom/scoped_list_value.h"
#include "cobalt/cssom/transform_function.h"

namespace cobalt {
namespace cssom {

// A list of transform functions that define how transformation is applied
// to the coordinate system an HTML element renders in.
//   http://www.w3.org/TR/css-transforms-1/#typedef-transform-list
class TransformFunctionListValue : public ScopedListValue<TransformFunction> {
 public:
  explicit TransformFunctionListValue(
      ScopedListValue<TransformFunction>::Builder value)
      : ScopedListValue(value.Pass()) {}

  void Accept(PropertyValueVisitor* visitor) OVERRIDE {
    visitor->VisitTransformFunctionList(this);
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(TransformFunctionListValue);

 private:
  virtual ~TransformFunctionListValue() OVERRIDE {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_TRANSFORM_FUNCTION_LIST_VALUE_H_

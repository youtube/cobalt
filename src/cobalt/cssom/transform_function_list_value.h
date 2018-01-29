// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_TRANSFORM_FUNCTION_LIST_VALUE_H_
#define COBALT_CSSOM_TRANSFORM_FUNCTION_LIST_VALUE_H_

#include <string>

#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/scoped_list_value.h"
#include "cobalt/cssom/transform_function.h"
#include "cobalt/cssom/transform_matrix.h"
#include "cobalt/math/matrix3_f.h"

namespace cobalt {
namespace cssom {

// A list of transform functions that define how transformation is applied
// to the coordinate system an HTML element renders in.
//   https://www.w3.org/TR/css-transforms-1/#typedef-transform-list
class TransformFunctionListValue : public ScopedListValue<TransformFunction> {
 public:
  explicit TransformFunctionListValue(
      ScopedListValue<TransformFunction>::Builder value)
      : ScopedListValue<TransformFunction>(value.Pass()) {}

  void Accept(PropertyValueVisitor* visitor) override {
    visitor->VisitTransformFunctionList(this);
  }

  TransformMatrix ToMatrix() const;

  std::string ToString() const override;

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(TransformFunctionListValue);

 private:
  ~TransformFunctionListValue() override {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_TRANSFORM_FUNCTION_LIST_VALUE_H_

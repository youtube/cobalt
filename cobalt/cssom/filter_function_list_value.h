// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_FILTER_FUNCTION_LIST_VALUE_H_
#define COBALT_CSSOM_FILTER_FUNCTION_LIST_VALUE_H_

#include <string>

#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/filter_function.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/scoped_list_value.h"

namespace cobalt {
namespace cssom {

// A list of composed filter functions.
class FilterFunctionListValue : public ScopedListValue<FilterFunction> {
 public:
  explicit FilterFunctionListValue(
      ScopedListValue<FilterFunction>::Builder value)
      : ScopedListValue<FilterFunction>(value.Pass()) {}

  void Accept(PropertyValueVisitor* visitor) override {
    visitor->VisitFilterFunctionList(this);
  }

  std::string ToString() const override;

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(FilterFunctionListValue);

 private:
  ~FilterFunctionListValue() override {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_FILTER_FUNCTION_LIST_VALUE_H_

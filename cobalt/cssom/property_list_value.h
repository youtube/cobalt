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

#ifndef COBALT_CSSOM_PROPERTY_LIST_VALUE_H_
#define COBALT_CSSOM_PROPERTY_LIST_VALUE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/scoped_ref_list_value.h"

namespace cobalt {
namespace cssom {

class PropertyListValue : public ScopedRefListValue<PropertyValue> {
 public:
  explicit PropertyListValue(
      scoped_ptr<ScopedRefListValue<PropertyValue>::Builder> value)
      : ScopedRefListValue<PropertyValue>(value.Pass()) {}

  void Accept(PropertyValueVisitor* visitor) override {
    visitor->VisitPropertyList(this);
  }

  std::string ToString() const override {
    std::string result;
    for (size_t i = 0; i < value().size(); ++i) {
      if (!result.empty()) result.append(", ");
      result.append(value()[i]->ToString());
    }
    return result;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(PropertyListValue);

 private:
  ~PropertyListValue() override {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_PROPERTY_LIST_VALUE_H_

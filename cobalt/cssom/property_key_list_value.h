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

#ifndef COBALT_CSSOM_PROPERTY_KEY_LIST_VALUE_H_
#define COBALT_CSSOM_PROPERTY_KEY_LIST_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/list_value.h"
#include "cobalt/cssom/property_definitions.h"

namespace cobalt {
namespace cssom {

class PropertyValueVisitor;

// A list of values of type PropertyKey.  May be used to store property
// keys, such as for the 'transition-property' CSS property.
class PropertyKeyListValue : public ListValue<PropertyKey> {
 public:
  explicit PropertyKeyListValue(
      scoped_ptr<ListValue<PropertyKey>::Builder> value)
      : ListValue<PropertyKey>(value.Pass()) {}

  void Accept(PropertyValueVisitor* visitor) override {
    visitor->VisitPropertyKeyList(this);
  }

  std::string ToString() const override;

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(PropertyKeyListValue);

 private:
  ~PropertyKeyListValue() override {}

  DISALLOW_COPY_AND_ASSIGN(PropertyKeyListValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_PROPERTY_KEY_LIST_VALUE_H_

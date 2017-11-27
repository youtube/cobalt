// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_LIST_VALUE_H_
#define COBALT_CSSOM_LIST_VALUE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

// This class is used to represent a CSS property value that is a list
// of concrete objects.  The list can be constructed with a ListValue::Builder
// object and after that is immutable.  List elements must have operator==()
// defined on them.  For lists of heap-allocated polymorphic data types, please
// see ScopedListValue instead.
template <typename T>
class ListValue : public PropertyValue {
 public:
  typedef std::vector<T> Builder;

  explicit ListValue(scoped_ptr<Builder> value) : value_(value.Pass()) {
    DCHECK(value_.get());
    DCHECK(!value_->empty());
  }

  const T& get_item_modulo_size(int index) const {
    return (*value_)[index % value_->size()];
  }

  const Builder& value() const { return *value_; }

  bool operator==(const ListValue<T>& other) const {
    return *value_ == *other.value_;
  }

 protected:
  ~ListValue() override {}

  const scoped_ptr<Builder> value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ListValue<T>);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_LIST_VALUE_H_

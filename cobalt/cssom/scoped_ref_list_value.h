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

#ifndef COBALT_CSSOM_SCOPED_REF_LIST_VALUE_H_
#define COBALT_CSSOM_SCOPED_REF_LIST_VALUE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// This class can be used to represent a CSS property value that is a list of
// ref-counted polymorphic objects.  All elements must be reference counted and
// also derive from base::PolymorphicEquatable so that they can be tested for
// equality by value.
template <typename T>
class ScopedRefListValue : public PropertyValue {
 public:
  typedef std::vector<scoped_refptr<T> > Builder;

  explicit ScopedRefListValue(scoped_ptr<Builder> value)
      : value_(value.Pass()) {
    DCHECK(value_.get());
    DCHECK(!value_->empty());
  }

  const scoped_refptr<T>& get_item_modulo_size(int index) const {
    return (*value_)[index % value_->size()];
  }

  const Builder& value() const { return *value_; }

  bool operator==(const ScopedRefListValue<T>& other) const {
    if (value_->size() != other.value_->size()) return false;

    typename Builder::const_iterator iter_a = value_->begin();
    typename Builder::const_iterator iter_b = other.value_->begin();
    for (; iter_a != value_->end(); ++iter_a, ++iter_b) {
      if (!(*iter_a)->Equals(**iter_b)) {
        return false;
      }
    }

    return true;
  }

 protected:
  ~ScopedRefListValue() override {}

  const scoped_ptr<Builder> value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedRefListValue<T>);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_SCOPED_REF_LIST_VALUE_H_

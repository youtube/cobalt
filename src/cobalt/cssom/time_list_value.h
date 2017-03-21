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

#ifndef COBALT_CSSOM_TIME_LIST_VALUE_H_
#define COBALT_CSSOM_TIME_LIST_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/list_value.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

// A CSS property value that is a list of base::TimeDelta values.  Example
// properties that can hold this value type are 'transition-duration' and
// 'transition-delay'.
class TimeListValue : public ListValue<base::TimeDelta> {
 public:
  explicit TimeListValue(scoped_ptr<ListValue<base::TimeDelta>::Builder> value)
      : ListValue<base::TimeDelta>(value.Pass()) {}

  void Accept(PropertyValueVisitor* visitor) OVERRIDE {
    visitor->VisitTimeList(this);
  }

  std::string ToString() const OVERRIDE;

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(TimeListValue);

 private:
  virtual ~TimeListValue() OVERRIDE {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_TIME_LIST_VALUE_H_

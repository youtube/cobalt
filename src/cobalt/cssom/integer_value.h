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

#ifndef COBALT_CSSOM_INTEGER_VALUE_H_
#define COBALT_CSSOM_INTEGER_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/stringprintf.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// Represents a dimensionless value.
//   https://www.w3.org/TR/css3-values/#integers
class IntegerValue : public PropertyValue {
 public:
  explicit IntegerValue(int value) : value_(value) {}

  virtual void Accept(PropertyValueVisitor* visitor) OVERRIDE;

  int value() const { return value_; }

  std::string ToString() const OVERRIDE {
    return base::StringPrintf("%d", value_);
  }


  bool operator==(const IntegerValue& other) const {
    return value_ == other.value_;
  }

  bool operator==(const int& other) const { return value_ == other; }

  bool operator>=(const IntegerValue& other) const {
    return value_ >= other.value_;
  }

  bool operator>=(const int& other) const { return value_ >= other; }

  bool operator<=(const IntegerValue& other) const {
    return value_ <= other.value_;
  }

  bool operator<=(const int& other) const { return value_ <= other; }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(IntegerValue);

 private:
  ~IntegerValue() OVERRIDE {}

  const int value_;

  DISALLOW_COPY_AND_ASSIGN(IntegerValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_INTEGER_VALUE_H_

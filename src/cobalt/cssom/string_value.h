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

#ifndef COBALT_CSSOM_STRING_VALUE_H_
#define COBALT_CSSOM_STRING_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

class PropertyValueVisitor;

// Represents a sequence of characters delimited by single or double quotes.
// Applies to properties like font-family.
// See https://www.w3.org/TR/css3-values/#strings for details.
class StringValue : public PropertyValue {
 public:
  explicit StringValue(const std::string& value) : value_(value) {}

  void Accept(PropertyValueVisitor* visitor) OVERRIDE;

  const std::string& value() const { return value_; }

  std::string ToString() const OVERRIDE { return "'" + value_ + "'"; }

  bool operator==(const StringValue& other) const {
    return value_ == other.value_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(StringValue);

 private:
  ~StringValue() OVERRIDE {}

  const std::string value_;

  DISALLOW_COPY_AND_ASSIGN(StringValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_STRING_VALUE_H_

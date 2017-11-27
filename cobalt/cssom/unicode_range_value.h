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

#ifndef COBALT_CSSOM_UNICODE_RANGE_VALUE_H_
#define COBALT_CSSOM_UNICODE_RANGE_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

class PropertyValueVisitor;

// See https://www.w3.org/TR/css3-fonts/#unicode-range-desc for details.
class UnicodeRangeValue : public PropertyValue {
 public:
  UnicodeRangeValue(int start, int end);

  void Accept(PropertyValueVisitor* visitor) override;

  int start() const { return start_; }
  int end() const { return end_; }

  std::string ToString() const override;

  bool operator==(const UnicodeRangeValue& other) const {
    return start_ == other.start_ && end_ == other.end_;
  }

  bool IsValid() const;

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(UnicodeRangeValue);

 private:
  ~UnicodeRangeValue() override {}

  const int start_;
  const int end_;

  DISALLOW_COPY_AND_ASSIGN(UnicodeRangeValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_UNICODE_RANGE_VALUE_H_

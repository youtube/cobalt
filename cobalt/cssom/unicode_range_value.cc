// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/unicode_range_value.h"

#include "base/strings/stringprintf.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

UnicodeRangeValue::UnicodeRangeValue(int start, int end)
    : start_(start), end_(end) {}

void UnicodeRangeValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitUnicodeRange(this);
}

std::string UnicodeRangeValue::ToString() const {
  if (start_ == end_) {
    return base::StringPrintf("U+%X", start_);
  } else {
    return base::StringPrintf("U+%X-%X", start_, end_);
  }
}

bool UnicodeRangeValue::IsValid() const { return start_ <= end_; }

}  // namespace cssom
}  // namespace cobalt

// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/length_value.h"

#include "base/strings/stringprintf.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

void LengthValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitLength(this);
}

std::string LengthValue::ToString() const {
  const char* kUnitNames[] = {
      "px", "em", "rem", "vw", "vh",
  };
  return base::StringPrintf("%.7g%s", value_, kUnitNames[unit_]);
}

}  // namespace cssom
}  // namespace cobalt

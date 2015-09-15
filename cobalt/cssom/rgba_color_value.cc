/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/cssom/rgba_color_value.h"

#include "base/stringprintf.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

void RGBAColorValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitRGBAColor(this);
}

std::string RGBAColorValue::ToString() const {
  if (a() >= 255.0) {
    return base::StringPrintf("rgb(%u,%u,%u)", r(), g(), b());
  } else {
    return base::StringPrintf("rgba(%u,%u,%u,%.7g)", r(), g(), b(),
                              a() / 255.0);
  }
}

}  // namespace cssom
}  // namespace cobalt

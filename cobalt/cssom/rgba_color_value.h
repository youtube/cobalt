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

#ifndef CSSOM_RGBA_COLOR_VALUE_H_
#define CSSOM_RGBA_COLOR_VALUE_H_

#include <iostream>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// Represents color values that are convertible to RGBA without knowledge
// of element's context, for example:
//   - #0047ab
//   - rgb(0, 71, 171)
//   - rgba(100%, 0, 0, 10%)
//   - hsl(120, 75%, 75%)
//   - fuchsia
//
// Applies to properties such as background-color, color, etc.
//
// See http://www.w3.org/TR/css3-color/#rgb-color for details.
class RGBAColorValue : public PropertyValue {
 public:
  explicit RGBAColorValue(uint32_t value) : value_(value) {}

  virtual void Accept(PropertyValueVisitor* visitor) OVERRIDE;

  uint32_t value() const { return value_; }

  DEFINE_WRAPPABLE_TYPE(RGBAColorValue);

 private:
  ~RGBAColorValue() OVERRIDE {}

  const uint32_t value_;

  DISALLOW_COPY_AND_ASSIGN(RGBAColorValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_RGBA_COLOR_VALUE_H_

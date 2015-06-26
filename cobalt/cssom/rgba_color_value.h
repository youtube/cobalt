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
// The data is maintained as a 32 bit integer layed out as RRGGBBAA, with
// the red bytes being the most significant.
//
// Applies to properties such as background-color, color, etc.
//
// See http://www.w3.org/TR/css3-color/#rgb-color for details.
class RGBAColorValue : public PropertyValue {
 public:
  explicit RGBAColorValue(uint32_t value) : value_(value) {}

  RGBAColorValue(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
      : value_(static_cast<uint32_t>(r << 24) | static_cast<uint32_t>(g << 16) |
               static_cast<uint32_t>(b << 8) | static_cast<uint32_t>(a << 0)) {}

  void Accept(PropertyValueVisitor* visitor) OVERRIDE;

  uint32_t value() const { return value_; }

  uint8_t r() const { return static_cast<uint8_t>((value_ >> 24) & 0xFF); }
  uint8_t g() const { return static_cast<uint8_t>((value_ >> 16) & 0xFF); }
  uint8_t b() const { return static_cast<uint8_t>((value_ >> 8) & 0xFF); }
  uint8_t a() const { return static_cast<uint8_t>((value_ >> 0) & 0xFF); }

  bool operator==(const RGBAColorValue& other) const {
    return value_ == other.value_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(RGBAColorValue);

 private:
  ~RGBAColorValue() OVERRIDE {}

  const uint32_t value_;

  DISALLOW_COPY_AND_ASSIGN(RGBAColorValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_RGBA_COLOR_VALUE_H_

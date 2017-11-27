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

#ifndef COBALT_CSSOM_RGBA_COLOR_VALUE_H_
#define COBALT_CSSOM_RGBA_COLOR_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

class PropertyValueVisitor;

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
// See https://www.w3.org/TR/css3-color/#rgb-color for details.
class RGBAColorValue : public PropertyValue {
 public:
  static const scoped_refptr<RGBAColorValue>& GetAqua();
  static const scoped_refptr<RGBAColorValue>& GetBlack();
  static const scoped_refptr<RGBAColorValue>& GetBlue();
  static const scoped_refptr<RGBAColorValue>& GetFuchsia();
  static const scoped_refptr<RGBAColorValue>& GetGray();
  static const scoped_refptr<RGBAColorValue>& GetGreen();
  static const scoped_refptr<RGBAColorValue>& GetLime();
  static const scoped_refptr<RGBAColorValue>& GetMaroon();
  static const scoped_refptr<RGBAColorValue>& GetNavy();
  static const scoped_refptr<RGBAColorValue>& GetOlive();
  static const scoped_refptr<RGBAColorValue>& GetPurple();
  static const scoped_refptr<RGBAColorValue>& GetRed();
  static const scoped_refptr<RGBAColorValue>& GetSilver();
  static const scoped_refptr<RGBAColorValue>& GetTeal();
  static const scoped_refptr<RGBAColorValue>& GetTransparent();
  static const scoped_refptr<RGBAColorValue>& GetWhite();
  static const scoped_refptr<RGBAColorValue>& GetYellow();

  explicit RGBAColorValue(uint32 value) : value_(value) {}

  RGBAColorValue(uint8 r, uint8 g, uint8 b, uint8 a)
      : value_(static_cast<uint32>(r << 24) | static_cast<uint32>(g << 16) |
               static_cast<uint32>(b << 8) | static_cast<uint32>(a << 0)) {}

  void Accept(PropertyValueVisitor* visitor) override;

  uint32 value() const { return value_; }

  uint8 r() const { return static_cast<uint8>((value_ >> 24) & 0xFF); }
  uint8 g() const { return static_cast<uint8>((value_ >> 16) & 0xFF); }
  uint8 b() const { return static_cast<uint8>((value_ >> 8) & 0xFF); }
  uint8 a() const { return static_cast<uint8>((value_ >> 0) & 0xFF); }

  std::string ToString() const override;

  bool operator==(const RGBAColorValue& other) const {
    return value_ == other.value_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(RGBAColorValue);

 private:
  ~RGBAColorValue() override {}

  const uint32 value_;

  DISALLOW_COPY_AND_ASSIGN(RGBAColorValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_RGBA_COLOR_VALUE_H_

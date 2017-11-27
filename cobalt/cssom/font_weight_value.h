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

#ifndef COBALT_CSSOM_FONT_WEIGHT_VALUE_H_
#define COBALT_CSSOM_FONT_WEIGHT_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

class PropertyValueVisitor;

// Specifies the weight of glyphs in the font, their degree of blackness
// or stroke thickness.
//   https://www.w3.org/TR/css3-fonts/#font-weight-prop
class FontWeightValue : public PropertyValue {
 public:
  enum Value {
    kThinAka100,
    kExtraLightAka200,  // same as Ultra Light
    kLightAka300,
    kNormalAka400,
    kMediumAka500,
    kSemiBoldAka600,    // same as Demi Bold
    kBoldAka700,
    kExtraBoldAka800,   // same as Ultra Bold
    kBlackAka900,       // same as Heavy
  };

  // For the sake of saving memory an explicit instantiation of this class
  // is disallowed. Use factory methods below to obtain shared instances.
  static const scoped_refptr<FontWeightValue>& GetThinAka100();
  static const scoped_refptr<FontWeightValue>& GetExtraLightAka200();
  static const scoped_refptr<FontWeightValue>& GetLightAka300();
  static const scoped_refptr<FontWeightValue>& GetNormalAka400();
  static const scoped_refptr<FontWeightValue>& GetMediumAka500();
  static const scoped_refptr<FontWeightValue>& GetSemiBoldAka600();
  static const scoped_refptr<FontWeightValue>& GetBoldAka700();
  static const scoped_refptr<FontWeightValue>& GetExtraBoldAka800();
  static const scoped_refptr<FontWeightValue>& GetBlackAka900();

  void Accept(PropertyValueVisitor* visitor) override;

  Value value() const { return value_; }

  std::string ToString() const override;

  bool operator==(const FontWeightValue& other) const {
    return value_ == other.value_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(FontWeightValue);

  // Implementation detail, has to be public in order to be constructible.
  struct NonTrivialStaticFields;

 private:
  explicit FontWeightValue(Value value) : value_(value) {}
  ~FontWeightValue() override {}

  const Value value_;

  DISALLOW_COPY_AND_ASSIGN(FontWeightValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_FONT_WEIGHT_VALUE_H_

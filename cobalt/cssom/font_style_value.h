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

#ifndef COBALT_CSSOM_FONT_STYLE_VALUE_H_
#define COBALT_CSSOM_FONT_STYLE_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

class PropertyValueVisitor;

// Allows italic or oblique faces to be selected. Italic forms are generally
// cursive in nature while oblique faces are typically sloped versions of
// regular face.
//   https://www.w3.org/TR/css3-fonts/#font-style-prop
class FontStyleValue : public PropertyValue {
 public:
  enum Value {
    kItalic,
    kNormal,
    kOblique,
  };

  // For the sake of saving memory an explicit instantiation of this class
  // is disallowed. Use getter methods below to obtain shared instances.
  static const scoped_refptr<FontStyleValue>& GetItalic();
  static const scoped_refptr<FontStyleValue>& GetNormal();
  static const scoped_refptr<FontStyleValue>& GetOblique();

  void Accept(PropertyValueVisitor* visitor) override;

  Value value() const { return value_; }

  std::string ToString() const override;

  bool operator==(const FontStyleValue& other) const {
    return value_ == other.value_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(FontStyleValue);

  // Implementation detail, has to be public in order to be constructible.
  struct NonTrivialStaticFields;

 private:
  explicit FontStyleValue(Value value) : value_(value) {}
  ~FontStyleValue() override {}

  const Value value_;

  DISALLOW_COPY_AND_ASSIGN(FontStyleValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_FONT_STYLE_VALUE_H_

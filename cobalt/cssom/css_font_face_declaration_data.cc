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

#include "cobalt/cssom/css_font_face_declaration_data.h"

#include "base/strings/string_util.h"
#include "cobalt/cssom/font_style_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/property_definitions.h"

namespace cobalt {
namespace cssom {

// Font style and font weight default to normal
//   https://www.w3.org/TR/css3-fonts/#descdef-font-style
//   https://www.w3.org/TR/css3-fonts/#descdef-font-weight
CSSFontFaceDeclarationData::CSSFontFaceDeclarationData()
    : style_(FontStyleValue::GetNormal()),
      weight_(FontWeightValue::GetNormalAka400()) {}

void CSSFontFaceDeclarationData::AssignFrom(
    const CSSFontFaceDeclarationData& rhs) {
  set_family(rhs.family());
  set_src(rhs.src());
  set_style(rhs.style());
  set_weight(rhs.weight());
  set_unicode_range(rhs.unicode_range());
}

bool CSSFontFaceDeclarationData::IsSupportedPropertyKey(PropertyKey key) const {
  return key == kSrcProperty || key == kFontFamilyProperty ||
         key == kFontStyleProperty || key == kFontWeightProperty ||
         key == kUnicodeRangeProperty;
}

scoped_refptr<PropertyValue> CSSFontFaceDeclarationData::GetPropertyValue(
    PropertyKey key) const {
  scoped_refptr<PropertyValue>* property_value_reference =
      const_cast<CSSFontFaceDeclarationData*>(this)
          ->GetPropertyValueReference(key);
  return property_value_reference ? *property_value_reference : NULL;
}

void CSSFontFaceDeclarationData::SetPropertyValue(
    PropertyKey key, const scoped_refptr<PropertyValue>& property_value) {
  scoped_refptr<PropertyValue>* property_value_reference =
      GetPropertyValueReference(key);
  if (property_value_reference) {
    *property_value_reference = property_value;
  } else {
    // 3. If property is not a case-sensitive match for a supported CSS
    // property, terminate this algorithm.
    //   https://www.w3.org/TR/2013/WD-cssom-20131205/#dom-cssstyledeclaration-setpropertyvalue
  }
}

scoped_refptr<PropertyValue>*
CSSFontFaceDeclarationData::GetPropertyValueReference(PropertyKey key) {
  // A large if-statement is used here instead of a switch statement so that
  // we can avoid a warning for not explicitly enumerating every property value.
  if (key == kSrcProperty) {
    return &src_;
  } else if (key == kFontFamilyProperty) {
    return &family_;
  } else if (key == kFontStyleProperty) {
    return &style_;
  } else if (key == kFontWeightProperty) {
    return &weight_;
  } else if (key == kUnicodeRangeProperty) {
    return &unicode_range_;
  } else {
    return NULL;
  }
}

}  // namespace cssom
}  // namespace cobalt

/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/cssom/css_font_face_declaration_data.h"

#include "base/string_util.h"
#include "cobalt/cssom/font_style_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/property_definitions.h"

namespace cobalt {
namespace cssom {

// Font style and font weight default to normal
//   http://www.w3.org/TR/css3-fonts/#descdef-font-style
//   http://www.w3.org/TR/css3-fonts/#descdef-font-weight
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

scoped_refptr<const PropertyValue> CSSFontFaceDeclarationData::GetPropertyValue(
    const std::string& property_name) {
  scoped_refptr<PropertyValue>* property_value_reference =
      GetPropertyValueReference(property_name);
  return property_value_reference ? *property_value_reference : NULL;
}

void CSSFontFaceDeclarationData::SetPropertyValue(
    const std::string& property_name,
    const scoped_refptr<PropertyValue>& property_value) {
  scoped_refptr<PropertyValue>* property_value_reference =
      GetPropertyValueReference(property_name);
  if (property_value_reference) {
    *property_value_reference = property_value;
  } else {
    // 3. If property is not a case-sensitive match for a supported CSS
    // property, terminate this algorithm.
    //   http://www.w3.org/TR/2013/WD-cssom-20131205/#dom-cssstyledeclaration-setpropertyvalue
  }
}

scoped_refptr<PropertyValue>*
CSSFontFaceDeclarationData::GetPropertyValueReference(
    const std::string& property_name) {
  switch (property_name.size()) {
    case 3:
      if (LowerCaseEqualsASCII(property_name, GetPropertyName(kSrcProperty))) {
        return &src_;
      }
      return NULL;

    case 10:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kFontStyleProperty))) {
        return &style_;
      }
      return NULL;

    case 11:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kFontFamilyProperty))) {
        return &family_;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kFontWeightProperty))) {
        return &weight_;
      }
      return NULL;

    case 13:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kUnicodeRangeProperty))) {
        return &unicode_range_;
      }
      return NULL;

    default:
      return NULL;
  }
}

}  // namespace cssom
}  // namespace cobalt

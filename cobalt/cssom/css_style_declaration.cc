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

#include "cobalt/cssom/css_style_declaration.h"

#include "base/string_util.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// WARNING: When adding a new property, update GetPropertyValue(),
//          SetPropertyValue(), and AssignFrom() methods below.
const char* const kBackgroundColorPropertyName = "background-color";
const char* const kColorPropertyName = "color";
const char* const kDisplayPropertyName = "display";
const char* const kFontFamilyPropertyName = "font-family";
const char* const kFontSizePropertyName = "font-size";
const char* const kHeightPropertyName = "height";
const char* const kOverflowPropertyName = "overflow";
const char* const kTransformPropertyName = "transform";
const char* const kWidthPropertyName = "width";

CSSStyleDeclaration::CSSStyleDeclaration() {}

CSSStyleDeclaration::~CSSStyleDeclaration() {}

scoped_refptr<PropertyValue> CSSStyleDeclaration::GetPropertyValue(
    const std::string& property_name) {
  switch (property_name.size()) {
    case 5:
      if (LowerCaseEqualsASCII(property_name, kColorPropertyName)) {
        return color();
      }
      if (LowerCaseEqualsASCII(property_name, kWidthPropertyName)) {
        return width();
      }
      return NULL;

    case 6:
      if (LowerCaseEqualsASCII(property_name, kHeightPropertyName)) {
        return height();
      }
      return NULL;

    case 7:
      if (LowerCaseEqualsASCII(property_name, kDisplayPropertyName)) {
        return display();
      }
      return NULL;

    case 8:
      if (LowerCaseEqualsASCII(property_name, kOverflowPropertyName)) {
        return overflow();
      }
      return NULL;

    case 9:
      if (LowerCaseEqualsASCII(property_name, kFontSizePropertyName)) {
        return font_size();
      }
      if (LowerCaseEqualsASCII(property_name, kTransformPropertyName)) {
        return transform();
      }
      return NULL;

    case 11:
      if (LowerCaseEqualsASCII(property_name, kFontFamilyPropertyName)) {
        return font_family();
      }
      return NULL;

    case 16:
      if (LowerCaseEqualsASCII(property_name, kBackgroundColorPropertyName)) {
        return background_color();
      }
      return NULL;

    default:
      return NULL;
  }
}

void CSSStyleDeclaration::SetPropertyValue(
    const std::string& property_name,
    const scoped_refptr<PropertyValue>& property_value) {
  switch (property_name.size()) {
    case 5:
      if (LowerCaseEqualsASCII(property_name, kColorPropertyName)) {
        set_color(property_value);
      } else if (LowerCaseEqualsASCII(property_name, kWidthPropertyName)) {
        set_width(property_value);
      }
      break;

    case 6:
      if (LowerCaseEqualsASCII(property_name, kHeightPropertyName)) {
        set_height(property_value);
      }
      break;

    case 7:
      if (LowerCaseEqualsASCII(property_name, kDisplayPropertyName)) {
        set_display(property_value);
      }
      break;

    case 8:
      if (LowerCaseEqualsASCII(property_name, kOverflowPropertyName)) {
        set_overflow(property_value);
      }
      break;

    case 9:
      if (LowerCaseEqualsASCII(property_name, kFontSizePropertyName)) {
        set_font_size(property_value);
      } else if (LowerCaseEqualsASCII(property_name, kTransformPropertyName)) {
        set_transform(property_value);
      }
      break;

    case 11:
      if (LowerCaseEqualsASCII(property_name, kFontFamilyPropertyName)) {
        set_font_family(property_value);
      }
      break;

    case 16:
      if (LowerCaseEqualsASCII(property_name, kBackgroundColorPropertyName)) {
        set_background_color(property_value);
      }
      break;

    default:
      // 3. If property is not a case-sensitive match for a supported CSS
      // property, terminate this algorithm.
      //   http://dev.w3.org/csswg/cssom/#dom-cssstyledeclaration-setproperty
      break;
  }
}

void CSSStyleDeclaration::AssignFrom(const CSSStyleDeclaration& rhs) {
  set_background_color(rhs.background_color());
  set_color(rhs.color());
  set_display(rhs.display());
  set_font_family(rhs.font_family());
  set_font_size(rhs.font_size());
  set_height(rhs.height());
  set_overflow(rhs.overflow());
  set_transform(rhs.transform());
  set_width(rhs.width());
}

}  // namespace cssom
}  // namespace cobalt

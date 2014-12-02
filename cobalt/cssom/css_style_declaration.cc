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

const char* kBackgroundColorProperty = "background-color";
const char* kColorProperty = "color";
const char* kFontFamilyProperty = "font-family";
const char* kFontSizeProperty = "font-size";

CSSStyleDeclaration::CSSStyleDeclaration() {}

CSSStyleDeclaration::~CSSStyleDeclaration() {}

PropertyValue* CSSStyleDeclaration::GetPropertyValue(
    const std::string& property_name) {
  if (LowerCaseEqualsASCII(property_name, kBackgroundColorProperty)) {
    return background_color();
  }
  if (LowerCaseEqualsASCII(property_name, kColorProperty)) {
    return color();
  }
  if (LowerCaseEqualsASCII(property_name, kFontFamilyProperty)) {
    return font_family();
  }
  if (LowerCaseEqualsASCII(property_name, kFontSizeProperty)) {
    return font_size();
  }
  return NULL;
}

void CSSStyleDeclaration::SetPropertyValue(
    const std::string& property_name,
    scoped_ptr<PropertyValue> property_value) {
  if (LowerCaseEqualsASCII(property_name, kBackgroundColorProperty)) {
    set_background_color(property_value.Pass());
  } else if (LowerCaseEqualsASCII(property_name, kColorProperty)) {
    set_color(property_value.Pass());
  } else if (LowerCaseEqualsASCII(property_name, kFontFamilyProperty)) {
    set_font_family(property_value.Pass());
  } else if (LowerCaseEqualsASCII(property_name, kFontSizeProperty)) {
    set_font_size(property_value.Pass());
  }
}

}  // namespace cssom
}  // namespace cobalt

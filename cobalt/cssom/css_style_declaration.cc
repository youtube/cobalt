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

const char* const kBackgroundColorProperty = "background-color";
const char* const kColorProperty = "color";
const char* const kDisplayProperty = "display";
const char* const kFontFamilyProperty = "font-family";
const char* const kFontSizeProperty = "font-size";
const char* const kHeightProperty = "height";
const char* const kTransformProperty = "transform";
const char* const kWidthProperty = "width";

CSSStyleDeclaration::CSSStyleDeclaration() {}

CSSStyleDeclaration::~CSSStyleDeclaration() {}

// TODO(***REMOVED***): Replace if-else cascade in Get/SetPropertyValue()
//               with hash map.

scoped_refptr<PropertyValue> CSSStyleDeclaration::GetPropertyValue(
    const std::string& property_name) {
  if (LowerCaseEqualsASCII(property_name, kBackgroundColorProperty)) {
    return background_color();
  }
  if (LowerCaseEqualsASCII(property_name, kColorProperty)) {
    return color();
  }
  if (LowerCaseEqualsASCII(property_name, kDisplayProperty)) {
    return display();
  }
  if (LowerCaseEqualsASCII(property_name, kFontFamilyProperty)) {
    return font_family();
  }
  if (LowerCaseEqualsASCII(property_name, kFontSizeProperty)) {
    return font_size();
  }
  if (LowerCaseEqualsASCII(property_name, kHeightProperty)) {
    return height();
  }
  if (LowerCaseEqualsASCII(property_name, kTransformProperty)) {
    return transform();
  }
  if (LowerCaseEqualsASCII(property_name, kWidthProperty)) {
    return width();
  }
  return NULL;
}

void CSSStyleDeclaration::SetPropertyValue(
    const std::string& property_name,
    const scoped_refptr<PropertyValue>& property_value) {
  if (LowerCaseEqualsASCII(property_name, kBackgroundColorProperty)) {
    set_background_color(property_value);
  } else if (LowerCaseEqualsASCII(property_name, kColorProperty)) {
    set_color(property_value);
  } else if (LowerCaseEqualsASCII(property_name, kDisplayProperty)) {
    set_display(property_value);
  } else if (LowerCaseEqualsASCII(property_name, kFontFamilyProperty)) {
    set_font_family(property_value);
  } else if (LowerCaseEqualsASCII(property_name, kFontSizeProperty)) {
    set_font_size(property_value);
  } else if (LowerCaseEqualsASCII(property_name, kHeightProperty)) {
    set_height(property_value);
  } else if (LowerCaseEqualsASCII(property_name, kTransformProperty)) {
    set_transform(property_value);
  } else if (LowerCaseEqualsASCII(property_name, kWidthProperty)) {
    set_width(property_value);
  }
}

void CSSStyleDeclaration::AssignFrom(const CSSStyleDeclaration& rhs) {
  set_background_color(rhs.background_color());
  set_color(rhs.color());
  set_display(rhs.display());
  set_font_family(rhs.font_family());
  set_font_size(rhs.font_size());
  set_height(rhs.height());
  set_transform(rhs.transform());
  set_width(rhs.width());
}

}  // namespace cssom
}  // namespace cobalt

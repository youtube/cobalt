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
#include "cobalt/base/source_location.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/cssom/property_names.h"

namespace cobalt {
namespace cssom {

namespace {
const base::SourceLocation kSourceLocation =
    base::SourceLocation("[object CSSStyleDeclaration]", 1, 1);
}  // namespace

CSSStyleDeclaration::CSSStyleDeclaration(cssom::CSSParser* css_parser)
    : css_parser_(css_parser), mutation_observer_(NULL) {}

CSSStyleDeclaration::~CSSStyleDeclaration() {}

scoped_refptr<PropertyValue>* CSSStyleDeclaration::GetPropertyValueReference(
    const std::string& property_name) {
  switch (property_name.size()) {
    case 5:
      if (LowerCaseEqualsASCII(property_name, kColorPropertyName)) {
        return &color_;
      }
      if (LowerCaseEqualsASCII(property_name, kWidthPropertyName)) {
        return &width_;
      }
      return NULL;

    case 6:
      if (LowerCaseEqualsASCII(property_name, kHeightPropertyName)) {
        return &height_;
      }
      return NULL;

    case 7:
      if (LowerCaseEqualsASCII(property_name, kDisplayPropertyName)) {
        return &display_;
      }
      if (LowerCaseEqualsASCII(property_name, kOpacityPropertyName)) {
        return &opacity_;
      }
      return NULL;

    case 8:
      if (LowerCaseEqualsASCII(property_name, kOverflowPropertyName)) {
        return &overflow_;
      }
      return NULL;

    case 9:
      if (LowerCaseEqualsASCII(property_name, kFontSizePropertyName)) {
        return &font_size_;
      }
      if (LowerCaseEqualsASCII(property_name, kTransformPropertyName)) {
        return &transform_;
      }
      return NULL;

    case 10:
      if (LowerCaseEqualsASCII(property_name, kBackgroundPropertyName)) {
        return &background_;
      }
      return NULL;

    case 11:
      if (LowerCaseEqualsASCII(property_name, kFontFamilyPropertyName)) {
        return &font_family_;
      }
      if (LowerCaseEqualsASCII(property_name, kFontWeightPropertyName)) {
        return &font_weight_;
      }
      return NULL;

    case 13:
      if (LowerCaseEqualsASCII(property_name, kBorderRadiusPropertyName)) {
        return &border_radius_;
      }
      return NULL;

    case 16:
      if (LowerCaseEqualsASCII(property_name, kBackgroundColorPropertyName)) {
        return &background_color_;
      }
      if (LowerCaseEqualsASCII(property_name, kBackgroundImagePropertyName)) {
        return &background_image_;
      }
      return NULL;

    case 19:
      if (LowerCaseEqualsASCII(
              property_name, kTransitionDurationPropertyName)) {
        return &transition_duration_;
      }
      if (LowerCaseEqualsASCII(
              property_name, kTransitionPropertyPropertyName)) {
        return &transition_property_;
      }
      return NULL;

    default:
      return NULL;
  }
}

scoped_refptr<PropertyValue> CSSStyleDeclaration::GetPropertyValue(
    const std::string& property_name) {
  scoped_refptr<PropertyValue>* property_value_reference =
      GetPropertyValueReference(property_name);
  return property_value_reference ? *property_value_reference : NULL;
}

void CSSStyleDeclaration::SetPropertyValue(
    const std::string& property_name,
    const scoped_refptr<PropertyValue>& property_value) {
  scoped_refptr<PropertyValue>* property_value_reference =
      GetPropertyValueReference(property_name);
  if (property_value_reference != NULL) {
    *property_value_reference = property_value;
  } else {
      // 3. If property is not a case-sensitive match for a supported CSS
      // property, terminate this algorithm.
      //   http://dev.w3.org/csswg/cssom/#dom-cssstyledeclaration-setproperty
  }
}

// TODO(***REMOVED***): The getter of css_text returns the result of serializing the
// declarations, which is not required for Performance Spike. This should be
// handled propertly afterwards.
const std::string CSSStyleDeclaration::css_text() const {
  NOTREACHED();
  return NULL;
}

void CSSStyleDeclaration::set_css_text(const std::string& css_text) {
  scoped_refptr<CSSStyleDeclaration> declaration =
      css_parser_->ParseDeclarationList(css_text, kSourceLocation);
  if (declaration) {
    AssignFrom(*declaration.get());

    if (mutation_observer_) {
      // Trigger layout update.
      mutation_observer_->OnMutation();
    }
  }
}

void CSSStyleDeclaration::AssignFrom(const CSSStyleDeclaration& rhs) {
  set_background(rhs.background());
  set_background_color(rhs.background_color());
  set_background_image(rhs.background_image());
  set_border_radius(rhs.border_radius());
  set_color(rhs.color());
  set_display(rhs.display());
  set_font_family(rhs.font_family());
  set_font_size(rhs.font_size());
  set_font_weight(rhs.font_weight());
  set_height(rhs.height());
  set_opacity(rhs.opacity());
  set_overflow(rhs.overflow());
  set_transform(rhs.transform());
  set_transition_duration(rhs.transition_duration());
  set_transition_property(rhs.transition_property());
  set_width(rhs.width());
}

}  // namespace cssom
}  // namespace cobalt

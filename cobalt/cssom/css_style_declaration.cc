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

#include "base/lazy_instance.h"
#include "base/string_util.h"
#include "cobalt/base/source_location.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/property_names.h"
#include "cobalt/cssom/style_sheet.h"
#include "cobalt/cssom/style_sheet_list.h"

namespace cobalt {
namespace cssom {

namespace {

struct NonTrivialStaticFields {
  NonTrivialStaticFields()
      : location(base::SourceLocation("[object CSSStyleDeclaration]", 1, 1)) {}

  const base::SourceLocation location;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

// |non_trivial_static_fields| will be lazily created on the first time it's
// accessed.
base::LazyInstance<NonTrivialStaticFields> non_trivial_static_fields =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

CSSStyleDeclaration::CSSStyleDeclaration(cssom::CSSParser* css_parser)
    : data_(new CSSStyleDeclarationData),
      css_parser_(css_parser),
      mutation_observer_(NULL) {}

CSSStyleDeclaration::CSSStyleDeclaration(
    const scoped_refptr<CSSStyleDeclarationData>& style, CSSParser* css_parser)
    : data_(style), css_parser_(css_parser), mutation_observer_(NULL) {
  DCHECK(data_.get());
}

CSSStyleDeclaration::~CSSStyleDeclaration() {}


std::string CSSStyleDeclaration::GetPropertyValue(
    const std::string& property_name) {
  return data_->GetPropertyValue(property_name)
             ? data_->GetPropertyValue(property_name)->ToString()
             : "";
}

void CSSStyleDeclaration::SetPropertyValue(const std::string& property_name,
                                           const std::string& property_value) {
  *data_->GetPropertyValueReference(property_name) =
      css_parser_->ParsePropertyValue(property_name, property_value,
                                      non_trivial_static_fields.Get().location);

  RecordMutation();
}

std::string CSSStyleDeclaration::background() const {
  return data_->background() ? data_->background()->ToString() : "";
}

void CSSStyleDeclaration::set_background(const std::string& background) {
  DCHECK(css_parser_);
  data_->set_background(css_parser_->ParsePropertyValue(
      kBackgroundPropertyName, background,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::background_color() const {
  return data_->background_color() ? data_->background_color()->ToString() : "";
}

void CSSStyleDeclaration::set_background_color(
    const std::string& background_color) {
  DCHECK(css_parser_);
  data_->set_background_color(css_parser_->ParsePropertyValue(
      kBackgroundColorPropertyName, background_color,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::background_image() const {
  return data_->background_image() ? data_->background_image()->ToString() : "";
}

void CSSStyleDeclaration::set_background_image(
    const std::string& background_image) {
  DCHECK(css_parser_);
  data_->set_background_image(css_parser_->ParsePropertyValue(
      kBackgroundImagePropertyName, background_image,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::border_radius() const {
  return data_->border_radius() ? data_->border_radius()->ToString() : "";
}

void CSSStyleDeclaration::set_border_radius(const std::string& border_radius) {
  DCHECK(css_parser_);
  data_->set_border_radius(css_parser_->ParsePropertyValue(
      kBorderRadiusPropertyName, border_radius,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::color() const {
  return data_->color() ? data_->color()->ToString() : "";
}

void CSSStyleDeclaration::set_color(const std::string& color) {
  DCHECK(css_parser_);
  data_->set_color(css_parser_->ParsePropertyValue(
      kColorPropertyName, color, non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::display() const {
  return data_->display() ? data_->display()->ToString() : "";
}

void CSSStyleDeclaration::set_display(const std::string& display) {
  DCHECK(css_parser_);
  data_->set_display(css_parser_->ParsePropertyValue(
      kDisplayPropertyName, display, non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::font_family() const {
  return data_->font_family() ? data_->font_family()->ToString() : "";
}

void CSSStyleDeclaration::set_font_family(const std::string& font_family) {
  DCHECK(css_parser_);
  data_->set_font_family(css_parser_->ParsePropertyValue(
      kFontFamilyPropertyName, font_family,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::font_size() const {
  return data_->font_size() ? data_->font_size()->ToString() : "";
}

void CSSStyleDeclaration::set_font_size(const std::string& font_size) {
  DCHECK(css_parser_);
  data_->set_font_size(css_parser_->ParsePropertyValue(
      kFontSizePropertyName, font_size,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::font_weight() const {
  return data_->font_weight() ? data_->font_weight()->ToString() : "";
}

void CSSStyleDeclaration::set_font_weight(const std::string& font_weight) {
  DCHECK(css_parser_);
  data_->set_font_weight(css_parser_->ParsePropertyValue(
      kFontWeightPropertyName, font_weight,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::height() const {
  return data_->height() ? data_->height()->ToString() : "";
}

void CSSStyleDeclaration::set_height(const std::string& height) {
  DCHECK(css_parser_);
  data_->set_height(css_parser_->ParsePropertyValue(
      kHeightPropertyName, height, non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::line_height() const {
  return data_->line_height() ? data_->line_height()->ToString() : "";
}

void CSSStyleDeclaration::set_line_height(const std::string& line_height) {
  DCHECK(css_parser_);
  data_->set_line_height(css_parser_->ParsePropertyValue(
      kLineHeightPropertyName, line_height,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::opacity() const {
  return data_->opacity() ? data_->opacity()->ToString() : "";
}

void CSSStyleDeclaration::set_opacity(const std::string& opacity) {
  DCHECK(css_parser_);
  data_->set_opacity(css_parser_->ParsePropertyValue(
      kOpacityPropertyName, opacity, non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::overflow() const {
  return data_->overflow() ? data_->overflow()->ToString() : "";
}

void CSSStyleDeclaration::set_overflow(const std::string& overflow) {
  DCHECK(css_parser_);
  data_->set_overflow(css_parser_->ParsePropertyValue(
      kOverflowPropertyName, overflow,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::transform() const {
  return data_->transform() ? data_->transform()->ToString() : "";
}

void CSSStyleDeclaration::set_transform(const std::string& transform) {
  DCHECK(css_parser_);
  data_->set_transform(css_parser_->ParsePropertyValue(
      kTransformPropertyName, transform,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::transition_delay() const {
  return data_->transition_delay() ? data_->transition_delay()->ToString() : "";
}

void CSSStyleDeclaration::set_transition_delay(
    const std::string& transition_delay) {
  DCHECK(css_parser_);
  data_->set_transition_delay(css_parser_->ParsePropertyValue(
      kTransitionDelayPropertyName, transition_delay,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::transition_duration() const {
  return data_->transition_duration() ? data_->transition_duration()->ToString()
                                      : "";
}

void CSSStyleDeclaration::set_transition_duration(
    const std::string& transition_duration) {
  DCHECK(css_parser_);
  data_->set_transition_duration(css_parser_->ParsePropertyValue(
      kTransitionDurationPropertyName, transition_duration,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::transition_property() const {
  return data_->transition_property() ? data_->transition_property()->ToString()
                                      : "";
}

void CSSStyleDeclaration::set_transition_property(
    const std::string& transition_property) {
  DCHECK(css_parser_);
  data_->set_transition_property(css_parser_->ParsePropertyValue(
      kTransitionPropertyPropertyName, transition_property,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::transition_timing_function() const {
  return data_->transition_timing_function()
             ? data_->transition_timing_function()->ToString()
             : "";
}

void CSSStyleDeclaration::set_transition_timing_function(
    const std::string& transition_timing_function) {
  DCHECK(css_parser_);
  data_->set_transition_timing_function(css_parser_->ParsePropertyValue(
      kTransitionTimingFunctionPropertyName, transition_timing_function,
      non_trivial_static_fields.Get().location));

  RecordMutation();
}

std::string CSSStyleDeclaration::width() const {
  return data_->width() ? data_->width()->ToString() : "";
}

void CSSStyleDeclaration::set_width(const std::string& width) {
  DCHECK(css_parser_);
  data_->set_width(css_parser_->ParsePropertyValue(
      kWidthPropertyName, width, non_trivial_static_fields.Get().location));

  RecordMutation();
}

// TODO(***REMOVED***): The getter of css_text returns the result of serializing the
// declarations, which is not required for Performance Spike. This should be
// handled propertly afterwards.
std::string CSSStyleDeclaration::css_text() const {
  NOTREACHED();
  return "";
}

void CSSStyleDeclaration::set_css_text(const std::string& css_text) {
  DCHECK(css_parser_);
  scoped_refptr<CSSStyleDeclarationData> declaration =
      css_parser_->ParseDeclarationList(
          css_text, non_trivial_static_fields.Get().location);

  if (declaration) {
    data_ = declaration;
    RecordMutation();
  }
}

void CSSStyleDeclaration::AttachToStyleSheet(StyleSheet* style_sheet) {
  DCHECK(style_sheet != NULL);
  DCHECK(style_sheet->ParentStyleSheetList() != NULL);
  mutation_observer_ = style_sheet->ParentStyleSheetList()->mutation_observer();
}

void CSSStyleDeclaration::RecordMutation() {
  if (mutation_observer_) {
    // Trigger layout update.
    mutation_observer_->OnMutation();
  }
}

}  // namespace cssom
}  // namespace cobalt

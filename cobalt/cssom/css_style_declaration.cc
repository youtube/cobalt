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
  DCHECK(css_parser_);
  css_parser_->ParsePropertyIntoStyle(property_name, property_value,
                                      non_trivial_static_fields.Get().location,
                                      data_.get());

  RecordMutation();
}

std::string CSSStyleDeclaration::background() const {
  // In order to implement this properly we must either save the incoming string
  // values when they are being set, or combine the results of getting the
  // styles from all the other background properties.
  NOTIMPLEMENTED();
  return "";
}

void CSSStyleDeclaration::set_background(const std::string& background) {
  SetPropertyValue(kBackgroundPropertyName, background);
}

std::string CSSStyleDeclaration::background_color() const {
  return data_->background_color() ? data_->background_color()->ToString() : "";
}

void CSSStyleDeclaration::set_background_color(
    const std::string& background_color) {
  SetPropertyValue(kBackgroundColorPropertyName, background_color);
}

std::string CSSStyleDeclaration::background_image() const {
  return data_->background_image() ? data_->background_image()->ToString() : "";
}

void CSSStyleDeclaration::set_background_image(
    const std::string& background_image) {
  SetPropertyValue(kBackgroundImagePropertyName, background_image);
}

std::string CSSStyleDeclaration::background_position() const {
  return data_->background_position() ? data_->background_position()->ToString()
                                      : "";
}

void CSSStyleDeclaration::set_background_position(
    const std::string& background_position) {
  SetPropertyValue(kBackgroundPositionPropertyName, background_position);
}

std::string CSSStyleDeclaration::background_size() const {
  return data_->background_size() ? data_->background_size()->ToString() : "";
}

void CSSStyleDeclaration::set_background_size(
    const std::string& background_size) {
  SetPropertyValue(kBackgroundSizePropertyName, background_size);
}

std::string CSSStyleDeclaration::border_radius() const {
  return data_->border_radius() ? data_->border_radius()->ToString() : "";
}

void CSSStyleDeclaration::set_border_radius(const std::string& border_radius) {
  SetPropertyValue(kBorderRadiusPropertyName, border_radius);
}

std::string CSSStyleDeclaration::bottom() const {
  return data_->bottom() ? data_->bottom()->ToString() : "";
}

void CSSStyleDeclaration::set_bottom(const std::string& bottom) {
  SetPropertyValue(kBottomPropertyName, bottom);
}

std::string CSSStyleDeclaration::color() const {
  return data_->color() ? data_->color()->ToString() : "";
}

void CSSStyleDeclaration::set_color(const std::string& color) {
  SetPropertyValue(kColorPropertyName, color);
}

std::string CSSStyleDeclaration::display() const {
  return data_->display() ? data_->display()->ToString() : "";
}

void CSSStyleDeclaration::set_display(const std::string& display) {
  SetPropertyValue(kDisplayPropertyName, display);
}

std::string CSSStyleDeclaration::font_family() const {
  return data_->font_family() ? data_->font_family()->ToString() : "";
}

void CSSStyleDeclaration::set_font_family(const std::string& font_family) {
  SetPropertyValue(kFontFamilyPropertyName, font_family);
}

std::string CSSStyleDeclaration::font_size() const {
  return data_->font_size() ? data_->font_size()->ToString() : "";
}

void CSSStyleDeclaration::set_font_size(const std::string& font_size) {
  SetPropertyValue(kFontSizePropertyName, font_size);
}

std::string CSSStyleDeclaration::font_weight() const {
  return data_->font_weight() ? data_->font_weight()->ToString() : "";
}

void CSSStyleDeclaration::set_font_weight(const std::string& font_weight) {
  SetPropertyValue(kFontWeightPropertyName, font_weight);
}

std::string CSSStyleDeclaration::height() const {
  return data_->height() ? data_->height()->ToString() : "";
}

void CSSStyleDeclaration::set_height(const std::string& height) {
  SetPropertyValue(kHeightPropertyName, height);
}

std::string CSSStyleDeclaration::left() const {
  return data_->left() ? data_->left()->ToString() : "";
}

void CSSStyleDeclaration::set_left(const std::string& left) {
  SetPropertyValue(kLeftPropertyName, left);
}

std::string CSSStyleDeclaration::line_height() const {
  return data_->line_height() ? data_->line_height()->ToString() : "";
}

void CSSStyleDeclaration::set_line_height(const std::string& line_height) {
  SetPropertyValue(kLineHeightPropertyName, line_height);
}

std::string CSSStyleDeclaration::opacity() const {
  return data_->opacity() ? data_->opacity()->ToString() : "";
}

void CSSStyleDeclaration::set_opacity(const std::string& opacity) {
  SetPropertyValue(kOpacityPropertyName, opacity);
}

std::string CSSStyleDeclaration::overflow() const {
  return data_->overflow() ? data_->overflow()->ToString() : "";
}

void CSSStyleDeclaration::set_overflow(const std::string& overflow) {
  SetPropertyValue(kOverflowPropertyName, overflow);
}

std::string CSSStyleDeclaration::position() const {
  return data_->position() ? data_->position()->ToString() : "";
}

void CSSStyleDeclaration::set_position(const std::string& position) {
  SetPropertyValue(kPositionPropertyName, position);
}

std::string CSSStyleDeclaration::right() const {
  return data_->right() ? data_->right()->ToString() : "";
}

void CSSStyleDeclaration::set_right(const std::string& right) {
  SetPropertyValue(kRightPropertyName, right);
}

std::string CSSStyleDeclaration::top() const {
  return data_->top() ? data_->top()->ToString() : "";
}

void CSSStyleDeclaration::set_top(const std::string& top) {
  SetPropertyValue(kTopPropertyName, top);
}

std::string CSSStyleDeclaration::transform() const {
  return data_->transform() ? data_->transform()->ToString() : "";
}

void CSSStyleDeclaration::set_transform(const std::string& transform) {
  SetPropertyValue(kTransformPropertyName, transform);
}

std::string CSSStyleDeclaration::transition() const {
  // In order to implement this properly we must either save the incoming string
  // values when they are being set, or combine the results of getting the
  // styles from all the other transition properties.
  NOTIMPLEMENTED();
  return "";
}

void CSSStyleDeclaration::set_transition(const std::string& transition) {
  SetPropertyValue(kTransitionPropertyName, transition);
}

std::string CSSStyleDeclaration::transition_delay() const {
  return data_->transition_delay() ? data_->transition_delay()->ToString() : "";
}

void CSSStyleDeclaration::set_transition_delay(
    const std::string& transition_delay) {
  SetPropertyValue(kTransitionDelayPropertyName, transition_delay);
}

std::string CSSStyleDeclaration::transition_duration() const {
  return data_->transition_duration() ? data_->transition_duration()->ToString()
                                      : "";
}

void CSSStyleDeclaration::set_transition_duration(
    const std::string& transition_duration) {
  SetPropertyValue(kTransitionDurationPropertyName, transition_duration);
}

std::string CSSStyleDeclaration::transition_property() const {
  return data_->transition_property() ? data_->transition_property()->ToString()
                                      : "";
}

void CSSStyleDeclaration::set_transition_property(
    const std::string& transition_property) {
  SetPropertyValue(kTransitionPropertyPropertyName, transition_property);
}

std::string CSSStyleDeclaration::transition_timing_function() const {
  return data_->transition_timing_function()
             ? data_->transition_timing_function()->ToString()
             : "";
}

void CSSStyleDeclaration::set_transition_timing_function(
    const std::string& transition_timing_function) {
  SetPropertyValue(kTransitionTimingFunctionPropertyName,
                   transition_timing_function);
}

std::string CSSStyleDeclaration::vertical_align() const {
  return data_->vertical_align() ? data_->vertical_align()->ToString() : "";
}

void CSSStyleDeclaration::set_vertical_align(
    const std::string& vertical_align) {
  SetPropertyValue(kVerticalAlignPropertyName, vertical_align);
}

std::string CSSStyleDeclaration::width() const {
  return data_->width() ? data_->width()->ToString() : "";
}

void CSSStyleDeclaration::set_width(const std::string& width) {
  SetPropertyValue(kWidthPropertyName, width);
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
  DCHECK(style_sheet);
  DCHECK(style_sheet->ParentStyleSheetList());
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

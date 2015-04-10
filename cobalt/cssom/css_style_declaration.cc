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

namespace cobalt {
namespace cssom {

namespace {

class SourceLocationAdapter {
 public:
  SourceLocationAdapter()
      : location_(base::SourceLocation("[object CSSStyleDeclaration]", 1, 1)) {}

  const base::SourceLocation& location() const { return location_; }

 private:
  const base::SourceLocation location_;
};

// |kSourceLocationAdapter| will be lazily created on the first time it's
// accessed.
base::LazyInstance<SourceLocationAdapter> kSourceLocationAdapter =
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


base::optional<std::string> CSSStyleDeclaration::GetPropertyValue(
    const std::string& property_name) {
  return data_->GetPropertyValue(property_name)
             ? data_->GetPropertyValue(property_name)->ToString()
             : base::nullopt;
}

void CSSStyleDeclaration::SetPropertyValue(const std::string& property_name,
                                           const std::string& property_value) {
  *data_->GetPropertyValueReference(property_name) =
      css_parser_->ParsePropertyValue(property_name, property_value,
                                      kSourceLocationAdapter.Get().location());

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::background() const {
  return data_->background() ? data_->background()->ToString() : base::nullopt;
}

void CSSStyleDeclaration::set_background(
    const base::optional<std::string>& background) {
  DCHECK(css_parser_);
  data_->set_background(css_parser_->ParsePropertyValue(
      kBackgroundPropertyName, background.value(),
      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::background_color() const {
  return data_->background_color() ? data_->background_color()->ToString()
                                   : base::nullopt;
}

void CSSStyleDeclaration::set_background_color(
    const base::optional<std::string>& background_color) {
  DCHECK(css_parser_);
  data_->set_background_color(css_parser_->ParsePropertyValue(
      kBackgroundColorPropertyName, background_color.value(),
      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::background_image() const {
  return data_->background_image() ? data_->background_image()->ToString()
                                   : base::nullopt;
}

void CSSStyleDeclaration::set_background_image(
    const base::optional<std::string>& background_image) {
  DCHECK(css_parser_);
  data_->set_background_image(css_parser_->ParsePropertyValue(
      kBackgroundImagePropertyName, background_image.value(),
      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::border_radius() const {
  return data_->border_radius() ? data_->border_radius()->ToString()
                                : base::nullopt;
}

void CSSStyleDeclaration::set_border_radius(
    const base::optional<std::string>& border_radius) {
  DCHECK(css_parser_);
  data_->set_border_radius(css_parser_->ParsePropertyValue(
      kBorderRadiusPropertyName, border_radius.value(),
      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::color() const {
  return data_->color() ? data_->color()->ToString() : base::nullopt;
}

void CSSStyleDeclaration::set_color(const base::optional<std::string>& color) {
  DCHECK(css_parser_);
  data_->set_color(
      css_parser_->ParsePropertyValue(kColorPropertyName, color.value(),
                                      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::display() const {
  return data_->display() ? data_->display()->ToString() : base::nullopt;
}

void CSSStyleDeclaration::set_display(
    const base::optional<std::string>& display) {
  DCHECK(css_parser_);
  data_->set_display(
      css_parser_->ParsePropertyValue(kDisplayPropertyName, display.value(),
                                      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::font_family() const {
  return data_->font_family() ? data_->font_family()->ToString()
                              : base::nullopt;
}

void CSSStyleDeclaration::set_font_family(
    const base::optional<std::string>& font_family) {
  DCHECK(css_parser_);
  data_->set_font_family(css_parser_->ParsePropertyValue(
      kFontFamilyPropertyName, font_family.value(),
      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::font_size() const {
  return data_->font_size() ? data_->font_size()->ToString() : base::nullopt;
}

void CSSStyleDeclaration::set_font_size(
    const base::optional<std::string>& font_size) {
  DCHECK(css_parser_);
  data_->set_font_size(
      css_parser_->ParsePropertyValue(kFontSizePropertyName, font_size.value(),
                                      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::font_weight() const {
  return data_->font_weight() ? data_->font_weight()->ToString()
                              : base::nullopt;
}

void CSSStyleDeclaration::set_font_weight(
    const base::optional<std::string>& font_weight) {
  DCHECK(css_parser_);
  data_->set_font_weight(css_parser_->ParsePropertyValue(
      kFontWeightPropertyName, font_weight.value(),
      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::height() const {
  return data_->height() ? data_->height()->ToString() : base::nullopt;
}

void CSSStyleDeclaration::set_height(
    const base::optional<std::string>& height) {
  DCHECK(css_parser_);
  data_->set_height(
      css_parser_->ParsePropertyValue(kHeightPropertyName, height.value(),
                                      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::opacity() const {
  return data_->opacity() ? data_->opacity()->ToString() : base::nullopt;
}

void CSSStyleDeclaration::set_opacity(
    const base::optional<std::string>& opacity) {
  DCHECK(css_parser_);
  data_->set_opacity(
      css_parser_->ParsePropertyValue(kOpacityPropertyName, opacity.value(),
                                      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::overflow() const {
  return data_->overflow() ? data_->overflow()->ToString() : base::nullopt;
}

void CSSStyleDeclaration::set_overflow(
    const base::optional<std::string>& overflow) {
  DCHECK(css_parser_);
  data_->set_overflow(
      css_parser_->ParsePropertyValue(kOverflowPropertyName, overflow.value(),
                                      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::transform() const {
  return data_->transform() ? data_->transform()->ToString() : base::nullopt;
}

void CSSStyleDeclaration::set_transform(
    const base::optional<std::string>& transform) {
  DCHECK(css_parser_);
  data_->set_transform(
      css_parser_->ParsePropertyValue(kTransformPropertyName, transform.value(),
                                      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::transition_duration() const {
  return data_->transition_duration() ? data_->transition_duration()->ToString()
                                      : base::nullopt;
}

void CSSStyleDeclaration::set_transition_duration(
    const base::optional<std::string>& transition_duration) {
  DCHECK(css_parser_);
  data_->set_transition_duration(css_parser_->ParsePropertyValue(
      kTransformPropertyName, transition_duration.value(),
      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::transition_property() const {
  return data_->transition_property() ? data_->transition_property()->ToString()
                                      : base::nullopt;
}

void CSSStyleDeclaration::set_transition_property(
    const base::optional<std::string>& transition_property) {
  DCHECK(css_parser_);
  data_->set_transition_property(css_parser_->ParsePropertyValue(
      kTransformPropertyName, transition_property.value(),
      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

base::optional<std::string> CSSStyleDeclaration::width() const {
  return data_->width() ? data_->width()->ToString() : base::nullopt;
}

void CSSStyleDeclaration::set_width(const base::optional<std::string>& width) {
  DCHECK(css_parser_);
  data_->set_width(
      css_parser_->ParsePropertyValue(kWidthPropertyName, width.value(),
                                      kSourceLocationAdapter.Get().location()));

  OnMutation();
}

// TODO(***REMOVED***): The getter of css_text returns the result of serializing the
// declarations, which is not required for Performance Spike. This should be
// handled propertly afterwards.
base::optional<std::string> CSSStyleDeclaration::css_text() const {
  NOTREACHED();
  return base::nullopt;
}

void CSSStyleDeclaration::set_css_text(
    const base::optional<std::string>& css_text) {
  DCHECK(css_parser_);
  scoped_refptr<CSSStyleDeclarationData> declaration =
      css_parser_->ParseDeclarationList(
          css_text.value(), kSourceLocationAdapter.Get().location());

  if (declaration) {
    data_ = declaration;
    OnMutation();
  }
}

void CSSStyleDeclaration::OnMutation() {
  if (mutation_observer_) {
    // Trigger layout update.
    mutation_observer_->OnMutation();
  }
}

}  // namespace cssom
}  // namespace cobalt

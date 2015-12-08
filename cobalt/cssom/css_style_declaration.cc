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
#include "cobalt/cssom/css_declaration_util.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/cssom/property_definitions.h"

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

CSSStyleDeclaration::CSSStyleDeclaration(CSSParser* css_parser)
    : data_(new CSSStyleDeclarationData),
      css_parser_(css_parser),
      mutation_observer_(NULL) {}

CSSStyleDeclaration::CSSStyleDeclaration(
    const scoped_refptr<CSSStyleDeclarationData>& style, CSSParser* css_parser)
    : data_(style), css_parser_(css_parser), mutation_observer_(NULL) {
  DCHECK(data_.get());
}

std::string CSSStyleDeclaration::animation() const {
  NOTIMPLEMENTED();
  return "";
}

void CSSStyleDeclaration::set_animation(const std::string& animation) {
  SetPropertyValueStringByKey(kAnimationProperty, animation);
}

std::string CSSStyleDeclaration::animation_delay() const {
  return data_->GetDeclaredPropertyValueString(kAnimationDelayProperty);
}

void CSSStyleDeclaration::set_animation_delay(
    const std::string& animation_delay) {
  SetPropertyValueStringByKey(kAnimationDelayProperty, animation_delay);
}

std::string CSSStyleDeclaration::animation_direction() const {
  return data_->GetDeclaredPropertyValueString(kAnimationDirectionProperty);
}

void CSSStyleDeclaration::set_animation_direction(
    const std::string& animation_direction) {
  SetPropertyValueStringByKey(kAnimationDirectionProperty, animation_direction);
}

std::string CSSStyleDeclaration::animation_duration() const {
  return data_->GetDeclaredPropertyValueString(kAnimationDurationProperty);
}

void CSSStyleDeclaration::set_animation_duration(
    const std::string& animation_duration) {
  SetPropertyValueStringByKey(kAnimationDurationProperty, animation_duration);
}

std::string CSSStyleDeclaration::animation_fill_mode() const {
  return data_->GetDeclaredPropertyValueString(kAnimationFillModeProperty);
}

void CSSStyleDeclaration::set_animation_fill_mode(
    const std::string& animation_fill_mode) {
  SetPropertyValueStringByKey(kAnimationFillModeProperty, animation_fill_mode);
}

std::string CSSStyleDeclaration::animation_iteration_count() const {
  return data_->GetDeclaredPropertyValueString(
      kAnimationIterationCountProperty);
}

void CSSStyleDeclaration::set_animation_iteration_count(
    const std::string& animation_iteration_count) {
  SetPropertyValueStringByKey(kAnimationIterationCountProperty,
                              animation_iteration_count);
}

std::string CSSStyleDeclaration::animation_name() const {
  return data_->GetDeclaredPropertyValueString(kAnimationNameProperty);
}

void CSSStyleDeclaration::set_animation_name(
    const std::string& animation_name) {
  SetPropertyValueStringByKey(kAnimationNameProperty, animation_name);
}

std::string CSSStyleDeclaration::animation_timing_function() const {
  return data_->GetDeclaredPropertyValueString(
      kAnimationTimingFunctionProperty);
}

void CSSStyleDeclaration::set_animation_timing_function(
    const std::string& animation_timing_function) {
  SetPropertyValueStringByKey(kAnimationTimingFunctionProperty,
                              animation_timing_function);
}

std::string CSSStyleDeclaration::background() const {
  // In order to implement this properly we must either save the incoming string
  // values when they are being set, or combine the results of getting the
  // styles from all the other background properties.
  NOTIMPLEMENTED();
  return "";
}

void CSSStyleDeclaration::set_background(const std::string& background) {
  SetPropertyValueStringByKey(kBackgroundProperty, background);
}

std::string CSSStyleDeclaration::background_color() const {
  return data_->GetDeclaredPropertyValueString(kBackgroundColorProperty);
}

void CSSStyleDeclaration::set_background_color(
    const std::string& background_color) {
  SetPropertyValueStringByKey(kBackgroundColorProperty, background_color);
}

std::string CSSStyleDeclaration::background_image() const {
  return data_->GetDeclaredPropertyValueString(kBackgroundImageProperty);
}

void CSSStyleDeclaration::set_background_image(
    const std::string& background_image) {
  SetPropertyValueStringByKey(kBackgroundImageProperty, background_image);
}

std::string CSSStyleDeclaration::background_position() const {
  return data_->GetDeclaredPropertyValueString(kBackgroundPositionProperty);
}

void CSSStyleDeclaration::set_background_position(
    const std::string& background_position) {
  SetPropertyValueStringByKey(kBackgroundPositionProperty, background_position);
}

std::string CSSStyleDeclaration::background_repeat() const {
  return data_->GetDeclaredPropertyValueString(kBackgroundRepeatProperty);
}

void CSSStyleDeclaration::set_background_repeat(
    const std::string& background_repeat) {
  SetPropertyValueStringByKey(kBackgroundRepeatProperty, background_repeat);
}

std::string CSSStyleDeclaration::background_size() const {
  return data_->GetDeclaredPropertyValueString(kBackgroundSizeProperty);
}

void CSSStyleDeclaration::set_background_size(
    const std::string& background_size) {
  SetPropertyValueStringByKey(kBackgroundSizeProperty, background_size);
}

std::string CSSStyleDeclaration::border() const {
  return data_->GetDeclaredPropertyValueString(kBorderProperty);
}

void CSSStyleDeclaration::set_border(const std::string& border) {
  SetPropertyValueStringByKey(kBorderProperty, border);
}

std::string CSSStyleDeclaration::border_color() const {
  return data_->GetDeclaredPropertyValueString(kBorderColorProperty);
}

void CSSStyleDeclaration::set_border_color(const std::string& border_color) {
  SetPropertyValueStringByKey(kBorderColorProperty, border_color);
}

std::string CSSStyleDeclaration::border_style() const {
  return data_->GetDeclaredPropertyValueString(kBorderStyleProperty);
}

void CSSStyleDeclaration::set_border_style(const std::string& border_style) {
  SetPropertyValueStringByKey(kBorderStyleProperty, border_style);
}

std::string CSSStyleDeclaration::border_width() const {
  return data_->GetDeclaredPropertyValueString(kBorderWidthProperty);
}

void CSSStyleDeclaration::set_border_width(const std::string& border_width) {
  SetPropertyValueStringByKey(kBorderWidthProperty, border_width);
}

std::string CSSStyleDeclaration::border_radius() const {
  return data_->GetDeclaredPropertyValueString(kBorderRadiusProperty);
}

void CSSStyleDeclaration::set_border_radius(const std::string& border_radius) {
  SetPropertyValueStringByKey(kBorderRadiusProperty, border_radius);
}

std::string CSSStyleDeclaration::bottom() const {
  return data_->GetDeclaredPropertyValueString(kBottomProperty);
}

void CSSStyleDeclaration::set_bottom(const std::string& bottom) {
  SetPropertyValueStringByKey(kBottomProperty, bottom);
}

std::string CSSStyleDeclaration::color() const {
  return data_->GetDeclaredPropertyValueString(kColorProperty);
}

void CSSStyleDeclaration::set_color(const std::string& color) {
  SetPropertyValueStringByKey(kColorProperty, color);
}

std::string CSSStyleDeclaration::content() const {
  return data_->GetDeclaredPropertyValueString(kContentProperty);
}

void CSSStyleDeclaration::set_content(const std::string& content) {
  SetPropertyValueStringByKey(kContentProperty, content);
}

std::string CSSStyleDeclaration::display() const {
  return data_->GetDeclaredPropertyValueString(kDisplayProperty);
}

void CSSStyleDeclaration::set_display(const std::string& display) {
  SetPropertyValueStringByKey(kDisplayProperty, display);
}

std::string CSSStyleDeclaration::font() const {
  NOTIMPLEMENTED();
  return "";
}

void CSSStyleDeclaration::set_font(const std::string& font) {
  SetPropertyValueStringByKey(kFontProperty, font);
}

std::string CSSStyleDeclaration::font_family() const {
  return data_->GetDeclaredPropertyValueString(kFontFamilyProperty);
}

void CSSStyleDeclaration::set_font_family(const std::string& font_family) {
  SetPropertyValueStringByKey(kFontFamilyProperty, font_family);
}

std::string CSSStyleDeclaration::font_size() const {
  return data_->GetDeclaredPropertyValueString(kFontSizeProperty);
}

void CSSStyleDeclaration::set_font_size(const std::string& font_size) {
  SetPropertyValueStringByKey(kFontSizeProperty, font_size);
}

std::string CSSStyleDeclaration::font_style() const {
  return data_->GetDeclaredPropertyValueString(kFontStyleProperty);
}

void CSSStyleDeclaration::set_font_style(const std::string& font_style) {
  SetPropertyValueStringByKey(kFontStyleProperty, font_style);
}

std::string CSSStyleDeclaration::font_weight() const {
  return data_->GetDeclaredPropertyValueString(kFontWeightProperty);
}

void CSSStyleDeclaration::set_font_weight(const std::string& font_weight) {
  SetPropertyValueStringByKey(kFontWeightProperty, font_weight);
}

std::string CSSStyleDeclaration::height() const {
  return data_->GetDeclaredPropertyValueString(kHeightProperty);
}

void CSSStyleDeclaration::set_height(const std::string& height) {
  SetPropertyValueStringByKey(kHeightProperty, height);
}

std::string CSSStyleDeclaration::left() const {
  return data_->GetDeclaredPropertyValueString(kLeftProperty);
}

void CSSStyleDeclaration::set_left(const std::string& left) {
  SetPropertyValueStringByKey(kLeftProperty, left);
}

std::string CSSStyleDeclaration::line_height() const {
  return data_->GetDeclaredPropertyValueString(kLineHeightProperty);
}

void CSSStyleDeclaration::set_line_height(const std::string& line_height) {
  SetPropertyValueStringByKey(kLineHeightProperty, line_height);
}

std::string CSSStyleDeclaration::max_height() const {
  return data_->GetDeclaredPropertyValueString(kMaxHeightProperty);
}

void CSSStyleDeclaration::set_max_height(const std::string& max_height) {
  SetPropertyValueStringByKey(kMaxHeightProperty, max_height);
}

std::string CSSStyleDeclaration::max_width() const {
  return data_->GetDeclaredPropertyValueString(kMaxWidthProperty);
}

void CSSStyleDeclaration::set_max_width(const std::string& max_width) {
  SetPropertyValueStringByKey(kMaxWidthProperty, max_width);
}

std::string CSSStyleDeclaration::min_height() const {
  return data_->GetDeclaredPropertyValueString(kMinHeightProperty);
}

void CSSStyleDeclaration::set_min_height(const std::string& min_height) {
  SetPropertyValueStringByKey(kMinHeightProperty, min_height);
}

std::string CSSStyleDeclaration::min_width() const {
  return data_->GetDeclaredPropertyValueString(kMinWidthProperty);
}

void CSSStyleDeclaration::set_min_width(const std::string& min_width) {
  SetPropertyValueStringByKey(kMinWidthProperty, min_width);
}

std::string CSSStyleDeclaration::opacity() const {
  return data_->GetDeclaredPropertyValueString(kOpacityProperty);
}

void CSSStyleDeclaration::set_opacity(const std::string& opacity) {
  SetPropertyValueStringByKey(kOpacityProperty, opacity);
}

std::string CSSStyleDeclaration::overflow() const {
  return data_->GetDeclaredPropertyValueString(kOverflowProperty);
}

void CSSStyleDeclaration::set_overflow(const std::string& overflow) {
  SetPropertyValueStringByKey(kOverflowProperty, overflow);
}

std::string CSSStyleDeclaration::overflow_wrap() const {
  return data_->GetDeclaredPropertyValueString(kOverflowWrapProperty);
}

void CSSStyleDeclaration::set_overflow_wrap(const std::string& overflow_wrap) {
  SetPropertyValueStringByKey(kOverflowWrapProperty, overflow_wrap);
}

std::string CSSStyleDeclaration::position() const {
  return data_->GetDeclaredPropertyValueString(kPositionProperty);
}

void CSSStyleDeclaration::set_position(const std::string& position) {
  SetPropertyValueStringByKey(kPositionProperty, position);
}

std::string CSSStyleDeclaration::right() const {
  return data_->GetDeclaredPropertyValueString(kRightProperty);
}

void CSSStyleDeclaration::set_right(const std::string& right) {
  SetPropertyValueStringByKey(kRightProperty, right);
}

std::string CSSStyleDeclaration::tab_size() const {
  return data_->GetDeclaredPropertyValueString(kTabSizeProperty);
}

void CSSStyleDeclaration::set_tab_size(const std::string& tab_size) {
  SetPropertyValueStringByKey(kTabSizeProperty, tab_size);
}

std::string CSSStyleDeclaration::text_align() const {
  return data_->GetDeclaredPropertyValueString(kTextAlignProperty);
}

void CSSStyleDeclaration::set_text_align(const std::string& text_align) {
  SetPropertyValueStringByKey(kTextAlignProperty, text_align);
}

std::string CSSStyleDeclaration::text_indent() const {
  return data_->GetDeclaredPropertyValueString(kTextIndentProperty);
}

void CSSStyleDeclaration::set_text_indent(const std::string& text_indent) {
  SetPropertyValueStringByKey(kTextIndentProperty, text_indent);
}

std::string CSSStyleDeclaration::text_overflow() const {
  return data_->GetDeclaredPropertyValueString(kTextOverflowProperty);
}

void CSSStyleDeclaration::set_text_overflow(const std::string& text_overflow) {
  SetPropertyValueStringByKey(kTextOverflowProperty, text_overflow);
}

std::string CSSStyleDeclaration::text_transform() const {
  return data_->GetDeclaredPropertyValueString(kTextTransformProperty);
}

void CSSStyleDeclaration::set_text_transform(
    const std::string& text_transform) {
  SetPropertyValueStringByKey(kTextTransformProperty, text_transform);
}

std::string CSSStyleDeclaration::top() const {
  return data_->GetDeclaredPropertyValueString(kTopProperty);
}

void CSSStyleDeclaration::set_top(const std::string& top) {
  SetPropertyValueStringByKey(kTopProperty, top);
}

std::string CSSStyleDeclaration::transform() const {
  return data_->GetDeclaredPropertyValueString(kTransformProperty);
}

void CSSStyleDeclaration::set_transform(const std::string& transform) {
  SetPropertyValueStringByKey(kTransformProperty, transform);
}

std::string CSSStyleDeclaration::transition() const {
  // In order to implement this properly we must either save the incoming string
  // values when they are being set, or combine the results of getting the
  // styles from all the other transition properties.
  NOTIMPLEMENTED();
  return "";
}

void CSSStyleDeclaration::set_transition(const std::string& transition) {
  SetPropertyValueStringByKey(kTransitionProperty, transition);
}

std::string CSSStyleDeclaration::transition_delay() const {
  return data_->GetDeclaredPropertyValueString(kTransitionDelayProperty);
}

void CSSStyleDeclaration::set_transition_delay(
    const std::string& transition_delay) {
  SetPropertyValueStringByKey(kTransitionDelayProperty, transition_delay);
}

std::string CSSStyleDeclaration::transition_duration() const {
  return data_->GetDeclaredPropertyValueString(kTransitionDurationProperty);
}

void CSSStyleDeclaration::set_transition_duration(
    const std::string& transition_duration) {
  SetPropertyValueStringByKey(kTransitionDurationProperty, transition_duration);
}

std::string CSSStyleDeclaration::transition_property() const {
  return data_->GetDeclaredPropertyValueString(kTransitionPropertyProperty);
}

void CSSStyleDeclaration::set_transition_property(
    const std::string& transition_property) {
  SetPropertyValueStringByKey(kTransitionPropertyProperty, transition_property);
}

std::string CSSStyleDeclaration::transition_timing_function() const {
  return data_->GetDeclaredPropertyValueString(
      kTransitionTimingFunctionProperty);
}

void CSSStyleDeclaration::set_transition_timing_function(
    const std::string& transition_timing_function) {
  SetPropertyValueStringByKey(kTransitionTimingFunctionProperty,
                              transition_timing_function);
}

std::string CSSStyleDeclaration::vertical_align() const {
  return data_->GetDeclaredPropertyValueString(kVerticalAlignProperty);
}

void CSSStyleDeclaration::set_vertical_align(
    const std::string& vertical_align) {
  SetPropertyValueStringByKey(kVerticalAlignProperty, vertical_align);
}

std::string CSSStyleDeclaration::visibility() const {
  return data_->GetDeclaredPropertyValueString(kVisibilityProperty);
}

void CSSStyleDeclaration::set_visibility(const std::string& visibility) {
  SetPropertyValueStringByKey(kVisibilityProperty, visibility);
}

std::string CSSStyleDeclaration::white_space() const {
  return data_->GetDeclaredPropertyValueString(kWhiteSpaceProperty);
}

void CSSStyleDeclaration::set_white_space(const std::string& white_space) {
  SetPropertyValueStringByKey(kWhiteSpaceProperty, white_space);
}

std::string CSSStyleDeclaration::width() const {
  return data_->GetDeclaredPropertyValueString(kWidthProperty);
}

void CSSStyleDeclaration::set_width(const std::string& width) {
  SetPropertyValueStringByKey(kWidthProperty, width);
}

// word-wrap is treated as an alias for overflow-wrap
//   http://www.w3.org/TR/css-text-3/#overflow-wrap
std::string CSSStyleDeclaration::word_wrap() const {
  return data_->GetDeclaredPropertyValueString(kOverflowWrapProperty);
}

void CSSStyleDeclaration::set_word_wrap(const std::string& word_wrap) {
  SetPropertyValueStringByKey(kWordWrapProperty, word_wrap);
}

std::string CSSStyleDeclaration::z_index() const {
  return data_->GetDeclaredPropertyValueString(kZIndexProperty);
}

void CSSStyleDeclaration::set_z_index(const std::string& z_index) {
  SetPropertyValueStringByKey(kZIndexProperty, z_index);
}

// This returns the result of serializing a CSS declaration block.
// The current implementation does not handle shorthands.
//   http://www.w3.org/TR/cssom/#serialize-a-css-declaration-block
std::string CSSStyleDeclaration::css_text() const {
  return data_->SerializeCSSDeclarationBlock();
}

void CSSStyleDeclaration::set_css_text(const std::string& css_text) {
  DCHECK(css_parser_);
  scoped_refptr<CSSStyleDeclarationData> declaration =
      css_parser_->ParseStyleDeclarationList(
          css_text, non_trivial_static_fields.Get().location);

  if (declaration) {
    data_ = declaration;
  } else {
    data_ = new CSSStyleDeclarationData();
  }

  RecordMutation();
}

// The length attribute must return the number of CSS declarations in the
// declarations.
//   http://www.w3.org/TR/cssom/#dom-cssstyledeclaration-length
unsigned int CSSStyleDeclaration::length() const { return data_->length(); }

// The item(index) method must return the property name of the CSS declaration
// at position index.
//   http://www.w3.org/TR/cssom/#dom-cssstyledeclaration-item
base::optional<std::string> CSSStyleDeclaration::Item(
    unsigned int index) const {
  const char* item = data_->Item(index);
  return item ? base::optional<std::string>(item) : base::nullopt;
}

std::string CSSStyleDeclaration::GetPropertyValue(
    const std::string& property_name) {
  const scoped_refptr<const PropertyValue>& property_value =
      data_->GetPropertyValue(GetPropertyKey(property_name));
  return property_value ? property_value->ToString() : "";
}

void CSSStyleDeclaration::SetPropertyValue(const std::string& property_name,
                                           const std::string& property_value) {
  DCHECK(css_parser_);
  css_parser_->ParsePropertyIntoDeclarationData(
      property_name, property_value, non_trivial_static_fields.Get().location,
      data_.get());

  RecordMutation();
}

scoped_refptr<CSSRule> CSSStyleDeclaration::parent_rule() const {
  return parent_rule_.get();
}

void CSSStyleDeclaration::set_parent_rule(CSSRule* parent_rule) {
  parent_rule_ = base::AsWeakPtr(parent_rule);
}

CSSStyleDeclaration::~CSSStyleDeclaration() {}

void CSSStyleDeclaration::RecordMutation() {
  if (mutation_observer_) {
    // Trigger layout update.
    mutation_observer_->OnCSSMutation();
  }
}

void CSSStyleDeclaration::SetPropertyValueStringByKey(
    PropertyKey key, const std::string& property_value) {
  SetPropertyValue(GetPropertyName(key), property_value);
}

}  // namespace cssom
}  // namespace cobalt

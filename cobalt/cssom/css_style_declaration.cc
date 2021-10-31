// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/css_style_declaration.h"

#include "base/strings/string_util.h"
#include "cobalt/base/source_location.h"
#include "cobalt/cssom/css_declaration_util.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/property_definitions.h"

namespace cobalt {
namespace cssom {

std::string CSSStyleDeclaration::align_content(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kAlignContentProperty);
}

void CSSStyleDeclaration::set_align_content(
    const std::string& align_content, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kAlignContentProperty, align_content,
                              exception_state);
}

std::string CSSStyleDeclaration::align_items(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kAlignItemsProperty);
}

void CSSStyleDeclaration::set_align_items(
    const std::string& align_items, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kAlignItemsProperty, align_items,
                              exception_state);
}

std::string CSSStyleDeclaration::align_self(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kAlignSelfProperty);
}

void CSSStyleDeclaration::set_align_self(
    const std::string& align_self, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kAlignSelfProperty, align_self, exception_state);
}

std::string CSSStyleDeclaration::animation(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kAnimationProperty);
}

void CSSStyleDeclaration::set_animation(
    const std::string& animation, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kAnimationProperty, animation, exception_state);
}

std::string CSSStyleDeclaration::animation_delay(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kAnimationDelayProperty);
}

void CSSStyleDeclaration::set_animation_delay(
    const std::string& animation_delay,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kAnimationDelayProperty, animation_delay,
                              exception_state);
}

std::string CSSStyleDeclaration::animation_direction(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kAnimationDirectionProperty);
}

void CSSStyleDeclaration::set_animation_direction(
    const std::string& animation_direction,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kAnimationDirectionProperty, animation_direction,
                              exception_state);
}

std::string CSSStyleDeclaration::animation_duration(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kAnimationDurationProperty);
}

void CSSStyleDeclaration::set_animation_duration(
    const std::string& animation_duration,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kAnimationDurationProperty, animation_duration,
                              exception_state);
}

std::string CSSStyleDeclaration::animation_fill_mode(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kAnimationFillModeProperty);
}

void CSSStyleDeclaration::set_animation_fill_mode(
    const std::string& animation_fill_mode,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kAnimationFillModeProperty, animation_fill_mode,
                              exception_state);
}

std::string CSSStyleDeclaration::animation_iteration_count(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kAnimationIterationCountProperty);
}

void CSSStyleDeclaration::set_animation_iteration_count(
    const std::string& animation_iteration_count,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kAnimationIterationCountProperty,
                              animation_iteration_count, exception_state);
}

std::string CSSStyleDeclaration::animation_name(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kAnimationNameProperty);
}

void CSSStyleDeclaration::set_animation_name(
    const std::string& animation_name,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kAnimationNameProperty, animation_name,
                              exception_state);
}

std::string CSSStyleDeclaration::animation_timing_function(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kAnimationTimingFunctionProperty);
}

void CSSStyleDeclaration::set_animation_timing_function(
    const std::string& animation_timing_function,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kAnimationTimingFunctionProperty,
                              animation_timing_function, exception_state);
}

std::string CSSStyleDeclaration::background(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBackgroundProperty);
}

void CSSStyleDeclaration::set_background(
    const std::string& background, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBackgroundProperty, background, exception_state);
}

std::string CSSStyleDeclaration::background_color(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBackgroundColorProperty);
}

void CSSStyleDeclaration::set_background_color(
    const std::string& background_color,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBackgroundColorProperty, background_color,
                              exception_state);
}

std::string CSSStyleDeclaration::background_image(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBackgroundImageProperty);
}

void CSSStyleDeclaration::set_background_image(
    const std::string& background_image,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBackgroundImageProperty, background_image,
                              exception_state);
}

std::string CSSStyleDeclaration::background_position(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBackgroundPositionProperty);
}

void CSSStyleDeclaration::set_background_position(
    const std::string& background_position,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBackgroundPositionProperty, background_position,
                              exception_state);
}

std::string CSSStyleDeclaration::background_repeat(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBackgroundRepeatProperty);
}

void CSSStyleDeclaration::set_background_repeat(
    const std::string& background_repeat,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBackgroundRepeatProperty, background_repeat,
                              exception_state);
}

std::string CSSStyleDeclaration::background_size(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBackgroundSizeProperty);
}

void CSSStyleDeclaration::set_background_size(
    const std::string& background_size,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBackgroundSizeProperty, background_size,
                              exception_state);
}

std::string CSSStyleDeclaration::border(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderProperty);
}

void CSSStyleDeclaration::set_border(const std::string& border,
                                     script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderProperty, border, exception_state);
}

std::string CSSStyleDeclaration::border_bottom(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderBottomProperty);
}

void CSSStyleDeclaration::set_border_bottom(
    const std::string& border_bottom, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderBottomProperty, border_bottom,
                              exception_state);
}

std::string CSSStyleDeclaration::border_left(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderLeftProperty);
}

void CSSStyleDeclaration::set_border_left(
    const std::string& border_left, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderLeftProperty, border_left,
                              exception_state);
}

std::string CSSStyleDeclaration::border_right(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderRightProperty);
}

void CSSStyleDeclaration::set_border_right(
    const std::string& border_right, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderRightProperty, border_right,
                              exception_state);
}

std::string CSSStyleDeclaration::border_top(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderTopProperty);
}

void CSSStyleDeclaration::set_border_top(
    const std::string& border_top, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderTopProperty, border_top, exception_state);
}

std::string CSSStyleDeclaration::border_color(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderColorProperty);
}

void CSSStyleDeclaration::set_border_color(
    const std::string& border_color, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderColorProperty, border_color,
                              exception_state);
}

std::string CSSStyleDeclaration::border_top_color(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderTopColorProperty);
}

void CSSStyleDeclaration::set_border_top_color(
    const std::string& border_top_color,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderTopColorProperty, border_top_color,
                              exception_state);
}

std::string CSSStyleDeclaration::border_right_color(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderRightColorProperty);
}

void CSSStyleDeclaration::set_border_right_color(
    const std::string& border_right_color,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderRightColorProperty, border_right_color,
                              exception_state);
}

std::string CSSStyleDeclaration::border_bottom_color(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderBottomColorProperty);
}

void CSSStyleDeclaration::set_border_bottom_color(
    const std::string& border_bottom_color,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderBottomColorProperty, border_bottom_color,
                              exception_state);
}

std::string CSSStyleDeclaration::border_left_color(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderLeftColorProperty);
}

void CSSStyleDeclaration::set_border_left_color(
    const std::string& border_left_color,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderLeftColorProperty, border_left_color,
                              exception_state);
}

std::string CSSStyleDeclaration::border_style(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderStyleProperty);
}

void CSSStyleDeclaration::set_border_style(
    const std::string& border_style, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderStyleProperty, border_style,
                              exception_state);
}

std::string CSSStyleDeclaration::border_top_style(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderTopStyleProperty);
}

void CSSStyleDeclaration::set_border_top_style(
    const std::string& border_top_style,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderTopStyleProperty, border_top_style,
                              exception_state);
}

std::string CSSStyleDeclaration::border_right_style(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderRightStyleProperty);
}

void CSSStyleDeclaration::set_border_right_style(
    const std::string& border_right_style,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderRightStyleProperty, border_right_style,
                              exception_state);
}

std::string CSSStyleDeclaration::border_bottom_style(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderBottomStyleProperty);
}

void CSSStyleDeclaration::set_border_bottom_style(
    const std::string& border_bottom_style,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderBottomStyleProperty, border_bottom_style,
                              exception_state);
}

std::string CSSStyleDeclaration::border_left_style(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderLeftStyleProperty);
}

void CSSStyleDeclaration::set_border_left_style(
    const std::string& border_left_style,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderLeftStyleProperty, border_left_style,
                              exception_state);
}

std::string CSSStyleDeclaration::border_width(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderWidthProperty);
}

void CSSStyleDeclaration::set_border_width(
    const std::string& border_width, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderWidthProperty, border_width,
                              exception_state);
}

std::string CSSStyleDeclaration::border_top_width(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderTopWidthProperty);
}

void CSSStyleDeclaration::set_border_top_width(
    const std::string& border_top_width,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderTopWidthProperty, border_top_width,
                              exception_state);
}

std::string CSSStyleDeclaration::border_right_width(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderRightWidthProperty);
}

void CSSStyleDeclaration::set_border_right_width(
    const std::string& border_right_width,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderRightWidthProperty, border_right_width,
                              exception_state);
}

std::string CSSStyleDeclaration::border_bottom_width(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderBottomWidthProperty);
}

void CSSStyleDeclaration::set_border_bottom_width(
    const std::string& border_bottom_width,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderBottomWidthProperty, border_bottom_width,
                              exception_state);
}

std::string CSSStyleDeclaration::border_left_width(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderLeftWidthProperty);
}

void CSSStyleDeclaration::set_border_left_width(
    const std::string& border_left_width,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderLeftWidthProperty, border_left_width,
                              exception_state);
}

std::string CSSStyleDeclaration::border_radius(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderRadiusProperty);
}

void CSSStyleDeclaration::set_border_radius(
    const std::string& border_radius, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderRadiusProperty, border_radius,
                              exception_state);
}

std::string CSSStyleDeclaration::border_top_left_radius(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderTopLeftRadiusProperty);
}

void CSSStyleDeclaration::set_border_top_left_radius(
    const std::string& border_top_left_radius,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderTopLeftRadiusProperty,
                              border_top_left_radius, exception_state);
}

std::string CSSStyleDeclaration::border_top_right_radius(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderTopRightRadiusProperty);
}

void CSSStyleDeclaration::set_border_top_right_radius(
    const std::string& border_top_right_radius,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderTopRightRadiusProperty,
                              border_top_right_radius, exception_state);
}

std::string CSSStyleDeclaration::border_bottom_right_radius(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderBottomRightRadiusProperty);
}

void CSSStyleDeclaration::set_border_bottom_right_radius(
    const std::string& border_bottom_right_radius,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderBottomRightRadiusProperty,
                              border_bottom_right_radius, exception_state);
}

std::string CSSStyleDeclaration::border_bottom_left_radius(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBorderBottomLeftRadiusProperty);
}

void CSSStyleDeclaration::set_border_bottom_left_radius(
    const std::string& border_bottom_left_radius,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBorderBottomLeftRadiusProperty,
                              border_bottom_left_radius, exception_state);
}

std::string CSSStyleDeclaration::bottom(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBottomProperty);
}

void CSSStyleDeclaration::set_bottom(const std::string& bottom,
                                     script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBottomProperty, bottom, exception_state);
}

std::string CSSStyleDeclaration::box_shadow(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kBoxShadowProperty);
}

void CSSStyleDeclaration::set_box_shadow(
    const std::string& box_shadow, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kBoxShadowProperty, box_shadow, exception_state);
}

std::string CSSStyleDeclaration::color(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kColorProperty);
}

void CSSStyleDeclaration::set_color(const std::string& color,
                                    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kColorProperty, color, exception_state);
}

std::string CSSStyleDeclaration::content(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kContentProperty);
}

void CSSStyleDeclaration::set_content(const std::string& content,
                                      script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kContentProperty, content, exception_state);
}

std::string CSSStyleDeclaration::display(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kDisplayProperty);
}

void CSSStyleDeclaration::set_display(const std::string& display,
                                      script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kDisplayProperty, display, exception_state);
}

std::string CSSStyleDeclaration::filter(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kFilterProperty);
}

void CSSStyleDeclaration::set_filter(const std::string& filter,
                                     script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kFilterProperty, filter, exception_state);
}

std::string CSSStyleDeclaration::flex(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kFlexProperty);
}

void CSSStyleDeclaration::set_flex(const std::string& flex,
                                   script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kFlexProperty, flex, exception_state);
}

std::string CSSStyleDeclaration::flex_basis(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kFlexBasisProperty);
}

void CSSStyleDeclaration::set_flex_basis(
    const std::string& flex_basis, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kFlexBasisProperty, flex_basis, exception_state);
}

std::string CSSStyleDeclaration::flex_direction(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kFlexDirectionProperty);
}

void CSSStyleDeclaration::set_flex_direction(
    const std::string& flex_direction,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kFlexDirectionProperty, flex_direction,
                              exception_state);
}

std::string CSSStyleDeclaration::flex_flow(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kFlexFlowProperty);
}

void CSSStyleDeclaration::set_flex_flow(
    const std::string& flex_flow, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kFlexFlowProperty, flex_flow, exception_state);
}

std::string CSSStyleDeclaration::flex_grow(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kFlexGrowProperty);
}

void CSSStyleDeclaration::set_flex_grow(
    const std::string& flex_grow, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kFlexGrowProperty, flex_grow, exception_state);
}

std::string CSSStyleDeclaration::flex_shrink(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kFlexShrinkProperty);
}

void CSSStyleDeclaration::set_flex_shrink(
    const std::string& flex_shrink, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kFlexShrinkProperty, flex_shrink,
                              exception_state);
}

std::string CSSStyleDeclaration::flex_wrap(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kFlexWrapProperty);
}

void CSSStyleDeclaration::set_flex_wrap(
    const std::string& flex_wrap, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kFlexWrapProperty, flex_wrap, exception_state);
}

std::string CSSStyleDeclaration::font(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kFontProperty);
}

void CSSStyleDeclaration::set_font(const std::string& font,
                                   script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kFontProperty, font, exception_state);
}

std::string CSSStyleDeclaration::font_family(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kFontFamilyProperty);
}

void CSSStyleDeclaration::set_font_family(
    const std::string& font_family, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kFontFamilyProperty, font_family,
                              exception_state);
}

std::string CSSStyleDeclaration::font_size(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kFontSizeProperty);
}

void CSSStyleDeclaration::set_font_size(
    const std::string& font_size, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kFontSizeProperty, font_size, exception_state);
}

std::string CSSStyleDeclaration::font_style(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kFontStyleProperty);
}

void CSSStyleDeclaration::set_font_style(
    const std::string& font_style, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kFontStyleProperty, font_style, exception_state);
}

std::string CSSStyleDeclaration::font_weight(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kFontWeightProperty);
}

void CSSStyleDeclaration::set_font_weight(
    const std::string& font_weight, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kFontWeightProperty, font_weight,
                              exception_state);
}

std::string CSSStyleDeclaration::height(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kHeightProperty);
}

void CSSStyleDeclaration::set_height(const std::string& height,
                                     script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kHeightProperty, height, exception_state);
}

std::string CSSStyleDeclaration::justify_content(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kJustifyContentProperty);
}

void CSSStyleDeclaration::set_justify_content(
    const std::string& justify_content,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kJustifyContentProperty, justify_content,
                              exception_state);
}

std::string CSSStyleDeclaration::left(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kLeftProperty);
}

void CSSStyleDeclaration::set_left(const std::string& left,
                                   script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kLeftProperty, left, exception_state);
}

std::string CSSStyleDeclaration::line_height(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kLineHeightProperty);
}

void CSSStyleDeclaration::set_line_height(
    const std::string& line_height, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kLineHeightProperty, line_height,
                              exception_state);
}

std::string CSSStyleDeclaration::margin(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kMarginProperty);
}

void CSSStyleDeclaration::set_margin(const std::string& margin,
                                     script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kMarginProperty, margin, exception_state);
}

std::string CSSStyleDeclaration::margin_bottom(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kMarginBottomProperty);
}

void CSSStyleDeclaration::set_margin_bottom(
    const std::string& margin_bottom, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kMarginBottomProperty, margin_bottom,
                              exception_state);
}

std::string CSSStyleDeclaration::margin_left(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kMarginLeftProperty);
}

void CSSStyleDeclaration::set_margin_left(
    const std::string& margin_left, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kMarginLeftProperty, margin_left,
                              exception_state);
}

std::string CSSStyleDeclaration::margin_right(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kMarginRightProperty);
}

void CSSStyleDeclaration::set_margin_right(
    const std::string& margin_right, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kMarginRightProperty, margin_right,
                              exception_state);
}

std::string CSSStyleDeclaration::margin_top(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kMarginTopProperty);
}

void CSSStyleDeclaration::set_margin_top(
    const std::string& margin_top, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kMarginTopProperty, margin_top, exception_state);
}

std::string CSSStyleDeclaration::max_height(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kMaxHeightProperty);
}

void CSSStyleDeclaration::set_max_height(
    const std::string& max_height, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kMaxHeightProperty, max_height, exception_state);
}

std::string CSSStyleDeclaration::max_width(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kMaxWidthProperty);
}

void CSSStyleDeclaration::set_max_width(
    const std::string& max_width, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kMaxWidthProperty, max_width, exception_state);
}

std::string CSSStyleDeclaration::min_height(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kMinHeightProperty);
}

void CSSStyleDeclaration::set_min_height(
    const std::string& min_height, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kMinHeightProperty, min_height, exception_state);
}

std::string CSSStyleDeclaration::min_width(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kMinWidthProperty);
}

void CSSStyleDeclaration::set_min_width(
    const std::string& min_width, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kMinWidthProperty, min_width, exception_state);
}

std::string CSSStyleDeclaration::opacity(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kOpacityProperty);
}

void CSSStyleDeclaration::set_opacity(const std::string& opacity,
                                      script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kOpacityProperty, opacity, exception_state);
}

std::string CSSStyleDeclaration::order(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kOrderProperty);
}

void CSSStyleDeclaration::set_order(const std::string& order,
                                    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kOrderProperty, order, exception_state);
}

std::string CSSStyleDeclaration::outline(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kOutlineProperty);
}

void CSSStyleDeclaration::set_outline(const std::string& outline,
                                      script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kOutlineProperty, outline, exception_state);
}

std::string CSSStyleDeclaration::outline_color(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kOutlineColorProperty);
}

void CSSStyleDeclaration::set_outline_color(
    const std::string& outline_color, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kOutlineColorProperty, outline_color,
                              exception_state);
}

std::string CSSStyleDeclaration::outline_style(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kOutlineStyleProperty);
}

void CSSStyleDeclaration::set_outline_style(
    const std::string& outline_style, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kOutlineStyleProperty, outline_style,
                              exception_state);
}

std::string CSSStyleDeclaration::outline_width(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kOutlineWidthProperty);
}

void CSSStyleDeclaration::set_outline_width(
    const std::string& outline_width, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kOutlineWidthProperty, outline_width,
                              exception_state);
}

std::string CSSStyleDeclaration::overflow(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kOverflowProperty);
}

void CSSStyleDeclaration::set_overflow(
    const std::string& overflow, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kOverflowProperty, overflow, exception_state);
}

std::string CSSStyleDeclaration::overflow_wrap(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kOverflowWrapProperty);
}

void CSSStyleDeclaration::set_overflow_wrap(
    const std::string& overflow_wrap, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kOverflowWrapProperty, overflow_wrap,
                              exception_state);
}

std::string CSSStyleDeclaration::padding(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kPaddingProperty);
}

void CSSStyleDeclaration::set_padding(const std::string& padding,
                                      script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kPaddingProperty, padding, exception_state);
}

std::string CSSStyleDeclaration::padding_bottom(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kPaddingBottomProperty);
}

void CSSStyleDeclaration::set_padding_bottom(
    const std::string& padding_bottom,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kPaddingBottomProperty, padding_bottom,
                              exception_state);
}

std::string CSSStyleDeclaration::padding_left(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kPaddingLeftProperty);
}

void CSSStyleDeclaration::set_padding_left(
    const std::string& padding_left, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kPaddingLeftProperty, padding_left,
                              exception_state);
}

std::string CSSStyleDeclaration::padding_right(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kPaddingRightProperty);
}

void CSSStyleDeclaration::set_padding_right(
    const std::string& padding_right, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kPaddingRightProperty, padding_right,
                              exception_state);
}

std::string CSSStyleDeclaration::padding_top(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kPaddingTopProperty);
}

void CSSStyleDeclaration::set_padding_top(
    const std::string& padding_top, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kPaddingTopProperty, padding_top,
                              exception_state);
}

std::string CSSStyleDeclaration::pointer_events(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kPointerEventsProperty);
}

void CSSStyleDeclaration::set_pointer_events(
    const std::string& pointer_events,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kPointerEventsProperty, pointer_events,
                              exception_state);
}

std::string CSSStyleDeclaration::position(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kPositionProperty);
}

void CSSStyleDeclaration::set_position(
    const std::string& position, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kPositionProperty, position, exception_state);
}

std::string CSSStyleDeclaration::right(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kRightProperty);
}

void CSSStyleDeclaration::set_right(const std::string& right,
                                    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kRightProperty, right, exception_state);
}

std::string CSSStyleDeclaration::text_align(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTextAlignProperty);
}

void CSSStyleDeclaration::set_text_align(
    const std::string& text_align, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTextAlignProperty, text_align, exception_state);
}

std::string CSSStyleDeclaration::text_decoration(
    script::ExceptionState* exception_state) const {
  // TODO: Redirect text decoration to text decoration line for now and
  // change it when fully implement text decoration.
  return GetDeclaredPropertyValueStringByKey(kTextDecorationLineProperty);
}

void CSSStyleDeclaration::set_text_decoration(
    const std::string& text_decoration,
    script::ExceptionState* exception_state) {
  // TODO: Redirect text decoration to text decoration line for now and
  // change it when fully implement text decoration.
  SetPropertyValueStringByKey(kTextDecorationLineProperty, text_decoration,
                              exception_state);
}

std::string CSSStyleDeclaration::text_decoration_color(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTextDecorationColorProperty);
}

void CSSStyleDeclaration::set_text_decoration_color(
    const std::string& text_decoration_color,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTextDecorationColorProperty,
                              text_decoration_color, exception_state);
}

std::string CSSStyleDeclaration::text_decoration_line(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTextDecorationLineProperty);
}

void CSSStyleDeclaration::set_text_decoration_line(
    const std::string& text_decoration_line,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTextDecorationLineProperty, text_decoration_line,
                              exception_state);
}

std::string CSSStyleDeclaration::text_indent(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTextIndentProperty);
}

void CSSStyleDeclaration::set_text_indent(
    const std::string& text_indent, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTextIndentProperty, text_indent,
                              exception_state);
}

std::string CSSStyleDeclaration::text_overflow(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTextOverflowProperty);
}

void CSSStyleDeclaration::set_text_overflow(
    const std::string& text_overflow, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTextOverflowProperty, text_overflow,
                              exception_state);
}

std::string CSSStyleDeclaration::text_shadow(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTextShadowProperty);
}

void CSSStyleDeclaration::set_text_shadow(
    const std::string& text_shadow, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTextShadowProperty, text_shadow,
                              exception_state);
}

std::string CSSStyleDeclaration::text_transform(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTextTransformProperty);
}

void CSSStyleDeclaration::set_text_transform(
    const std::string& text_transform,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTextTransformProperty, text_transform,
                              exception_state);
}

std::string CSSStyleDeclaration::top(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTopProperty);
}

void CSSStyleDeclaration::set_top(const std::string& top,
                                  script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTopProperty, top, exception_state);
}

std::string CSSStyleDeclaration::transform(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTransformProperty);
}

void CSSStyleDeclaration::set_transform(
    const std::string& transform, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTransformProperty, transform, exception_state);
}

std::string CSSStyleDeclaration::transform_origin(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTransformOriginProperty);
}

void CSSStyleDeclaration::set_transform_origin(
    const std::string& transform_origin,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTransformOriginProperty, transform_origin,
                              exception_state);
}

std::string CSSStyleDeclaration::transition(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTransitionProperty);
}

void CSSStyleDeclaration::set_transition(
    const std::string& transition, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTransitionProperty, transition, exception_state);
}

std::string CSSStyleDeclaration::transition_delay(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTransitionDelayProperty);
}

void CSSStyleDeclaration::set_transition_delay(
    const std::string& transition_delay,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTransitionDelayProperty, transition_delay,
                              exception_state);
}

std::string CSSStyleDeclaration::transition_duration(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTransitionDurationProperty);
}

void CSSStyleDeclaration::set_transition_duration(
    const std::string& transition_duration,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTransitionDurationProperty, transition_duration,
                              exception_state);
}

std::string CSSStyleDeclaration::transition_property(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTransitionPropertyProperty);
}

void CSSStyleDeclaration::set_transition_property(
    const std::string& transition_property,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTransitionPropertyProperty, transition_property,
                              exception_state);
}

std::string CSSStyleDeclaration::transition_timing_function(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kTransitionTimingFunctionProperty);
}

void CSSStyleDeclaration::set_transition_timing_function(
    const std::string& transition_timing_function,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kTransitionTimingFunctionProperty,
                              transition_timing_function, exception_state);
}

std::string CSSStyleDeclaration::vertical_align(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kVerticalAlignProperty);
}

void CSSStyleDeclaration::set_vertical_align(
    const std::string& vertical_align,
    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kVerticalAlignProperty, vertical_align,
                              exception_state);
}

std::string CSSStyleDeclaration::visibility(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kVisibilityProperty);
}

void CSSStyleDeclaration::set_visibility(
    const std::string& visibility, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kVisibilityProperty, visibility, exception_state);
}

std::string CSSStyleDeclaration::white_space(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kWhiteSpaceProperty);
}

void CSSStyleDeclaration::set_white_space(
    const std::string& white_space, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kWhiteSpaceProperty, white_space,
                              exception_state);
}

std::string CSSStyleDeclaration::width(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kWidthProperty);
}

void CSSStyleDeclaration::set_width(const std::string& width,
                                    script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kWidthProperty, width, exception_state);
}

// word-wrap is treated as an alias for overflow-wrap
//   https://www.w3.org/TR/css-text-3/#overflow-wrap
std::string CSSStyleDeclaration::word_wrap(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kOverflowWrapProperty);
}

void CSSStyleDeclaration::set_word_wrap(
    const std::string& word_wrap, script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kWordWrapProperty, word_wrap, exception_state);
}

std::string CSSStyleDeclaration::z_index(
    script::ExceptionState* exception_state) const {
  return GetDeclaredPropertyValueStringByKey(kZIndexProperty);
}

void CSSStyleDeclaration::set_z_index(const std::string& z_index,
                                      script::ExceptionState* exception_state) {
  SetPropertyValueStringByKey(kZIndexProperty, z_index, exception_state);
}

scoped_refptr<CSSRule> CSSStyleDeclaration::parent_rule() const { return NULL; }

std::string CSSStyleDeclaration::GetPropertyValue(
    const std::string& property_name) {
  return GetDeclaredPropertyValueStringByKey(GetPropertyKey(property_name));
}

void CSSStyleDeclaration::SetPropertyValueStringByKey(
    PropertyKey key, const std::string& property_value,
    script::ExceptionState* exception_state) {
  SetPropertyValue(GetPropertyName(key), property_value, exception_state);
}

}  // namespace cssom
}  // namespace cobalt

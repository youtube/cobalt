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

#ifndef COBALT_CSSOM_CSS_STYLE_DECLARATION_H_
#define COBALT_CSSOM_CSS_STYLE_DECLARATION_H_

#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSRule;
class MutationObserver;

// The CSSStyleDeclaration interface represents a CSS declaration block,
// including its underlying state, where this underlying state depends
// upon the source of the CSSStyleDeclaration instance.
//   https://www.w3.org/TR/2013/WD-cssom-20131205/#the-cssstyledeclaration-interface
class CSSStyleDeclaration : public script::Wrappable {
 public:
  // Web API: CSSStyleDeclaration
  //
  std::string align_content(script::ExceptionState* exception_state) const;
  void set_align_content(const std::string& align_content,
                         script::ExceptionState* exception_state);

  std::string align_items(script::ExceptionState* exception_state) const;
  void set_align_items(const std::string& align_items,
                       script::ExceptionState* exception_state);

  std::string align_self(script::ExceptionState* exception_state) const;
  void set_align_self(const std::string& align_self,
                      script::ExceptionState* exception_state);

  std::string animation(script::ExceptionState* exception_state) const;
  void set_animation(const std::string& animation,
                     script::ExceptionState* exception_state);

  std::string animation_delay(script::ExceptionState* exception_state) const;
  void set_animation_delay(const std::string& animation_delay,
                           script::ExceptionState* exception_state);

  std::string animation_direction(
      script::ExceptionState* exception_state) const;
  void set_animation_direction(const std::string& animation_direction,
                               script::ExceptionState* exception_state);

  std::string animation_duration(script::ExceptionState* exception_state) const;
  void set_animation_duration(const std::string& animation_duration,
                              script::ExceptionState* exception_state);

  std::string animation_fill_mode(
      script::ExceptionState* exception_state) const;
  void set_animation_fill_mode(const std::string& animation_fill_mode,
                               script::ExceptionState* exception_state);

  std::string animation_iteration_count(
      script::ExceptionState* exception_state) const;
  void set_animation_iteration_count(
      const std::string& animation_iteration_count,
      script::ExceptionState* exception_state);

  std::string animation_name(script::ExceptionState* exception_state) const;
  void set_animation_name(const std::string& animation_name,
                          script::ExceptionState* exception_state);

  std::string animation_timing_function(
      script::ExceptionState* exception_state) const;
  void set_animation_timing_function(
      const std::string& animation_timing_function,
      script::ExceptionState* exception_state);

  std::string background(script::ExceptionState* exception_state) const;
  void set_background(const std::string& background,
                      script::ExceptionState* exception_state);

  std::string background_color(script::ExceptionState* exception_state) const;
  void set_background_color(const std::string& background_color,
                            script::ExceptionState* exception_state);

  std::string background_image(script::ExceptionState* exception_state) const;
  void set_background_image(const std::string& background_image,
                            script::ExceptionState* exception_state);

  std::string background_position(
      script::ExceptionState* exception_state) const;
  void set_background_position(const std::string& background_position,
                               script::ExceptionState* exception_state);

  std::string background_repeat(script::ExceptionState* exception_state) const;
  void set_background_repeat(const std::string& background_repeat,
                             script::ExceptionState* exception_state);

  std::string background_size(script::ExceptionState* exception_state) const;
  void set_background_size(const std::string& background_size,
                           script::ExceptionState* exception_state);

  std::string border(script::ExceptionState* exception_state) const;
  void set_border(const std::string& border,
                  script::ExceptionState* exception_state);

  std::string border_bottom(script::ExceptionState* exception_state) const;
  void set_border_bottom(const std::string& border_bottom,
                         script::ExceptionState* exception_state);

  std::string border_bottom_color(
      script::ExceptionState* exception_state) const;
  void set_border_bottom_color(const std::string& border_bottom_color,
                               script::ExceptionState* exception_state);

  std::string border_bottom_left_radius(
      script::ExceptionState* exception_state) const;
  void set_border_bottom_left_radius(
      const std::string& border_bottom_left_radius,
      script::ExceptionState* exception_state);

  std::string border_bottom_right_radius(
      script::ExceptionState* exception_state) const;
  void set_border_bottom_right_radius(
      const std::string& border_bottom_right_radius,
      script::ExceptionState* exception_state);

  std::string border_bottom_style(
      script::ExceptionState* exception_state) const;
  void set_border_bottom_style(const std::string& border_bottom_style,
                               script::ExceptionState* exception_state);

  std::string border_bottom_width(
      script::ExceptionState* exception_state) const;
  void set_border_bottom_width(const std::string& border_bottom_width,
                               script::ExceptionState* exception_state);

  std::string border_color(script::ExceptionState* exception_state) const;
  void set_border_color(const std::string& border_color,
                        script::ExceptionState* exception_state);

  std::string border_left(script::ExceptionState* exception_state) const;
  void set_border_left(const std::string& border_left,
                       script::ExceptionState* exception_state);

  std::string border_left_color(script::ExceptionState* exception_state) const;
  void set_border_left_color(const std::string& border_left_color,
                             script::ExceptionState* exception_state);

  std::string border_left_style(script::ExceptionState* exception_state) const;
  void set_border_left_style(const std::string& border_left_style,
                             script::ExceptionState* exception_state);

  std::string border_left_width(script::ExceptionState* exception_state) const;
  void set_border_left_width(const std::string& border_left_width,
                             script::ExceptionState* exception_state);

  std::string border_radius(script::ExceptionState* exception_state) const;
  void set_border_radius(const std::string& border_radius,
                         script::ExceptionState* exception_state);

  std::string border_right(script::ExceptionState* exception_state) const;
  void set_border_right(const std::string& border_right,
                        script::ExceptionState* exception_state);

  std::string border_right_color(script::ExceptionState* exception_state) const;
  void set_border_right_color(const std::string& border_right_color,
                              script::ExceptionState* exception_state);

  std::string border_right_style(script::ExceptionState* exception_state) const;
  void set_border_right_style(const std::string& border_right_style,
                              script::ExceptionState* exception_state);

  std::string border_right_width(script::ExceptionState* exception_state) const;
  void set_border_right_width(const std::string& border_right_width,
                              script::ExceptionState* exception_state);

  std::string border_top(script::ExceptionState* exception_state) const;
  void set_border_top(const std::string& border_top,
                      script::ExceptionState* exception_state);

  std::string border_top_color(script::ExceptionState* exception_state) const;
  void set_border_top_color(const std::string& border_top_color,
                            script::ExceptionState* exception_state);

  std::string border_top_left_radius(
      script::ExceptionState* exception_state) const;
  void set_border_top_left_radius(const std::string& border_top_left_radius,
                                  script::ExceptionState* exception_state);

  std::string border_top_right_radius(
      script::ExceptionState* exception_state) const;
  void set_border_top_right_radius(const std::string& border_top_right_radius,
                                   script::ExceptionState* exception_state);

  std::string border_top_style(script::ExceptionState* exception_state) const;
  void set_border_top_style(const std::string& border_top_style,
                            script::ExceptionState* exception_state);

  std::string border_top_width(script::ExceptionState* exception_state) const;
  void set_border_top_width(const std::string& border_top_width,
                            script::ExceptionState* exception_state);

  std::string border_style(script::ExceptionState* exception_state) const;
  void set_border_style(const std::string& border_style,
                        script::ExceptionState* exception_state);

  std::string border_width(script::ExceptionState* exception_state) const;
  void set_border_width(const std::string& border_width,
                        script::ExceptionState* exception_state);

  std::string bottom(script::ExceptionState* exception_state) const;
  void set_bottom(const std::string& bottom,
                  script::ExceptionState* exception_state);

  std::string box_shadow(script::ExceptionState* exception_state) const;
  void set_box_shadow(const std::string& box_shadow,
                      script::ExceptionState* exception_state);

  std::string color(script::ExceptionState* exception_state) const;
  void set_color(const std::string& color,
                 script::ExceptionState* exception_state);

  std::string content(script::ExceptionState* exception_state) const;
  void set_content(const std::string& content,
                   script::ExceptionState* exception_state);

  std::string display(script::ExceptionState* exception_state) const;
  void set_display(const std::string& display,
                   script::ExceptionState* exception_state);

  std::string filter(script::ExceptionState* exception_state) const;
  void set_filter(const std::string& filter,
                  script::ExceptionState* exception_state);

  std::string flex(script::ExceptionState* exception_state) const;
  void set_flex(const std::string& flex,
                script::ExceptionState* exception_state);

  std::string flex_basis(script::ExceptionState* exception_state) const;
  void set_flex_basis(const std::string& flex_basis,
                      script::ExceptionState* exception_state);

  std::string flex_direction(script::ExceptionState* exception_state) const;
  void set_flex_direction(const std::string& flex_direction,
                          script::ExceptionState* exception_state);

  std::string flex_flow(script::ExceptionState* exception_state) const;
  void set_flex_flow(const std::string& flex_flow,
                     script::ExceptionState* exception_state);

  std::string flex_grow(script::ExceptionState* exception_state) const;
  void set_flex_grow(const std::string& flex_grow,
                     script::ExceptionState* exception_state);

  std::string flex_shrink(script::ExceptionState* exception_state) const;
  void set_flex_shrink(const std::string& flex_shrink,
                       script::ExceptionState* exception_state);

  std::string flex_wrap(script::ExceptionState* exception_state) const;
  void set_flex_wrap(const std::string& flex_wrap,
                     script::ExceptionState* exception_state);

  std::string font(script::ExceptionState* exception_state) const;
  void set_font(const std::string& font,
                script::ExceptionState* exception_state);

  std::string font_family(script::ExceptionState* exception_state) const;
  void set_font_family(const std::string& font_family,
                       script::ExceptionState* exception_state);

  std::string font_size(script::ExceptionState* exception_state) const;
  void set_font_size(const std::string& font_size,
                     script::ExceptionState* exception_state);

  std::string font_style(script::ExceptionState* exception_state) const;
  void set_font_style(const std::string& font_style,
                      script::ExceptionState* exception_state);

  std::string font_weight(script::ExceptionState* exception_state) const;
  void set_font_weight(const std::string& font_weight,
                       script::ExceptionState* exception_state);

  std::string height(script::ExceptionState* exception_state) const;
  void set_height(const std::string& height,
                  script::ExceptionState* exception_state);

  std::string justify_content(script::ExceptionState* exception_state) const;
  void set_justify_content(const std::string& justify_content,
                           script::ExceptionState* exception_state);

  std::string left(script::ExceptionState* exception_state) const;
  void set_left(const std::string& left,
                script::ExceptionState* exception_state);

  std::string line_height(script::ExceptionState* exception_state) const;
  void set_line_height(const std::string& line_height,
                       script::ExceptionState* exception_state);

  std::string margin(script::ExceptionState* exception_state) const;
  void set_margin(const std::string& margin,
                  script::ExceptionState* exception_state);

  std::string margin_bottom(script::ExceptionState* exception_state) const;
  void set_margin_bottom(const std::string& margin_bottom,
                         script::ExceptionState* exception_state);

  std::string margin_left(script::ExceptionState* exception_state) const;
  void set_margin_left(const std::string& margin_left,
                       script::ExceptionState* exception_state);

  std::string margin_right(script::ExceptionState* exception_state) const;
  void set_margin_right(const std::string& margin_right,
                        script::ExceptionState* exception_state);

  std::string margin_top(script::ExceptionState* exception_state) const;
  void set_margin_top(const std::string& margin_top,
                      script::ExceptionState* exception_state);

  std::string max_height(script::ExceptionState* exception_state) const;
  void set_max_height(const std::string& max_height,
                      script::ExceptionState* exception_state);

  std::string max_width(script::ExceptionState* exception_state) const;
  void set_max_width(const std::string& max_width,
                     script::ExceptionState* exception_state);

  std::string min_height(script::ExceptionState* exception_state) const;
  void set_min_height(const std::string& min_height,
                      script::ExceptionState* exception_state);

  std::string min_width(script::ExceptionState* exception_state) const;
  void set_min_width(const std::string& min_width,
                     script::ExceptionState* exception_state);

  std::string opacity(script::ExceptionState* exception_state) const;
  void set_opacity(const std::string& opacity,
                   script::ExceptionState* exception_state);

  std::string order(script::ExceptionState* exception_state) const;
  void set_order(const std::string& order,
                 script::ExceptionState* exception_state);

  std::string outline(script::ExceptionState* exception_state) const;
  void set_outline(const std::string& outline,
                   script::ExceptionState* exception_state);

  std::string outline_color(script::ExceptionState* exception_state) const;
  void set_outline_color(const std::string& outline_color,
                         script::ExceptionState* exception_state);

  std::string outline_style(script::ExceptionState* exception_state) const;
  void set_outline_style(const std::string& outline_style,
                         script::ExceptionState* exception_state);

  std::string outline_width(script::ExceptionState* exception_state) const;
  void set_outline_width(const std::string& outline_width,
                         script::ExceptionState* exception_state);

  std::string overflow(script::ExceptionState* exception_state) const;
  void set_overflow(const std::string& overflow,
                    script::ExceptionState* exception_state);

  std::string overflow_wrap(script::ExceptionState* exception_state) const;
  void set_overflow_wrap(const std::string& overflow_wrap,
                         script::ExceptionState* exception_state);

  std::string padding(script::ExceptionState* exception_state) const;
  void set_padding(const std::string& padding,
                   script::ExceptionState* exception_state);

  std::string padding_bottom(script::ExceptionState* exception_state) const;
  void set_padding_bottom(const std::string& padding_bottom,
                          script::ExceptionState* exception_state);

  std::string padding_left(script::ExceptionState* exception_state) const;
  void set_padding_left(const std::string& padding_left,
                        script::ExceptionState* exception_state);

  std::string padding_right(script::ExceptionState* exception_state) const;
  void set_padding_right(const std::string& padding_right,
                         script::ExceptionState* exception_state);

  std::string padding_top(script::ExceptionState* exception_state) const;
  void set_padding_top(const std::string& padding_top,
                       script::ExceptionState* exception_state);

  std::string pointer_events(script::ExceptionState* exception_state) const;
  void set_pointer_events(const std::string& pointer_events,
                          script::ExceptionState* exception_state);

  std::string position(script::ExceptionState* exception_state) const;
  void set_position(const std::string& position,
                    script::ExceptionState* exception_state);

  std::string right(script::ExceptionState* exception_state) const;
  void set_right(const std::string& right,
                 script::ExceptionState* exception_state);

  std::string text_align(script::ExceptionState* exception_state) const;
  void set_text_align(const std::string& text_align,
                      script::ExceptionState* exception_state);

  std::string text_decoration(script::ExceptionState* exception_state) const;
  void set_text_decoration(const std::string& text_decoration,
                           script::ExceptionState* exception_state);

  std::string text_decoration_color(
      script::ExceptionState* exception_state) const;
  void set_text_decoration_color(const std::string& text_decoration_color,
                                 script::ExceptionState* exception_state);

  std::string text_decoration_line(
      script::ExceptionState* exception_state) const;
  void set_text_decoration_line(const std::string& text_decoration_line,
                                script::ExceptionState* exception_state);

  std::string text_indent(script::ExceptionState* exception_state) const;
  void set_text_indent(const std::string& text_indent,
                       script::ExceptionState* exception_state);

  std::string text_overflow(script::ExceptionState* exception_state) const;
  void set_text_overflow(const std::string& text_overflow,
                         script::ExceptionState* exception_state);

  std::string text_shadow(script::ExceptionState* exception_state) const;
  void set_text_shadow(const std::string& text_shadow,
                       script::ExceptionState* exception_state);

  std::string text_transform(script::ExceptionState* exception_state) const;
  void set_text_transform(const std::string& text_transform,
                          script::ExceptionState* exception_state);

  std::string top(script::ExceptionState* exception_state) const;
  void set_top(const std::string& top, script::ExceptionState* exception_state);

  std::string transform(script::ExceptionState* exception_state) const;
  void set_transform(const std::string& transform,
                     script::ExceptionState* exception_state);

  std::string transform_origin(script::ExceptionState* exception_state) const;
  void set_transform_origin(const std::string& transform_origin,
                            script::ExceptionState* exception_state);

  std::string transition(script::ExceptionState* exception_state) const;
  void set_transition(const std::string& transition,
                      script::ExceptionState* exception_state);

  std::string transition_delay(script::ExceptionState* exception_state) const;
  void set_transition_delay(const std::string& transition_delay,
                            script::ExceptionState* exception_state);

  std::string transition_duration(
      script::ExceptionState* exception_state) const;
  void set_transition_duration(const std::string& transition_duration,
                               script::ExceptionState* exception_state);

  std::string transition_property(
      script::ExceptionState* exception_state) const;
  void set_transition_property(const std::string& transition_property,
                               script::ExceptionState* exception_state);

  std::string transition_timing_function(
      script::ExceptionState* exception_state) const;
  void set_transition_timing_function(
      const std::string& transition_timing_function,
      script::ExceptionState* exception_state);

  std::string vertical_align(script::ExceptionState* exception_state) const;
  void set_vertical_align(const std::string& vertical_align,
                          script::ExceptionState* exception_state);

  std::string visibility(script::ExceptionState* exception_state) const;
  void set_visibility(const std::string& visibility,
                      script::ExceptionState* exception_state);

  std::string white_space(script::ExceptionState* exception_state) const;
  void set_white_space(const std::string& width,
                       script::ExceptionState* exception_state);

  std::string width(script::ExceptionState* exception_state) const;
  void set_width(const std::string& width,
                 script::ExceptionState* exception_state);

  std::string word_wrap(script::ExceptionState* exception_state) const;
  void set_word_wrap(const std::string& word_wrap,
                     script::ExceptionState* exception_state);

  std::string z_index(script::ExceptionState* exception_state) const;
  void set_z_index(const std::string& z_index,
                   script::ExceptionState* exception_state);

  virtual std::string css_text(
      script::ExceptionState* exception_state) const = 0;
  virtual void set_css_text(const std::string& css_text,
                            script::ExceptionState* exception_state) = 0;

  virtual unsigned int length() const = 0;

  virtual base::Optional<std::string> Item(unsigned int index) const = 0;

  std::string GetPropertyValue(const std::string& property_name);
  virtual void SetPropertyValue(const std::string& property_name,
                                const std::string& property_value,
                                script::ExceptionState* exception_state) = 0;

  virtual void SetProperty(const std::string& property_name,
                           const std::string& property_value,
                           const std::string& priority,
                           script::ExceptionState* exception_state) = 0;

  void SetProperty(const std::string& property_name,
                   const std::string& property_value,
                   script::ExceptionState* exception_state) {
    SetPropertyValue(property_name, property_value, exception_state);
  }

  std::string RemoveProperty(const std::string& property_name,
                             script::ExceptionState* exception_state) {
    std::string retval = GetPropertyValue(property_name);
    if (!retval.empty()) {
      SetPropertyValue(property_name, "", exception_state);
    }
    return retval;
  }

  // The parent rule is the CSS rule that the CSS declaration block is
  // associated with, if any, or null otherwise.
  //   https://www.w3.org/TR/2013/WD-cssom-20131205/#css-declaration-block
  virtual scoped_refptr<CSSRule> parent_rule() const;

  DEFINE_WRAPPABLE_TYPE(CSSStyleDeclaration);

 protected:
  CSSStyleDeclaration() {}
  virtual ~CSSStyleDeclaration() {}

 private:
  virtual std::string GetDeclaredPropertyValueStringByKey(
      const PropertyKey key) const = 0;
  void SetPropertyValueStringByKey(PropertyKey key,
                                   const std::string& property_value,
                                   script::ExceptionState* exception_state);

  DISALLOW_COPY_AND_ASSIGN(CSSStyleDeclaration);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_STYLE_DECLARATION_H_

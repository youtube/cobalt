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

#ifndef CSSOM_CSS_STYLE_DECLARATION_H_
#define CSSOM_CSS_STYLE_DECLARATION_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSParser;
class CSSRule;
class MutationObserver;

// The CSSStyleDeclaration interface represents a CSS declaration block,
// including its underlying state, where this underlying state depends
// upon the source of the CSSStyleDeclaration instance.
//   http://dev.w3.org/csswg/cssom/#the-cssstyledeclaration-interface
class CSSStyleDeclaration : public script::Wrappable {
 public:
  // String type properties are used in JavaScript according to spec,
  // but PropertyValue type is used inside Cobalt for easy manipulation.
  // Introducing |css_parser| helps for parsing JavaScript strings to
  // PropertyValue and CSSStyleDeclaration.
  // |css_parser| can be null if only dealing with |data_|.
  explicit CSSStyleDeclaration(CSSParser* css_parser);
  CSSStyleDeclaration(const scoped_refptr<CSSStyleDeclarationData>& style,
                      CSSParser* css_parser);

  // Intentionally allowing compiler to generate copy constructor
  // and assignment operator.

  // Web API: CSSStyleDeclaration
  //

  std::string background() const;
  void set_background(const std::string& background);

  std::string background_color() const;
  void set_background_color(const std::string& background_color);

  std::string background_image() const;
  void set_background_image(const std::string& background_image);

  std::string background_position() const;
  void set_background_position(const std::string& background_position);

  std::string background_repeat() const;
  void set_background_repeat(const std::string& background_repeat);

  std::string background_size() const;
  void set_background_size(const std::string& background_size);

  std::string border_radius() const;
  void set_border_radius(const std::string& border_radius);

  std::string bottom() const;
  void set_bottom(const std::string& bottom);

  std::string color() const;
  void set_color(const std::string& color);

  std::string content() const;
  void set_content(const std::string& content);

  std::string display() const;
  void set_display(const std::string& display);

  std::string font_family() const;
  void set_font_family(const std::string& font_family);

  std::string font_size() const;
  void set_font_size(const std::string& font_size);

  std::string font_style() const;
  void set_font_style(const std::string& font_style);

  std::string font_weight() const;
  void set_font_weight(const std::string& font_weight);

  std::string height() const;
  void set_height(const std::string& height);

  std::string left() const;
  void set_left(const std::string& left);

  std::string line_height() const;
  void set_line_height(const std::string& line_height);

  std::string max_height() const;
  void set_max_height(const std::string& max_height);

  std::string max_width() const;
  void set_max_width(const std::string& max_width);

  std::string min_height() const;
  void set_min_height(const std::string& min_height);

  std::string min_width() const;
  void set_min_width(const std::string& min_width);

  std::string opacity() const;
  void set_opacity(const std::string& opacity);

  std::string overflow() const;
  void set_overflow(const std::string& overflow);

  std::string overflow_wrap() const;
  void set_overflow_wrap(const std::string& overflow_wrap);

  std::string position() const;
  void set_position(const std::string& position);

  std::string right() const;
  void set_right(const std::string& right);

  std::string tab_size() const;
  void set_tab_size(const std::string& tab_size);

  std::string text_align() const;
  void set_text_align(const std::string& text_align);

  std::string text_indent() const;
  void set_text_indent(const std::string& text_indent);

  std::string text_overflow() const;
  void set_text_overflow(const std::string& text_overflow);

  std::string text_transform() const;
  void set_text_transform(const std::string& text_transform);

  std::string top() const;
  void set_top(const std::string& top);

  std::string transform() const;
  void set_transform(const std::string& transform);

  std::string transition() const;
  void set_transition(const std::string& transition);

  std::string transition_delay() const;
  void set_transition_delay(const std::string& transition_delay);

  std::string transition_duration() const;
  void set_transition_duration(const std::string& transition_duration);

  std::string transition_property() const;
  void set_transition_property(const std::string& transition_property);

  std::string transition_timing_function() const;
  void set_transition_timing_function(
      const std::string& transition_timing_function);

  std::string vertical_align() const;
  void set_vertical_align(const std::string& vertical_align);

  std::string visibility() const;
  void set_visibility(const std::string& visibility);

  std::string white_space() const;
  void set_white_space(const std::string& width);

  std::string width() const;
  void set_width(const std::string& width);

  std::string word_wrap() const;
  void set_word_wrap(const std::string& word_wrap);

  std::string z_index() const;
  void set_z_index(const std::string& z_index);

  std::string css_text() const;
  void set_css_text(const std::string& css_text);

  unsigned int length() const;

  base::optional<std::string> Item(unsigned int index) const;

  std::string GetPropertyValue(const std::string& property_name);
  void SetPropertyValue(const std::string& property_name,
                        const std::string& property_value);

  scoped_refptr<CSSRule> parent_rule() const { return parent_rule_.get(); }

  // Custom, not in any spec.
  //

  void set_parent_rule(CSSRule* parent_rule) {
    parent_rule_ = base::AsWeakPtr(parent_rule);
  }

  scoped_refptr<const CSSStyleDeclarationData> data() { return data_; }

  void set_mutation_observer(MutationObserver* observer) {
    mutation_observer_ = observer;
  }

  DEFINE_WRAPPABLE_TYPE(CSSStyleDeclaration);

 private:
  ~CSSStyleDeclaration();

  void RecordMutation();

  scoped_refptr<CSSStyleDeclarationData> data_;

  base::WeakPtr<CSSRule> parent_rule_;
  CSSParser* const css_parser_;
  MutationObserver* mutation_observer_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_STYLE_DECLARATION_H_

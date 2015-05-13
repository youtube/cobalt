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

#include "base/memory/scoped_ptr.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSParser;
class MutationObserver;
class StyleSheet;

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

  std::string GetPropertyValue(const std::string& property_name);
  void SetPropertyValue(const std::string& property_name,
                        const std::string& property_value);

  std::string background() const;
  void set_background(const std::string& background);

  std::string background_color() const;
  void set_background_color(const std::string& background_color);

  std::string background_image() const;
  void set_background_image(const std::string& background_image);

  std::string border_radius() const;
  void set_border_radius(const std::string& border_radius);

  std::string color() const;
  void set_color(const std::string& color);

  std::string display() const;
  void set_display(const std::string& display);

  std::string font_family() const;
  void set_font_family(const std::string& font_family);

  std::string font_size() const;
  void set_font_size(const std::string& font_size);

  std::string font_weight() const;
  void set_font_weight(const std::string& font_weight);

  std::string height() const;
  void set_height(const std::string& height);

  std::string line_height() const;
  void set_line_height(const std::string& line_height);

  std::string opacity() const;
  void set_opacity(const std::string& opacity);

  std::string overflow() const;
  void set_overflow(const std::string& overflow);

  std::string transform() const;
  void set_transform(const std::string& transform);

  std::string transition_delay() const;
  void set_transition_delay(const std::string& transition_delay);

  std::string transition_duration() const;
  void set_transition_duration(const std::string& transition_duration);

  std::string transition_property() const;
  void set_transition_property(const std::string& transition_property);

  std::string transition_timing_function() const;
  void set_transition_timing_function(
      const std::string& transition_timing_function);

  std::string width() const;
  void set_width(const std::string& width);

  std::string css_text() const;
  void set_css_text(const std::string& css_text);

  // Custom, not in any spec.
  //

  scoped_refptr<const CSSStyleDeclarationData> data() { return data_; }

  void AttachToStyleSheet(StyleSheet* style_sheet);

  void set_mutation_observer(MutationObserver* observer) {
    mutation_observer_ = observer;
  }

  DEFINE_WRAPPABLE_TYPE(CSSStyleDeclaration);

 private:
  ~CSSStyleDeclaration();

  void RecordMutation();

  scoped_refptr<CSSStyleDeclarationData> data_;

  cssom::CSSParser* const css_parser_;
  MutationObserver* mutation_observer_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_STYLE_DECLARATION_H_

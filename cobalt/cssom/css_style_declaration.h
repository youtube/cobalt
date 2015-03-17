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
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// The CSSStyleDeclaration interface represents a CSS declaration block,
// including its underlying state, where this underlying state depends
// upon the source of the CSSStyleDeclaration instance.
//   http://dev.w3.org/csswg/cssom/#the-cssstyledeclaration-interface
class CSSStyleDeclaration : public base::RefCounted<CSSStyleDeclaration> {
 public:
  CSSStyleDeclaration();

  // Intentionally allowing compiler to generate copy constructor
  // and assignment operator.

  // Web API: CSSStyleDeclaration
  //

  scoped_refptr<PropertyValue> GetPropertyValue(
      const std::string& property_name);
  void SetPropertyValue(const std::string& property_name,
                        const scoped_refptr<PropertyValue>& property_value);

  // TODO(***REMOVED***): Consider defining types of the properties more precisely
  // by introducing an additional level of property value class hierarchy.
  // For example:
  //
  // Base class:
  // class PropertyValue {};
  //
  // "color" class hierarchy:
  // class UnresolvedColorValue : public PropertyValue {};
  // class ColorValue : public UnresolvedColorValue {};
  //
  // "font-size" class hierarchy:
  // class UnresolvedFontSizeValue : public PropertyValue {};
  // class LengthValue : public UnresolvedFontSizeValue {};
  // class PercentageValue : public UnresolvedFontSizeValue {};
  //
  // "initial" and "inherit" are applicable to both color and font size:
  // class InitialValue : public UnresolvedColorValue,
  //                      public UnresolvedFontSizeValue {};
  // class InheritValue : public UnresolvedColorValue,
  //                      public UnresolvedFontSizeValue {};
  //
  // Getter signatures then become:
  // UnresolvedColorValue* color() const;
  // UnresolvedFontSizeValue* font_size() const;

  const scoped_refptr<PropertyValue>& background_color() const {
    return background_color_;
  }
  void set_background_color(
      const scoped_refptr<PropertyValue>& background_color) {
    background_color_ = background_color;
  }

  const scoped_refptr<PropertyValue>& color() const { return color_; }
  void set_color(const scoped_refptr<PropertyValue>& color) { color_ = color; }

  const scoped_refptr<PropertyValue>& display() const { return display_; }
  void set_display(const scoped_refptr<PropertyValue>& display) {
    display_ = display;
  }

  const scoped_refptr<PropertyValue>& font_family() const {
    return font_family_;
  }
  void set_font_family(const scoped_refptr<PropertyValue>& font_family) {
    font_family_ = font_family;
  }

  const scoped_refptr<PropertyValue>& font_size() const { return font_size_; }
  void set_font_size(const scoped_refptr<PropertyValue>& font_size) {
    font_size_ = font_size;
  }

  const scoped_refptr<PropertyValue>& font_weight() const {
    return font_weight_;
  }
  void set_font_weight(const scoped_refptr<PropertyValue>& font_weight) {
    font_weight_ = font_weight;
  }

  const scoped_refptr<PropertyValue>& height() const { return height_; }
  void set_height(const scoped_refptr<PropertyValue>& height) {
    height_ = height;
  }

  const scoped_refptr<PropertyValue>& opacity() const { return opacity_; }
  void set_opacity(const scoped_refptr<PropertyValue>& opacity) {
    opacity_ = opacity;
  }

  const scoped_refptr<PropertyValue>& overflow() const { return overflow_; }
  void set_overflow(const scoped_refptr<PropertyValue>& overflow) {
    overflow_ = overflow;
  }

  const scoped_refptr<PropertyValue>& transform() const { return transform_; }
  void set_transform(const scoped_refptr<PropertyValue>& transform) {
    transform_ = transform;
  }

  const scoped_refptr<PropertyValue>& width() const { return width_; }
  void set_width(const scoped_refptr<PropertyValue>& width) { width_ = width; }

  // Custom, not in any spec.
  //

  void AssignFrom(const CSSStyleDeclaration& rhs);

 private:
  ~CSSStyleDeclaration();

  scoped_refptr<PropertyValue> background_color_;
  scoped_refptr<PropertyValue> color_;
  scoped_refptr<PropertyValue> display_;
  scoped_refptr<PropertyValue> font_family_;
  scoped_refptr<PropertyValue> font_size_;
  scoped_refptr<PropertyValue> font_weight_;
  scoped_refptr<PropertyValue> height_;
  scoped_refptr<PropertyValue> opacity_;
  scoped_refptr<PropertyValue> overflow_;
  scoped_refptr<PropertyValue> transform_;
  scoped_refptr<PropertyValue> width_;

  friend class base::RefCounted<CSSStyleDeclaration>;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_STYLE_DECLARATION_H_

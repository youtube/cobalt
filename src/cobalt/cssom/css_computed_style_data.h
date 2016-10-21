/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_CSS_COMPUTED_STYLE_DATA_H_
#define COBALT_CSSOM_CSS_COMPUTED_STYLE_DATA_H_

#include <functional>
#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/small_map.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/unused.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

class CSSComputedStyleDeclaration;

// CSSComputedStyleData which has PropertyValue type properties only used
// internally and it is not exposed to JavaScript.
class CSSComputedStyleData : public base::RefCounted<CSSComputedStyleData> {
 public:
  // This class provides the ability to determine whether the properties of two
  // CSSComputedStyleData objects match for a given set of property keys.
  class PropertySetMatcher {
   public:
    PropertySetMatcher() {}

    explicit PropertySetMatcher(const PropertyKeyVector& properties);

    bool DoDeclaredPropertiesMatch(
        const scoped_refptr<const CSSComputedStyleData>& lhs,
        const scoped_refptr<const CSSComputedStyleData>& rhs) const;

   private:
    PropertyKeyVector properties_;
    LonghandPropertiesBitset properties_bitset_;
  };

  // Time spent during initial layout and incremental layout is the shortest
  // with an array size of 16.
  typedef base::SmallMap<std::map<PropertyKey, scoped_refptr<PropertyValue> >,
                         16, std::equal_to<PropertyKey> > PropertyValues;

  CSSComputedStyleData();
  ~CSSComputedStyleData();

  // The length attribute must return the number of CSS declarations in the
  // declarations.
  //   https://www.w3.org/TR/cssom/#dom-cssstyledeclaration-length
  unsigned int length() const;

  // The item(index) method must return the property name of the CSS declaration
  // at position index.
  //  https://www.w3.org/TR/cssom/#dom-cssstyledeclaration-item
  const char* Item(unsigned int index) const;

  void SetPropertyValue(const PropertyKey key,
                        const scoped_refptr<PropertyValue>& value);

  // TODO: Define types of the properties more precisely using
  // non-intrusive visitors, like boost::variant (which can serve as inspiration
  // for our own base::variant).
  //
  // For example:
  //
  //   // Value: <length> | <percentage> | auto | inherit | initial | unset
  //   typedef base::variant<Length, Percentage, Auto, Inherit, Initial, Unset>
  //           DeclaredWidth;
  //
  //   // Same as above but with "inherit", "initial", and "unset" resolved.
  //   typedef base::variant<Length, Percentage, Auto>
  //           SpecifiedWidth;
  //
  //   // Computed value: the percentage or "auto" as specified
  //   //                 or the absolute length.
  //   typedef base::variant<float, Percentage, Auto>
  //           ComputedWidth;

  const scoped_refptr<PropertyValue>& animation_delay() const {
    return GetPropertyValueReference(kAnimationDelayProperty);
  }
  void set_animation_delay(
      const scoped_refptr<PropertyValue>& animation_delay) {
    SetPropertyValue(kAnimationDelayProperty, animation_delay);
  }

  const scoped_refptr<PropertyValue>& animation_direction() const {
    return GetPropertyValueReference(kAnimationDirectionProperty);
  }
  void set_animation_direction(
      const scoped_refptr<PropertyValue>& animation_direction) {
    SetPropertyValue(kAnimationDirectionProperty, animation_direction);
  }

  const scoped_refptr<PropertyValue>& animation_duration() const {
    return GetPropertyValueReference(kAnimationDurationProperty);
  }
  void set_animation_duration(
      const scoped_refptr<PropertyValue>& animation_duration) {
    SetPropertyValue(kAnimationDurationProperty, animation_duration);
  }

  const scoped_refptr<PropertyValue>& animation_fill_mode() const {
    return GetPropertyValueReference(kAnimationFillModeProperty);
  }
  void set_animation_fill_mode(
      const scoped_refptr<PropertyValue>& animation_fill_mode) {
    SetPropertyValue(kAnimationFillModeProperty, animation_fill_mode);
  }

  const scoped_refptr<PropertyValue>& animation_iteration_count() const {
    return GetPropertyValueReference(kAnimationIterationCountProperty);
  }
  void set_animation_iteration_count(
      const scoped_refptr<PropertyValue>& animation_iteration_count) {
    SetPropertyValue(kAnimationIterationCountProperty,
                     animation_iteration_count);
  }

  const scoped_refptr<PropertyValue>& animation_name() const {
    return GetPropertyValueReference(kAnimationNameProperty);
  }
  void set_animation_name(const scoped_refptr<PropertyValue>& animation_name) {
    SetPropertyValue(kAnimationNameProperty, animation_name);
  }

  const scoped_refptr<PropertyValue>& animation_timing_function() const {
    return GetPropertyValueReference(kAnimationTimingFunctionProperty);
  }
  void set_animation_timing_function(
      const scoped_refptr<PropertyValue>& animation_timing_function) {
    SetPropertyValue(kAnimationTimingFunctionProperty,
                     animation_timing_function);
  }

  const scoped_refptr<PropertyValue>& background_color() const {
    return GetPropertyValueReference(kBackgroundColorProperty);
  }
  void set_background_color(
      const scoped_refptr<PropertyValue>& background_color) {
    SetPropertyValue(kBackgroundColorProperty, background_color);
  }

  const scoped_refptr<PropertyValue>& background_image() const {
    return GetPropertyValueReference(kBackgroundImageProperty);
  }
  void set_background_image(
      const scoped_refptr<PropertyValue>& background_image) {
    SetPropertyValue(kBackgroundImageProperty, background_image);
  }

  const scoped_refptr<PropertyValue>& background_position() const {
    return GetPropertyValueReference(kBackgroundPositionProperty);
  }
  void set_background_position(
      const scoped_refptr<PropertyValue>& background_position) {
    SetPropertyValue(kBackgroundPositionProperty, background_position);
  }

  const scoped_refptr<PropertyValue>& background_repeat() const {
    return GetPropertyValueReference(kBackgroundRepeatProperty);
  }
  void set_background_repeat(
      const scoped_refptr<PropertyValue>& background_repeat) {
    SetPropertyValue(kBackgroundRepeatProperty, background_repeat);
  }

  const scoped_refptr<PropertyValue>& background_size() const {
    return GetPropertyValueReference(kBackgroundSizeProperty);
  }
  void set_background_size(
      const scoped_refptr<PropertyValue>& background_size) {
    SetPropertyValue(kBackgroundSizeProperty, background_size);
  }

  const scoped_refptr<PropertyValue>& border_top_color() const {
    return GetPropertyValueReference(kBorderTopColorProperty);
  }
  void set_border_top_color(
      const scoped_refptr<PropertyValue>& border_top_color) {
    SetPropertyValue(kBorderTopColorProperty, border_top_color);
  }

  const scoped_refptr<PropertyValue>& border_right_color() const {
    return GetPropertyValueReference(kBorderRightColorProperty);
  }
  void set_border_right_color(
      const scoped_refptr<PropertyValue>& border_right_color) {
    SetPropertyValue(kBorderRightColorProperty, border_right_color);
  }

  const scoped_refptr<PropertyValue>& border_bottom_color() const {
    return GetPropertyValueReference(kBorderBottomColorProperty);
  }
  void set_border_bottom_color(
      const scoped_refptr<PropertyValue>& border_bottom_color) {
    SetPropertyValue(kBorderBottomColorProperty, border_bottom_color);
  }

  const scoped_refptr<PropertyValue>& border_left_color() const {
    return GetPropertyValueReference(kBorderLeftColorProperty);
  }
  void set_border_left_color(
      const scoped_refptr<PropertyValue>& border_left_color) {
    SetPropertyValue(kBorderLeftColorProperty, border_left_color);
  }

  const scoped_refptr<PropertyValue>& border_top_style() const {
    return GetPropertyValueReference(kBorderTopStyleProperty);
  }
  void set_border_top_style(
      const scoped_refptr<PropertyValue>& border_top_style) {
    SetPropertyValue(kBorderTopStyleProperty, border_top_style);
  }

  const scoped_refptr<PropertyValue>& border_right_style() const {
    return GetPropertyValueReference(kBorderRightStyleProperty);
  }
  void set_border_right_style(
      const scoped_refptr<PropertyValue>& border_right_style) {
    SetPropertyValue(kBorderRightStyleProperty, border_right_style);
  }

  const scoped_refptr<PropertyValue>& border_bottom_style() const {
    return GetPropertyValueReference(kBorderBottomStyleProperty);
  }
  void set_border_bottom_style(
      const scoped_refptr<PropertyValue>& border_bottom_style) {
    SetPropertyValue(kBorderBottomStyleProperty, border_bottom_style);
  }

  const scoped_refptr<PropertyValue>& border_left_style() const {
    return GetPropertyValueReference(kBorderLeftStyleProperty);
  }
  void set_border_left_style(
      const scoped_refptr<PropertyValue>& border_left_style) {
    SetPropertyValue(kBorderLeftStyleProperty, border_left_style);
  }

  const scoped_refptr<PropertyValue>& border_top_width() const {
    return GetPropertyValueReference(kBorderTopWidthProperty);
  }
  void set_border_top_width(
      const scoped_refptr<PropertyValue>& border_top_width) {
    SetPropertyValue(kBorderTopWidthProperty, border_top_width);
  }

  const scoped_refptr<PropertyValue>& border_right_width() const {
    return GetPropertyValueReference(kBorderRightWidthProperty);
  }
  void set_border_right_width(
      const scoped_refptr<PropertyValue>& border_right_width) {
    SetPropertyValue(kBorderRightWidthProperty, border_right_width);
  }

  const scoped_refptr<PropertyValue>& border_bottom_width() const {
    return GetPropertyValueReference(kBorderBottomWidthProperty);
  }
  void set_border_bottom_width(
      const scoped_refptr<PropertyValue>& border_bottom_width) {
    SetPropertyValue(kBorderBottomWidthProperty, border_bottom_width);
  }

  const scoped_refptr<PropertyValue>& border_left_width() const {
    return GetPropertyValueReference(kBorderLeftWidthProperty);
  }
  void set_border_left_width(
      const scoped_refptr<PropertyValue>& border_left_width) {
    SetPropertyValue(kBorderLeftWidthProperty, border_left_width);
  }

  const scoped_refptr<PropertyValue>& border_radius() const {
    return GetPropertyValueReference(kBorderRadiusProperty);
  }
  void set_border_radius(const scoped_refptr<PropertyValue>& border_radius) {
    SetPropertyValue(kBorderRadiusProperty, border_radius);
  }

  const scoped_refptr<PropertyValue>& bottom() const {
    return GetPropertyValueReference(kBottomProperty);
  }
  void set_bottom(const scoped_refptr<PropertyValue>& bottom) {
    SetPropertyValue(kBottomProperty, bottom);
  }

  const scoped_refptr<PropertyValue>& box_shadow() const {
    return GetPropertyValueReference(kBoxShadowProperty);
  }
  void set_box_shadow(const scoped_refptr<PropertyValue>& box_shadow) {
    SetPropertyValue(kBoxShadowProperty, box_shadow);
  }

  const scoped_refptr<PropertyValue>& color() const {
    return GetPropertyValueReference(kColorProperty);
  }
  void set_color(const scoped_refptr<PropertyValue>& color) {
    SetPropertyValue(kColorProperty, color);
  }

  const scoped_refptr<PropertyValue>& content() const {
    return GetPropertyValueReference(kContentProperty);
  }
  void set_content(const scoped_refptr<PropertyValue>& content) {
    SetPropertyValue(kContentProperty, content);
  }

  const scoped_refptr<PropertyValue>& display() const {
    return GetPropertyValueReference(kDisplayProperty);
  }
  void set_display(const scoped_refptr<PropertyValue>& display) {
    SetPropertyValue(kDisplayProperty, display);
  }

  const scoped_refptr<PropertyValue>& filter() const {
    return GetPropertyValueReference(kFilterProperty);
  }
  void set_filter(const scoped_refptr<PropertyValue>& filter) {
    SetPropertyValue(kFilterProperty, filter);
  }

  const scoped_refptr<PropertyValue>& font_family() const {
    return GetPropertyValueReference(kFontFamilyProperty);
  }
  void set_font_family(const scoped_refptr<PropertyValue>& font_family) {
    SetPropertyValue(kFontFamilyProperty, font_family);
  }

  const scoped_refptr<PropertyValue>& font_size() const {
    return GetPropertyValueReference(kFontSizeProperty);
  }
  void set_font_size(const scoped_refptr<PropertyValue>& font_size) {
    SetPropertyValue(kFontSizeProperty, font_size);
  }

  const scoped_refptr<PropertyValue>& font_style() const {
    return GetPropertyValueReference(kFontStyleProperty);
  }
  void set_font_style(const scoped_refptr<PropertyValue>& font_style) {
    SetPropertyValue(kFontStyleProperty, font_style);
  }

  const scoped_refptr<PropertyValue>& font_weight() const {
    return GetPropertyValueReference(kFontWeightProperty);
  }
  void set_font_weight(const scoped_refptr<PropertyValue>& font_weight) {
    SetPropertyValue(kFontWeightProperty, font_weight);
  }

  const scoped_refptr<PropertyValue>& height() const {
    return GetPropertyValueReference(kHeightProperty);
  }
  void set_height(const scoped_refptr<PropertyValue>& height) {
    SetPropertyValue(kHeightProperty, height);
  }

  const scoped_refptr<PropertyValue>& left() const {
    return GetPropertyValueReference(kLeftProperty);
  }
  void set_left(const scoped_refptr<PropertyValue>& left) {
    SetPropertyValue(kLeftProperty, left);
  }

  const scoped_refptr<PropertyValue>& line_height() const {
    return GetPropertyValueReference(kLineHeightProperty);
  }
  void set_line_height(const scoped_refptr<PropertyValue>& line_height) {
    SetPropertyValue(kLineHeightProperty, line_height);
  }

  const scoped_refptr<PropertyValue>& margin() const {
    return GetPropertyValueReference(kMarginProperty);
  }
  void set_margin(const scoped_refptr<PropertyValue>& margin) {
    SetPropertyValue(kMarginProperty, margin);
  }

  const scoped_refptr<PropertyValue>& margin_bottom() const {
    return GetPropertyValueReference(kMarginBottomProperty);
  }
  void set_margin_bottom(const scoped_refptr<PropertyValue>& margin_bottom) {
    SetPropertyValue(kMarginBottomProperty, margin_bottom);
  }

  const scoped_refptr<PropertyValue>& margin_left() const {
    return GetPropertyValueReference(kMarginLeftProperty);
  }
  void set_margin_left(const scoped_refptr<PropertyValue>& margin_left) {
    SetPropertyValue(kMarginLeftProperty, margin_left);
  }

  const scoped_refptr<PropertyValue>& margin_right() const {
    return GetPropertyValueReference(kMarginRightProperty);
  }
  void set_margin_right(const scoped_refptr<PropertyValue>& margin_right) {
    SetPropertyValue(kMarginRightProperty, margin_right);
  }

  const scoped_refptr<PropertyValue>& margin_top() const {
    return GetPropertyValueReference(kMarginTopProperty);
  }
  void set_margin_top(const scoped_refptr<PropertyValue>& margin_top) {
    SetPropertyValue(kMarginTopProperty, margin_top);
  }

  const scoped_refptr<PropertyValue>& max_height() const {
    return GetPropertyValueReference(kMaxHeightProperty);
  }
  void set_max_height(const scoped_refptr<PropertyValue>& max_height) {
    SetPropertyValue(kMaxHeightProperty, max_height);
  }

  const scoped_refptr<PropertyValue>& max_width() const {
    return GetPropertyValueReference(kMaxWidthProperty);
  }
  void set_max_width(const scoped_refptr<PropertyValue>& max_width) {
    SetPropertyValue(kMaxWidthProperty, max_width);
  }

  const scoped_refptr<PropertyValue>& min_height() const {
    return GetPropertyValueReference(kMinHeightProperty);
  }
  void set_min_height(const scoped_refptr<PropertyValue>& min_height) {
    SetPropertyValue(kMinHeightProperty, min_height);
  }

  const scoped_refptr<PropertyValue>& min_width() const {
    return GetPropertyValueReference(kMinWidthProperty);
  }
  void set_min_width(const scoped_refptr<PropertyValue>& min_width) {
    SetPropertyValue(kMinWidthProperty, min_width);
  }

  const scoped_refptr<PropertyValue>& opacity() const {
    return GetPropertyValueReference(kOpacityProperty);
  }
  void set_opacity(const scoped_refptr<PropertyValue>& opacity) {
    SetPropertyValue(kOpacityProperty, opacity);
  }

  const scoped_refptr<PropertyValue>& overflow() const {
    return GetPropertyValueReference(kOverflowProperty);
  }
  void set_overflow(const scoped_refptr<PropertyValue>& overflow) {
    SetPropertyValue(kOverflowProperty, overflow);
  }

  const scoped_refptr<PropertyValue>& overflow_wrap() const {
    return GetPropertyValueReference(kOverflowWrapProperty);
  }
  void set_overflow_wrap(const scoped_refptr<PropertyValue>& overflow_wrap) {
    SetPropertyValue(kOverflowWrapProperty, overflow_wrap);
  }

  const scoped_refptr<PropertyValue>& padding_bottom() const {
    return GetPropertyValueReference(kPaddingBottomProperty);
  }
  void set_padding_bottom(const scoped_refptr<PropertyValue>& padding_bottom) {
    SetPropertyValue(kPaddingBottomProperty, padding_bottom);
  }

  const scoped_refptr<PropertyValue>& padding_left() const {
    return GetPropertyValueReference(kPaddingLeftProperty);
  }
  void set_padding_left(const scoped_refptr<PropertyValue>& padding_left) {
    SetPropertyValue(kPaddingLeftProperty, padding_left);
  }

  const scoped_refptr<PropertyValue>& padding_right() const {
    return GetPropertyValueReference(kPaddingRightProperty);
  }
  void set_padding_right(const scoped_refptr<PropertyValue>& padding_right) {
    SetPropertyValue(kPaddingRightProperty, padding_right);
  }

  const scoped_refptr<PropertyValue>& padding_top() const {
    return GetPropertyValueReference(kPaddingTopProperty);
  }
  void set_padding_top(const scoped_refptr<PropertyValue>& padding_top) {
    SetPropertyValue(kPaddingTopProperty, padding_top);
  }

  const scoped_refptr<PropertyValue>& position() const {
    return GetPropertyValueReference(kPositionProperty);
  }
  void set_position(const scoped_refptr<PropertyValue>& position) {
    SetPropertyValue(kPositionProperty, position);
  }

  const scoped_refptr<PropertyValue>& right() const {
    return GetPropertyValueReference(kRightProperty);
  }
  void set_right(const scoped_refptr<PropertyValue>& right) {
    SetPropertyValue(kRightProperty, right);
  }

  const scoped_refptr<PropertyValue>& text_align() const {
    return GetPropertyValueReference(kTextAlignProperty);
  }
  void set_text_align(const scoped_refptr<PropertyValue>& text_align) {
    SetPropertyValue(kTextAlignProperty, text_align);
  }

  const scoped_refptr<PropertyValue>& text_decoration_color() const {
    return GetPropertyValueReference(kTextDecorationColorProperty);
  }
  void set_text_decoration_color(
      const scoped_refptr<PropertyValue>& text_decoration_color) {
    SetPropertyValue(kTextDecorationColorProperty, text_decoration_color);
  }

  const scoped_refptr<PropertyValue>& text_decoration_line() const {
    return GetPropertyValueReference(kTextDecorationLineProperty);
  }
  void set_text_decoration_line(
      const scoped_refptr<PropertyValue>& text_decoration_line) {
    SetPropertyValue(kTextDecorationLineProperty, text_decoration_line);
  }

  const scoped_refptr<PropertyValue>& text_indent() const {
    return GetPropertyValueReference(kTextIndentProperty);
  }
  void set_text_indent(const scoped_refptr<PropertyValue>& text_indent) {
    SetPropertyValue(kTextIndentProperty, text_indent);
  }

  const scoped_refptr<PropertyValue>& text_overflow() const {
    return GetPropertyValueReference(kTextOverflowProperty);
  }
  void set_text_overflow(const scoped_refptr<PropertyValue>& text_overflow) {
    SetPropertyValue(kTextOverflowProperty, text_overflow);
  }

  const scoped_refptr<PropertyValue>& text_shadow() const {
    return GetPropertyValueReference(kTextShadowProperty);
  }
  void set_text_shadow(const scoped_refptr<PropertyValue>& text_shadow) {
    SetPropertyValue(kTextShadowProperty, text_shadow);
  }

  const scoped_refptr<PropertyValue>& text_transform() const {
    return GetPropertyValueReference(kTextTransformProperty);
  }
  void set_text_transform(const scoped_refptr<PropertyValue>& text_transform) {
    SetPropertyValue(kTextTransformProperty, text_transform);
  }

  const scoped_refptr<PropertyValue>& top() const {
    return GetPropertyValueReference(kTopProperty);
  }
  void set_top(const scoped_refptr<PropertyValue>& top) {
    SetPropertyValue(kTopProperty, top);
  }

  const scoped_refptr<PropertyValue>& transform() const {
    return GetPropertyValueReference(kTransformProperty);
  }
  void set_transform(const scoped_refptr<PropertyValue>& transform) {
    SetPropertyValue(kTransformProperty, transform);
  }

  const scoped_refptr<PropertyValue>& transform_origin() const {
    return GetPropertyValueReference(kTransformOriginProperty);
  }
  void set_transform_origin(
      const scoped_refptr<PropertyValue>& transform_origin) {
    SetPropertyValue(kTransformOriginProperty, transform_origin);
  }

  const scoped_refptr<PropertyValue>& transition_delay() const {
    return GetPropertyValueReference(kTransitionDelayProperty);
  }
  void set_transition_delay(
      const scoped_refptr<PropertyValue>& transition_delay) {
    SetPropertyValue(kTransitionDelayProperty, transition_delay);
  }

  const scoped_refptr<PropertyValue>& transition_duration() const {
    return GetPropertyValueReference(kTransitionDurationProperty);
  }
  void set_transition_duration(
      const scoped_refptr<PropertyValue>& transition_duration) {
    SetPropertyValue(kTransitionDurationProperty, transition_duration);
  }

  const scoped_refptr<PropertyValue>& transition_property() const {
    return GetPropertyValueReference(kTransitionPropertyProperty);
  }
  void set_transition_property(
      const scoped_refptr<PropertyValue>& transition_property) {
    SetPropertyValue(kTransitionPropertyProperty, transition_property);
  }

  const scoped_refptr<PropertyValue>& transition_timing_function() const {
    return GetPropertyValueReference(kTransitionTimingFunctionProperty);
  }
  void set_transition_timing_function(
      const scoped_refptr<PropertyValue>& transition_timing_function) {
    SetPropertyValue(kTransitionTimingFunctionProperty,
                     transition_timing_function);
  }

  const scoped_refptr<PropertyValue>& vertical_align() const {
    return GetPropertyValueReference(kVerticalAlignProperty);
  }
  void set_vertical_align(const scoped_refptr<PropertyValue>& vertical_align) {
    SetPropertyValue(kVerticalAlignProperty, vertical_align);
  }

  const scoped_refptr<PropertyValue>& visibility() const {
    return GetPropertyValueReference(kVisibilityProperty);
  }
  void set_visibility(const scoped_refptr<PropertyValue>& visibility) {
    SetPropertyValue(kVisibilityProperty, visibility);
  }

  const scoped_refptr<PropertyValue>& white_space() const {
    return GetPropertyValueReference(kWhiteSpaceProperty);
  }
  void set_white_space(const scoped_refptr<PropertyValue>& white_space) {
    SetPropertyValue(kWhiteSpaceProperty, white_space);
  }

  const scoped_refptr<PropertyValue>& width() const {
    return GetPropertyValueReference(kWidthProperty);
  }
  void set_width(const scoped_refptr<PropertyValue>& width) {
    SetPropertyValue(kWidthProperty, width);
  }

  const scoped_refptr<PropertyValue>& z_index() const {
    return GetPropertyValueReference(kZIndexProperty);
  }
  void set_z_index(const scoped_refptr<PropertyValue>& z_index) {
    SetPropertyValue(kZIndexProperty, z_index);
  }

  // From CSSDeclarationData
  //
  scoped_refptr<PropertyValue> GetPropertyValue(PropertyKey key) const {
    return GetPropertyValueReference(key);
  }

  // Rest of public methods.

  const scoped_refptr<PropertyValue>& GetPropertyValueReference(
      PropertyKey key) const;

  scoped_refptr<PropertyValue>& GetDeclaredPropertyValueReference(
      PropertyKey key);

  void AssignFrom(const CSSComputedStyleData& rhs);

  // This returns the result of serializing a CSS declaration block.
  // The current implementation does not handle shorthands.
  //   https://www.w3.org/TR/cssom/#serialize-a-css-declaration-block
  std::string SerializeCSSDeclarationBlock() const;

  // Returns true if the property is explicitly declared in this style, as
  // opposed to implicitly inheriting from its parent or the initial value.
  bool IsDeclared(const PropertyKey key) const {
    return declared_properties_[key];
  }

  // Whether or not any inherited properties have been declared.
  // NOTE: Inherited properties that are set to a value "inherit" do not impact
  // this flag, as they will have the same value as the parent and can be
  // skipped by descendants retrieving their inherited value without impacting
  // the returned value.
  bool has_declared_inherited_properties() const {
    return has_declared_inherited_properties_;
  }

  // Adds a declared property that was inherited from the parent to an
  // internal list. This facilitates tracking of whether or not a value that was
  // initially set to a parent's value continues to match the parent's value.
  void AddDeclaredPropertyInheritedFromParent(PropertyKey key);

  // Returns true if all of the declared properties that were inherited from the
  // parent are still valid. They become invalid when the parent's value
  // changes.
  bool AreDeclaredPropertiesInheritedFromParentValid() const;

  PropertyValues* declared_property_values() {
    return &declared_property_values_;
  }

  // Returns whether or not the declared properties in the passed in
  // CSSComputedStyleData matches those declared within this one.
  bool DoDeclaredPropertiesMatch(
      const scoped_refptr<const CSSComputedStyleData>& other) const;

  // Set the parent computed style for tracking inherited properties.
  void SetParentComputedStyleDeclaration(
      const scoped_refptr<CSSComputedStyleDeclaration>&
          parent_computed_style_declaration);

  // Returns the parent computed style used for tracking inherited properties.
  const scoped_refptr<CSSComputedStyleDeclaration>&
  GetParentComputedStyleDeclaration() const;

 private:
  // Helper function that returns the computed value if the initial property
  // value is not computed or returns the initial value if it is computed.
  const scoped_refptr<PropertyValue>& GetComputedInitialValue(
      PropertyKey key) const;

  bool IsBorderStyleNoneOrHiddenForAnEdge(PropertyKey key) const;

  LonghandPropertiesBitset declared_properties_;
  PropertyValues declared_property_values_;

  // True if this style has any inherited properties declared.
  // NOTE: Inherited properties that are set to a value "inherit" do not impact
  // this flag, as they will have the same value as the parent and can be
  // skipped by descendants retrieving their inherited value without impacting
  // the returned value.
  bool has_declared_inherited_properties_;

  // Properties that were initially set to a value of "inherit" before being
  // updated with the parent's value. This is used to determine whether the
  // declared properties inherited from the parent have subsequently changed.
  PropertyKeyVector declared_properties_inherited_from_parent_;

  // The parent used for inherited properties.
  // NOTE: The parent is a CSSComputedStyleDeclaration, rather than a
  // CSSComputedStyleData, in order to allow for the replacement of ancestor
  // CSSComputedStyleData objects without requiring all of its descendants to
  // also be replaced. The descendant's inherited property value will instead
  // dynamically update.
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_COMPUTED_STYLE_DATA_H_

// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_CSS_COMPUTED_STYLE_DATA_H_
#define COBALT_CSSOM_CSS_COMPUTED_STYLE_DATA_H_

#include <functional>
#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/small_map.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/unused.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

class CSSComputedStyleDeclaration;

// CSSComputedStyleData which has PropertyValue type properties only used
// internally and it is not exposed to JavaScript.
class CSSComputedStyleData
    : public base::RefCountedThreadSafe<CSSComputedStyleData> {
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
  typedef base::small_map<std::map<PropertyKey, scoped_refptr<PropertyValue> >,
                          16, std::equal_to<PropertyKey> >
      PropertyValues;

  CSSComputedStyleData();
  ~CSSComputedStyleData();

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

  const scoped_refptr<PropertyValue>& align_content() const {
    return GetPropertyValueReference(kAlignContentProperty);
  }

  const scoped_refptr<PropertyValue>& align_items() const {
    return GetPropertyValueReference(kAlignItemsProperty);
  }

  const scoped_refptr<PropertyValue>& animation_delay() const {
    return GetPropertyValueReference(kAnimationDelayProperty);
  }

  const scoped_refptr<PropertyValue>& animation_direction() const {
    return GetPropertyValueReference(kAnimationDirectionProperty);
  }

  const scoped_refptr<PropertyValue>& animation_duration() const {
    return GetPropertyValueReference(kAnimationDurationProperty);
  }

  const scoped_refptr<PropertyValue>& animation_fill_mode() const {
    return GetPropertyValueReference(kAnimationFillModeProperty);
  }

  const scoped_refptr<PropertyValue>& animation_iteration_count() const {
    return GetPropertyValueReference(kAnimationIterationCountProperty);
  }

  const scoped_refptr<PropertyValue>& animation_name() const {
    return GetPropertyValueReference(kAnimationNameProperty);
  }

  const scoped_refptr<PropertyValue>& animation_timing_function() const {
    return GetPropertyValueReference(kAnimationTimingFunctionProperty);
  }

  const scoped_refptr<PropertyValue>& background_color() const {
    return GetPropertyValueReference(kBackgroundColorProperty);
  }

  const scoped_refptr<PropertyValue>& background_image() const {
    return GetPropertyValueReference(kBackgroundImageProperty);
  }

  const scoped_refptr<PropertyValue>& background_position() const {
    return GetPropertyValueReference(kBackgroundPositionProperty);
  }

  const scoped_refptr<PropertyValue>& background_repeat() const {
    return GetPropertyValueReference(kBackgroundRepeatProperty);
  }

  const scoped_refptr<PropertyValue>& background_size() const {
    return GetPropertyValueReference(kBackgroundSizeProperty);
  }

  const scoped_refptr<PropertyValue>& border_top_color() const {
    return GetPropertyValueReference(kBorderTopColorProperty);
  }

  const scoped_refptr<PropertyValue>& border_right_color() const {
    return GetPropertyValueReference(kBorderRightColorProperty);
  }

  const scoped_refptr<PropertyValue>& border_bottom_color() const {
    return GetPropertyValueReference(kBorderBottomColorProperty);
  }

  const scoped_refptr<PropertyValue>& border_left_color() const {
    return GetPropertyValueReference(kBorderLeftColorProperty);
  }

  const scoped_refptr<PropertyValue>& border_top_style() const {
    return GetPropertyValueReference(kBorderTopStyleProperty);
  }

  const scoped_refptr<PropertyValue>& border_right_style() const {
    return GetPropertyValueReference(kBorderRightStyleProperty);
  }

  const scoped_refptr<PropertyValue>& border_bottom_style() const {
    return GetPropertyValueReference(kBorderBottomStyleProperty);
  }

  const scoped_refptr<PropertyValue>& border_left_style() const {
    return GetPropertyValueReference(kBorderLeftStyleProperty);
  }

  const scoped_refptr<PropertyValue>& border_top_width() const {
    return GetPropertyValueReference(kBorderTopWidthProperty);
  }

  const scoped_refptr<PropertyValue>& border_right_width() const {
    return GetPropertyValueReference(kBorderRightWidthProperty);
  }

  const scoped_refptr<PropertyValue>& border_bottom_width() const {
    return GetPropertyValueReference(kBorderBottomWidthProperty);
  }

  const scoped_refptr<PropertyValue>& border_left_width() const {
    return GetPropertyValueReference(kBorderLeftWidthProperty);
  }

  const scoped_refptr<PropertyValue>& border_top_left_radius() const {
    return GetPropertyValueReference(kBorderTopLeftRadiusProperty);
  }

  const scoped_refptr<PropertyValue>& border_top_right_radius() const {
    return GetPropertyValueReference(kBorderTopRightRadiusProperty);
  }

  const scoped_refptr<PropertyValue>& border_bottom_right_radius() const {
    return GetPropertyValueReference(kBorderBottomRightRadiusProperty);
  }

  const scoped_refptr<PropertyValue>& border_bottom_left_radius() const {
    return GetPropertyValueReference(kBorderBottomLeftRadiusProperty);
  }

  const scoped_refptr<PropertyValue>& bottom() const {
    return GetPropertyValueReference(kBottomProperty);
  }

  const scoped_refptr<PropertyValue>& box_shadow() const {
    return GetPropertyValueReference(kBoxShadowProperty);
  }

  const scoped_refptr<PropertyValue>& color() const {
    return GetPropertyValueReference(kColorProperty);
  }

  const scoped_refptr<PropertyValue>& content() const {
    return GetPropertyValueReference(kContentProperty);
  }

  const scoped_refptr<PropertyValue>& display() const {
    return GetPropertyValueReference(kDisplayProperty);
  }

  const scoped_refptr<PropertyValue>& filter() const {
    return GetPropertyValueReference(kFilterProperty);
  }

  const scoped_refptr<PropertyValue>& flex_basis() const {
    return GetPropertyValueReference(kFlexBasisProperty);
  }

  const scoped_refptr<PropertyValue>& flex_direction() const {
    return GetPropertyValueReference(kFlexDirectionProperty);
  }

  const scoped_refptr<PropertyValue>& flex_grow() const {
    return GetPropertyValueReference(kFlexGrowProperty);
  }

  const scoped_refptr<PropertyValue>& flex_shrink() const {
    return GetPropertyValueReference(kFlexShrinkProperty);
  }

  const scoped_refptr<PropertyValue>& flex_wrap() const {
    return GetPropertyValueReference(kFlexWrapProperty);
  }

  const scoped_refptr<PropertyValue>& font_family() const {
    return GetPropertyValueReference(kFontFamilyProperty);
  }

  const scoped_refptr<PropertyValue>& font_size() const {
    return GetPropertyValueReference(kFontSizeProperty);
  }

  const scoped_refptr<PropertyValue>& font_style() const {
    return GetPropertyValueReference(kFontStyleProperty);
  }

  const scoped_refptr<PropertyValue>& font_weight() const {
    return GetPropertyValueReference(kFontWeightProperty);
  }

  const scoped_refptr<PropertyValue>& height() const {
    return GetPropertyValueReference(kHeightProperty);
  }

  const scoped_refptr<PropertyValue>& justify_content() const {
    return GetPropertyValueReference(kJustifyContentProperty);
  }

  const scoped_refptr<PropertyValue>& left() const {
    return GetPropertyValueReference(kLeftProperty);
  }

  const scoped_refptr<PropertyValue>& line_height() const {
    return GetPropertyValueReference(kLineHeightProperty);
  }

  const scoped_refptr<PropertyValue>& margin_bottom() const {
    return GetPropertyValueReference(kMarginBottomProperty);
  }

  const scoped_refptr<PropertyValue>& margin_left() const {
    return GetPropertyValueReference(kMarginLeftProperty);
  }

  const scoped_refptr<PropertyValue>& margin_right() const {
    return GetPropertyValueReference(kMarginRightProperty);
  }

  const scoped_refptr<PropertyValue>& margin_top() const {
    return GetPropertyValueReference(kMarginTopProperty);
  }

  const scoped_refptr<PropertyValue>& max_height() const {
    return GetPropertyValueReference(kMaxHeightProperty);
  }

  const scoped_refptr<PropertyValue>& max_width() const {
    return GetPropertyValueReference(kMaxWidthProperty);
  }

  const scoped_refptr<PropertyValue>& min_height() const {
    return GetPropertyValueReference(kMinHeightProperty);
  }

  const scoped_refptr<PropertyValue>& min_width() const {
    return GetPropertyValueReference(kMinWidthProperty);
  }

  const scoped_refptr<PropertyValue>& opacity() const {
    return GetPropertyValueReference(kOpacityProperty);
  }

  const scoped_refptr<PropertyValue>& order() const {
    return GetPropertyValueReference(kOrderProperty);
  }

  const scoped_refptr<PropertyValue>& outline_color() const {
    return GetPropertyValueReference(kOutlineColorProperty);
  }

  const scoped_refptr<PropertyValue>& outline_style() const {
    return GetPropertyValueReference(kOutlineStyleProperty);
  }

  const scoped_refptr<PropertyValue>& outline_width() const {
    return GetPropertyValueReference(kOutlineWidthProperty);
  }

  const scoped_refptr<PropertyValue>& overflow() const {
    return GetPropertyValueReference(kOverflowProperty);
  }

  const scoped_refptr<PropertyValue>& overflow_wrap() const {
    return GetPropertyValueReference(kOverflowWrapProperty);
  }

  const scoped_refptr<PropertyValue>& padding_bottom() const {
    return GetPropertyValueReference(kPaddingBottomProperty);
  }

  const scoped_refptr<PropertyValue>& padding_left() const {
    return GetPropertyValueReference(kPaddingLeftProperty);
  }

  const scoped_refptr<PropertyValue>& padding_right() const {
    return GetPropertyValueReference(kPaddingRightProperty);
  }

  const scoped_refptr<PropertyValue>& padding_top() const {
    return GetPropertyValueReference(kPaddingTopProperty);
  }

  const scoped_refptr<PropertyValue>& pointer_events() const {
    return GetPropertyValueReference(kPointerEventsProperty);
  }

  const scoped_refptr<PropertyValue>& position() const {
    return GetPropertyValueReference(kPositionProperty);
  }

  const scoped_refptr<PropertyValue>& right() const {
    return GetPropertyValueReference(kRightProperty);
  }

  const scoped_refptr<PropertyValue>& text_align() const {
    return GetPropertyValueReference(kTextAlignProperty);
  }

  const scoped_refptr<PropertyValue>& text_decoration_color() const {
    return GetPropertyValueReference(kTextDecorationColorProperty);
  }

  const scoped_refptr<PropertyValue>& text_decoration_line() const {
    return GetPropertyValueReference(kTextDecorationLineProperty);
  }

  const scoped_refptr<PropertyValue>& text_indent() const {
    return GetPropertyValueReference(kTextIndentProperty);
  }

  const scoped_refptr<PropertyValue>& text_overflow() const {
    return GetPropertyValueReference(kTextOverflowProperty);
  }

  const scoped_refptr<PropertyValue>& text_shadow() const {
    return GetPropertyValueReference(kTextShadowProperty);
  }

  const scoped_refptr<PropertyValue>& text_transform() const {
    return GetPropertyValueReference(kTextTransformProperty);
  }

  const scoped_refptr<PropertyValue>& top() const {
    return GetPropertyValueReference(kTopProperty);
  }

  const scoped_refptr<PropertyValue>& transform() const {
    return GetPropertyValueReference(kTransformProperty);
  }

  const scoped_refptr<PropertyValue>& transform_origin() const {
    return GetPropertyValueReference(kTransformOriginProperty);
  }

  const scoped_refptr<PropertyValue>& transition_delay() const {
    return GetPropertyValueReference(kTransitionDelayProperty);
  }

  const scoped_refptr<PropertyValue>& transition_duration() const {
    return GetPropertyValueReference(kTransitionDurationProperty);
  }

  const scoped_refptr<PropertyValue>& transition_property() const {
    return GetPropertyValueReference(kTransitionPropertyProperty);
  }

  const scoped_refptr<PropertyValue>& transition_timing_function() const {
    return GetPropertyValueReference(kTransitionTimingFunctionProperty);
  }

  const scoped_refptr<PropertyValue>& vertical_align() const {
    return GetPropertyValueReference(kVerticalAlignProperty);
  }

  const scoped_refptr<PropertyValue>& visibility() const {
    return GetPropertyValueReference(kVisibilityProperty);
  }

  const scoped_refptr<PropertyValue>& white_space() const {
    return GetPropertyValueReference(kWhiteSpaceProperty);
  }

  const scoped_refptr<PropertyValue>& width() const {
    return GetPropertyValueReference(kWidthProperty);
  }

  const scoped_refptr<PropertyValue>& z_index() const {
    return GetPropertyValueReference(kZIndexProperty);
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

  // When an element is blockified, that should not affect the static position.
  //   https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width
  //   https://www.w3.org/TR/CSS21/visuren.html#dis-pos-flo
  // Return true if the element's outer display type was inline before any
  // optional blockificiation has occurred.
  bool is_inline_before_blockification() const {
    return is_inline_before_blockification_;
  }

  bool IsContainingBlockForPositionAbsoluteElements() const {
    return IsPositioned() || IsTransformed();
  }

  bool IsPositioned() const {
    return position() != cssom::KeywordValue::GetStatic();
  }

  bool IsTransformed() const {
    return transform() != cssom::KeywordValue::GetNone();
  }

 protected:
  void SetPropertyValue(const PropertyKey key,
                        const scoped_refptr<PropertyValue>& value);

  void set_is_inline_before_blockification(bool value) {
    is_inline_before_blockification_ = value;
  }

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
  bool has_declared_inherited_properties_ = false;

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

  // Stores whether the outer display type is inline before blockification.
  bool is_inline_before_blockification_ = true;
};

class MutableCSSComputedStyleData : public CSSComputedStyleData {
 public:
  void set_align_content(const scoped_refptr<PropertyValue>& align_content) {
    SetPropertyValue(kAlignContentProperty, align_content);
  }

  void set_align_items(const scoped_refptr<PropertyValue>& align_items) {
    SetPropertyValue(kAlignItemsProperty, align_items);
  }

  void set_align_self(const scoped_refptr<PropertyValue>& align_self) {
    SetPropertyValue(kAlignSelfProperty, align_self);
  }

  void set_animation_delay(
      const scoped_refptr<PropertyValue>& animation_delay) {
    SetPropertyValue(kAnimationDelayProperty, animation_delay);
  }

  void set_animation_direction(
      const scoped_refptr<PropertyValue>& animation_direction) {
    SetPropertyValue(kAnimationDirectionProperty, animation_direction);
  }

  void set_animation_duration(
      const scoped_refptr<PropertyValue>& animation_duration) {
    SetPropertyValue(kAnimationDurationProperty, animation_duration);
  }

  void set_animation_fill_mode(
      const scoped_refptr<PropertyValue>& animation_fill_mode) {
    SetPropertyValue(kAnimationFillModeProperty, animation_fill_mode);
  }

  void set_animation_iteration_count(
      const scoped_refptr<PropertyValue>& animation_iteration_count) {
    SetPropertyValue(kAnimationIterationCountProperty,
                     animation_iteration_count);
  }

  void set_animation_name(const scoped_refptr<PropertyValue>& animation_name) {
    SetPropertyValue(kAnimationNameProperty, animation_name);
  }

  void set_animation_timing_function(
      const scoped_refptr<PropertyValue>& animation_timing_function) {
    SetPropertyValue(kAnimationTimingFunctionProperty,
                     animation_timing_function);
  }

  void set_background_color(
      const scoped_refptr<PropertyValue>& background_color) {
    SetPropertyValue(kBackgroundColorProperty, background_color);
  }

  void set_background_image(
      const scoped_refptr<PropertyValue>& background_image) {
    SetPropertyValue(kBackgroundImageProperty, background_image);
  }

  void set_background_position(
      const scoped_refptr<PropertyValue>& background_position) {
    SetPropertyValue(kBackgroundPositionProperty, background_position);
  }

  void set_background_repeat(
      const scoped_refptr<PropertyValue>& background_repeat) {
    SetPropertyValue(kBackgroundRepeatProperty, background_repeat);
  }

  void set_background_size(
      const scoped_refptr<PropertyValue>& background_size) {
    SetPropertyValue(kBackgroundSizeProperty, background_size);
  }

  void set_border_top_color(
      const scoped_refptr<PropertyValue>& border_top_color) {
    SetPropertyValue(kBorderTopColorProperty, border_top_color);
  }

  void set_border_right_color(
      const scoped_refptr<PropertyValue>& border_right_color) {
    SetPropertyValue(kBorderRightColorProperty, border_right_color);
  }

  void set_border_bottom_color(
      const scoped_refptr<PropertyValue>& border_bottom_color) {
    SetPropertyValue(kBorderBottomColorProperty, border_bottom_color);
  }

  void set_border_left_color(
      const scoped_refptr<PropertyValue>& border_left_color) {
    SetPropertyValue(kBorderLeftColorProperty, border_left_color);
  }

  void set_border_top_style(
      const scoped_refptr<PropertyValue>& border_top_style) {
    SetPropertyValue(kBorderTopStyleProperty, border_top_style);
  }

  void set_border_right_style(
      const scoped_refptr<PropertyValue>& border_right_style) {
    SetPropertyValue(kBorderRightStyleProperty, border_right_style);
  }

  void set_border_bottom_style(
      const scoped_refptr<PropertyValue>& border_bottom_style) {
    SetPropertyValue(kBorderBottomStyleProperty, border_bottom_style);
  }

  void set_border_left_style(
      const scoped_refptr<PropertyValue>& border_left_style) {
    SetPropertyValue(kBorderLeftStyleProperty, border_left_style);
  }

  void set_border_top_width(
      const scoped_refptr<PropertyValue>& border_top_width) {
    SetPropertyValue(kBorderTopWidthProperty, border_top_width);
  }

  void set_border_right_width(
      const scoped_refptr<PropertyValue>& border_right_width) {
    SetPropertyValue(kBorderRightWidthProperty, border_right_width);
  }

  void set_border_bottom_width(
      const scoped_refptr<PropertyValue>& border_bottom_width) {
    SetPropertyValue(kBorderBottomWidthProperty, border_bottom_width);
  }

  void set_border_left_width(
      const scoped_refptr<PropertyValue>& border_left_width) {
    SetPropertyValue(kBorderLeftWidthProperty, border_left_width);
  }

  void set_border_top_left_radius(
      const scoped_refptr<PropertyValue>& border_top_left_radius) {
    SetPropertyValue(kBorderTopLeftRadiusProperty, border_top_left_radius);
  }

  void set_border_top_right_radius(
      const scoped_refptr<PropertyValue>& border_top_right_radius) {
    SetPropertyValue(kBorderTopRightRadiusProperty, border_top_right_radius);
  }

  void set_border_bottom_right_radius(
      const scoped_refptr<PropertyValue>& border_bottom_right_radius) {
    SetPropertyValue(kBorderBottomRightRadiusProperty,
                     border_bottom_right_radius);
  }

  void set_border_bottom_left_radius(
      const scoped_refptr<PropertyValue>& border_bottom_left_radius) {
    SetPropertyValue(kBorderBottomLeftRadiusProperty,
                     border_bottom_left_radius);
  }

  void set_bottom(const scoped_refptr<PropertyValue>& bottom) {
    SetPropertyValue(kBottomProperty, bottom);
  }

  void set_box_shadow(const scoped_refptr<PropertyValue>& box_shadow) {
    SetPropertyValue(kBoxShadowProperty, box_shadow);
  }

  void set_color(const scoped_refptr<PropertyValue>& color) {
    SetPropertyValue(kColorProperty, color);
  }

  void set_content(const scoped_refptr<PropertyValue>& content) {
    SetPropertyValue(kContentProperty, content);
  }

  void set_display(const scoped_refptr<PropertyValue>& display) {
    SetPropertyValue(kDisplayProperty, display);
  }

  void set_filter(const scoped_refptr<PropertyValue>& filter) {
    SetPropertyValue(kFilterProperty, filter);
  }

  void set_flex_basis(const scoped_refptr<PropertyValue>& flex_basis) {
    SetPropertyValue(kFlexBasisProperty, flex_basis);
  }

  void set_flex_direction(const scoped_refptr<PropertyValue>& flex_direction) {
    SetPropertyValue(kFlexDirectionProperty, flex_direction);
  }

  void set_flex_grow(const scoped_refptr<PropertyValue>& flex_grow) {
    SetPropertyValue(kFlexGrowProperty, flex_grow);
  }

  void set_flex_shrink(const scoped_refptr<PropertyValue>& flex_shrink) {
    SetPropertyValue(kFlexShrinkProperty, flex_shrink);
  }

  void set_flex_wrap(const scoped_refptr<PropertyValue>& flex_wrap) {
    SetPropertyValue(kFlexWrapProperty, flex_wrap);
  }

  void set_font_family(const scoped_refptr<PropertyValue>& font_family) {
    SetPropertyValue(kFontFamilyProperty, font_family);
  }

  void set_font_size(const scoped_refptr<PropertyValue>& font_size) {
    SetPropertyValue(kFontSizeProperty, font_size);
  }

  void set_font_style(const scoped_refptr<PropertyValue>& font_style) {
    SetPropertyValue(kFontStyleProperty, font_style);
  }

  void set_font_weight(const scoped_refptr<PropertyValue>& font_weight) {
    SetPropertyValue(kFontWeightProperty, font_weight);
  }

  void set_height(const scoped_refptr<PropertyValue>& height) {
    SetPropertyValue(kHeightProperty, height);
  }

  void set_justify_content(
      const scoped_refptr<PropertyValue>& justify_content) {
    SetPropertyValue(kJustifyContentProperty, justify_content);
  }

  void set_left(const scoped_refptr<PropertyValue>& left) {
    SetPropertyValue(kLeftProperty, left);
  }

  void set_line_height(const scoped_refptr<PropertyValue>& line_height) {
    SetPropertyValue(kLineHeightProperty, line_height);
  }

  void set_margin_bottom(const scoped_refptr<PropertyValue>& margin_bottom) {
    SetPropertyValue(kMarginBottomProperty, margin_bottom);
  }

  void set_margin_left(const scoped_refptr<PropertyValue>& margin_left) {
    SetPropertyValue(kMarginLeftProperty, margin_left);
  }

  void set_margin_right(const scoped_refptr<PropertyValue>& margin_right) {
    SetPropertyValue(kMarginRightProperty, margin_right);
  }

  void set_margin_top(const scoped_refptr<PropertyValue>& margin_top) {
    SetPropertyValue(kMarginTopProperty, margin_top);
  }

  void set_max_height(const scoped_refptr<PropertyValue>& max_height) {
    SetPropertyValue(kMaxHeightProperty, max_height);
  }

  void set_max_width(const scoped_refptr<PropertyValue>& max_width) {
    SetPropertyValue(kMaxWidthProperty, max_width);
  }

  void set_min_height(const scoped_refptr<PropertyValue>& min_height) {
    SetPropertyValue(kMinHeightProperty, min_height);
  }

  void set_min_width(const scoped_refptr<PropertyValue>& min_width) {
    SetPropertyValue(kMinWidthProperty, min_width);
  }

  void set_opacity(const scoped_refptr<PropertyValue>& opacity) {
    SetPropertyValue(kOpacityProperty, opacity);
  }

  void set_order(const scoped_refptr<PropertyValue>& order) {
    SetPropertyValue(kOrderProperty, order);
  }

  void set_outline_color(const scoped_refptr<PropertyValue>& outline_color) {
    SetPropertyValue(kOutlineColorProperty, outline_color);
  }

  void set_outline_style(const scoped_refptr<PropertyValue>& outline_style) {
    SetPropertyValue(kOutlineStyleProperty, outline_style);
  }

  void set_outline_width(const scoped_refptr<PropertyValue>& outline_width) {
    SetPropertyValue(kOutlineWidthProperty, outline_width);
  }

  void set_overflow(const scoped_refptr<PropertyValue>& overflow) {
    SetPropertyValue(kOverflowProperty, overflow);
  }

  void set_overflow_wrap(const scoped_refptr<PropertyValue>& overflow_wrap) {
    SetPropertyValue(kOverflowWrapProperty, overflow_wrap);
  }

  void set_padding_bottom(const scoped_refptr<PropertyValue>& padding_bottom) {
    SetPropertyValue(kPaddingBottomProperty, padding_bottom);
  }

  void set_padding_left(const scoped_refptr<PropertyValue>& padding_left) {
    SetPropertyValue(kPaddingLeftProperty, padding_left);
  }

  void set_padding_right(const scoped_refptr<PropertyValue>& padding_right) {
    SetPropertyValue(kPaddingRightProperty, padding_right);
  }

  void set_padding_top(const scoped_refptr<PropertyValue>& padding_top) {
    SetPropertyValue(kPaddingTopProperty, padding_top);
  }

  void set_pointer_events(const scoped_refptr<PropertyValue>& pointer_events) {
    SetPropertyValue(kPointerEventsProperty, pointer_events);
  }

  void set_position(const scoped_refptr<PropertyValue>& position) {
    SetPropertyValue(kPositionProperty, position);
  }

  void set_right(const scoped_refptr<PropertyValue>& right) {
    SetPropertyValue(kRightProperty, right);
  }

  void set_text_align(const scoped_refptr<PropertyValue>& text_align) {
    SetPropertyValue(kTextAlignProperty, text_align);
  }

  void set_text_decoration_color(
      const scoped_refptr<PropertyValue>& text_decoration_color) {
    SetPropertyValue(kTextDecorationColorProperty, text_decoration_color);
  }

  void set_text_decoration_line(
      const scoped_refptr<PropertyValue>& text_decoration_line) {
    SetPropertyValue(kTextDecorationLineProperty, text_decoration_line);
  }

  void set_text_indent(const scoped_refptr<PropertyValue>& text_indent) {
    SetPropertyValue(kTextIndentProperty, text_indent);
  }

  void set_text_overflow(const scoped_refptr<PropertyValue>& text_overflow) {
    SetPropertyValue(kTextOverflowProperty, text_overflow);
  }

  void set_text_shadow(const scoped_refptr<PropertyValue>& text_shadow) {
    SetPropertyValue(kTextShadowProperty, text_shadow);
  }

  void set_text_transform(const scoped_refptr<PropertyValue>& text_transform) {
    SetPropertyValue(kTextTransformProperty, text_transform);
  }

  void set_top(const scoped_refptr<PropertyValue>& top) {
    SetPropertyValue(kTopProperty, top);
  }

  void set_transform(const scoped_refptr<PropertyValue>& transform) {
    SetPropertyValue(kTransformProperty, transform);
  }

  void set_transform_origin(
      const scoped_refptr<PropertyValue>& transform_origin) {
    SetPropertyValue(kTransformOriginProperty, transform_origin);
  }

  void set_transition_delay(
      const scoped_refptr<PropertyValue>& transition_delay) {
    SetPropertyValue(kTransitionDelayProperty, transition_delay);
  }

  void set_transition_duration(
      const scoped_refptr<PropertyValue>& transition_duration) {
    SetPropertyValue(kTransitionDurationProperty, transition_duration);
  }

  void set_transition_property(
      const scoped_refptr<PropertyValue>& transition_property) {
    SetPropertyValue(kTransitionPropertyProperty, transition_property);
  }

  void set_transition_timing_function(
      const scoped_refptr<PropertyValue>& transition_timing_function) {
    SetPropertyValue(kTransitionTimingFunctionProperty,
                     transition_timing_function);
  }

  void set_vertical_align(const scoped_refptr<PropertyValue>& vertical_align) {
    SetPropertyValue(kVerticalAlignProperty, vertical_align);
  }

  void set_visibility(const scoped_refptr<PropertyValue>& visibility) {
    SetPropertyValue(kVisibilityProperty, visibility);
  }

  void set_white_space(const scoped_refptr<PropertyValue>& white_space) {
    SetPropertyValue(kWhiteSpaceProperty, white_space);
  }

  void set_width(const scoped_refptr<PropertyValue>& width) {
    SetPropertyValue(kWidthProperty, width);
  }

  void set_z_index(const scoped_refptr<PropertyValue>& z_index) {
    SetPropertyValue(kZIndexProperty, z_index);
  }

  void SetPropertyValue(const PropertyKey key,
                        const scoped_refptr<PropertyValue>& value) {
    CSSComputedStyleData::SetPropertyValue(key, value);
  }

  void set_is_inline_before_blockification(bool value) {
    CSSComputedStyleData::set_is_inline_before_blockification(value);
  }
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_COMPUTED_STYLE_DATA_H_

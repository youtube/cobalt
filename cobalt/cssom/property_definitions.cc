/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/cssom/property_definitions.h"

#include "base/lazy_instance.h"
#include "base/string_util.h"
#include "cobalt/cssom/font_style_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/integer_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_key_list_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/time_list_value.h"
#include "cobalt/cssom/timing_function.h"
#include "cobalt/cssom/timing_function_list_value.h"
#include "cobalt/cssom/unicode_range_value.h"

namespace cobalt {
namespace cssom {

namespace {

struct PropertyDefinition {
  const char* name;
  bool inherited;
  bool animatable;
  scoped_refptr<PropertyValue> initial_value;
};

struct NonTrivialGlobalVariables {
  NonTrivialGlobalVariables();

  PropertyDefinition properties[kMaxEveryPropertyKey + 1];
  AnimatablePropertyList animatable_properties;

  void SetPropertyDefinition(
      PropertyKey key, const char* name, bool inherited, bool animatable,
      const scoped_refptr<PropertyValue>& initial_value) {
    PropertyDefinition& definition = properties[key];
    definition.name = name;
    definition.inherited = inherited;
    definition.animatable = animatable;
    definition.initial_value = initial_value;
  }

  // Computes a set of animatable property to allow fast iteration through
  // only animatable properties.
  void CompileSetOfAnimatableProperties();
};

scoped_refptr<TimeListValue> CreateTimeListWithZeroSeconds() {
  scoped_ptr<TimeListValue::Builder> time_list(new TimeListValue::Builder());
  time_list->push_back(base::TimeDelta());
  return make_scoped_refptr(new TimeListValue(time_list.Pass()));
}

scoped_refptr<PropertyKeyListValue> CreatePropertyKeyListWithAll() {
  scoped_ptr<PropertyKeyListValue::Builder> property_list(
      new PropertyKeyListValue::Builder());
  property_list->push_back(kAllProperty);
  return make_scoped_refptr(new PropertyKeyListValue(property_list.Pass()));
}

scoped_refptr<TimingFunctionListValue>
CreateTransitionTimingFunctionListWithEase() {
  scoped_ptr<TimingFunctionListValue::Builder> timing_function_list(
      new TimingFunctionListValue::Builder());
  timing_function_list->push_back(TimingFunction::GetEase());
  return make_scoped_refptr(
      new TimingFunctionListValue(timing_function_list.Pass()));
}

NonTrivialGlobalVariables::NonTrivialGlobalVariables() {
  // Inherited: no
  // Initial: transparent.
  //   http://www.w3.org/TR/css3-background/#the-background-color
  SetPropertyDefinition(kBackgroundColorProperty, "background-color", false,
                        true, new RGBAColorValue(0x00000000));

  // Inherited: no.
  // Initial: none.
  //   http://www.w3.org/TR/css3-background/#background-image
  SetPropertyDefinition(kBackgroundImageProperty, "background-image", false,
                        false, KeywordValue::GetNone());

  // Inherited: no.
  // Initial: 0% 0%..
  //   http://www.w3.org/TR/css3-background/#the-background-position
  scoped_ptr<PropertyListValue::Builder> background_position_builder(
      new PropertyListValue::Builder());
  background_position_builder->reserve(2);
  background_position_builder->push_back(new PercentageValue(0.0f));
  background_position_builder->push_back(new PercentageValue(0.0f));
  scoped_refptr<PropertyListValue> background_position_list(
      new PropertyListValue(background_position_builder.Pass()));
  SetPropertyDefinition(kBackgroundPositionProperty, "background-position",
                        false, false, background_position_list);

  // Inherited: no.
  // Initial: repeat.
  //   http://www.w3.org/TR/css3-background/#the-background-repeat
  // The first value is for the horizontal direction, and the second for the
  // vertical one. If only one 'repeat' is given, the second is assumed to be
  // 'repeat'.
  scoped_ptr<PropertyListValue::Builder> background_repeat_builder(
      new PropertyListValue::Builder());
  background_repeat_builder->reserve(2);
  background_repeat_builder->push_back(KeywordValue::GetRepeat());
  background_repeat_builder->push_back(KeywordValue::GetRepeat());
  scoped_refptr<PropertyListValue> background_repeat_list(
      new PropertyListValue(background_repeat_builder.Pass()));
  SetPropertyDefinition(kBackgroundRepeatProperty, "background-repeat", false,
                        false, background_repeat_list);

  // Inherited: no.
  // Initial: auto.
  //   http://www.w3.org/TR/css3-background/#background-size
  // The first value gives the width of the corresponding image, and the second
  // value gives its height. If only one value is given, the second is assumed
  // to be 'auto'.
  scoped_ptr<PropertyListValue::Builder> background_size_builder(
      new PropertyListValue::Builder());
  background_size_builder->reserve(2);
  background_size_builder->push_back(KeywordValue::GetAuto());
  background_size_builder->push_back(KeywordValue::GetAuto());
  scoped_refptr<PropertyListValue> background_size_list(
      new PropertyListValue(background_size_builder.Pass()));
  SetPropertyDefinition(kBackgroundSizeProperty, "background-size", false,
                        false, background_size_list);

  // Inherited: no.
  // Initial: see individual properties (0).
  //   http://www.w3.org/TR/css3-background/#the-border-radius
  // Cobalt only support a single length value that applies to all borders.
  SetPropertyDefinition(kBorderRadiusProperty, "border-radius", false, false,
                        new LengthValue(0, kPixelsUnit));

  // Inherited: no.
  // Initial: auto.
  //   http://www.w3.org/TR/CSS2/visuren.html#propdef-bottom
  SetPropertyDefinition(kBottomProperty, "bottom", false, false,
                        KeywordValue::GetAuto());

  // Inherited: yes.
  // Initial: depends on user agent.
  //   http://www.w3.org/TR/css3-color/#foreground
  // Opaque black in Chromium and Cobalt.
  SetPropertyDefinition(kColorProperty, "color", true, true,
                        new RGBAColorValue(0x000000ff));

  // Inherited: no.
  // Initial: normal.
  //   http://www.w3.org/TR/CSS21/generate.html#content
  SetPropertyDefinition(kContentProperty, "content", false, false,
                        KeywordValue::GetNormal());

  // Inherited: no.
  // Initial: inline.
  //   http://www.w3.org/TR/CSS21/visuren.html#display-prop
  SetPropertyDefinition(kDisplayProperty, "display", false, false,
                        KeywordValue::GetInline());

  // Inherited: yes.
  // Initial: depends on user agent.
  //   http://www.w3.org/TR/css3-fonts/#font-family-prop
  // Varies by platform in Chromium, Roboto in Cobalt.
  scoped_ptr<PropertyListValue::Builder> font_family_builder(
      new PropertyListValue::Builder());
  font_family_builder->push_back(new StringValue("Roboto"));
  scoped_refptr<PropertyListValue> font_family_list(
      new PropertyListValue(font_family_builder.Pass()));
  SetPropertyDefinition(kFontFamilyProperty, "font-family", true, false,
                        font_family_list);

  // Inherited: yes.
  // Initial: medium.
  //   http://www.w3.org/TR/css3-fonts/#font-size-prop
  // "medium" translates to 16px in Chromium.
  // Cobalt does not support keyword sizes, so we simply hardcode 16px.
  SetPropertyDefinition(kFontSizeProperty, "font-size", true, false,
                        new LengthValue(16, kPixelsUnit));

  // Inherited: yes.
  // Initial: normal.
  //   http://www.w3.org/TR/css3-fonts/#font-style-prop
  SetPropertyDefinition(kFontStyleProperty, "font-style", true, false,
                        FontStyleValue::GetNormal());

  // Inherited: yes.
  // Initial: normal.
  //   http://www.w3.org/TR/css3-fonts/#font-weight-prop
  SetPropertyDefinition(kFontWeightProperty, "font-weight", true, false,
                        FontWeightValue::GetNormalAka400());

  // Inherited: no.
  // Initial: auto.
  //   http://www.w3.org/TR/CSS21/visudet.html#the-height-property
  SetPropertyDefinition(kHeightProperty, "height", false, false,
                        KeywordValue::GetAuto());

  // Inherited: no.
  // Initial: auto.
  //   http://www.w3.org/TR/CSS2/visuren.html#propdef-left
  SetPropertyDefinition(kLeftProperty, "left", false, false,
                        KeywordValue::GetAuto());

  // Inherited: yes.
  // Initial: normal.
  //   http://www.w3.org/TR/CSS21/visudet.html#line-height
  SetPropertyDefinition(kLineHeightProperty, "line-height", true, false,
                        KeywordValue::GetNormal());

  // Inherited: no.
  // Initial: 0.
  //   http://www.w3.org/TR/CSS21/box.html#margin-properties
  SetPropertyDefinition(kMarginBottomProperty, "margin-bottom", false, false,
                        new LengthValue(0, kPixelsUnit));

  // Inherited: no.
  // Initial: 0.
  //   http://www.w3.org/TR/CSS21/box.html#margin-properties
  SetPropertyDefinition(kMarginLeftProperty, "margin-left", false, false,
                        new LengthValue(0, kPixelsUnit));

  // Inherited: no.
  // Initial: 0.
  //   http://www.w3.org/TR/CSS21/box.html#margin-properties
  SetPropertyDefinition(kMarginRightProperty, "margin-right", false, false,
                        new LengthValue(0, kPixelsUnit));

  // Inherited: no.
  // Initial: 0.
  //   http://www.w3.org/TR/CSS21/box.html#margin-properties
  SetPropertyDefinition(kMarginTopProperty, "margin-top", false, false,
                        new LengthValue(0, kPixelsUnit));

  // Inherited: no.
  // Initial: none.
  //   http://www.w3.org/TR/CSS2/visudet.html#propdef-max-height
  SetPropertyDefinition(kMaxHeightProperty, "max-height", false, false,
                        KeywordValue::GetNone());

  // Inherited: no.
  // Initial: none.
  //   http://www.w3.org/TR/CSS2/visudet.html#propdef-max-width
  SetPropertyDefinition(kMaxWidthProperty, "max-width", false, false,
                        KeywordValue::GetNone());

  // Inherited: no.
  // Initial: 0.
  //   http://www.w3.org/TR/CSS2/visudet.html#propdef-min-height
  SetPropertyDefinition(kMinHeightProperty, "min-height", false, false,
                        new LengthValue(0, kPixelsUnit));

  // Inherited: no.
  // Initial: 0.
  //   http://www.w3.org/TR/CSS2/visudet.html#propdef-min-width
  SetPropertyDefinition(kMinWidthProperty, "min-width", false, false,
                        new LengthValue(0, kPixelsUnit));

  // Inherited: no.
  // Initial: 1.
  //   http://www.w3.org/TR/css3-color/#opacity
  SetPropertyDefinition(kOpacityProperty, "opacity", false, true,
                        new NumberValue(1.0f));

  // Inherited: no.
  // Initial: visible.
  //   http://www.w3.org/TR/css-overflow-3/#overflow-properties
  SetPropertyDefinition(kOverflowProperty, "overflow", false, false,
                        KeywordValue::GetVisible());

  // Inherited: yes.
  // Initial: normal.
  //   http://www.w3.org/TR/css-text-3/#overflow-wrap
  SetPropertyDefinition(kOverflowWrapProperty, "overflow-wrap", true, false,
                        KeywordValue::GetNormal());

  // Inherited: no.
  // Initial: 0.
  //   http://www.w3.org/TR/CSS21/box.html#padding-properties
  SetPropertyDefinition(kPaddingBottomProperty, "padding-bottom", false, false,
                        new LengthValue(0, kPixelsUnit));

  // Inherited: no.
  // Initial: 0.
  //   http://www.w3.org/TR/CSS21/box.html#padding-properties
  SetPropertyDefinition(kPaddingLeftProperty, "padding-left", false, false,
                        new LengthValue(0, kPixelsUnit));

  // Inherited: no.
  // Initial: 0.
  //   http://www.w3.org/TR/CSS21/box.html#padding-properties
  SetPropertyDefinition(kPaddingRightProperty, "padding-right", false, false,
                        new LengthValue(0, kPixelsUnit));

  // Inherited: no.
  // Initial: 0.
  //   http://www.w3.org/TR/CSS21/box.html#padding-properties
  SetPropertyDefinition(kPaddingTopProperty, "padding-top", false, false,
                        new LengthValue(0, kPixelsUnit));

  // Inherited: no.
  // Initial: static.
  //   http://www.w3.org/TR/css3-positioning/#position-property
  SetPropertyDefinition(kPositionProperty, "position", false, false,
                        KeywordValue::GetStatic());

  // Inherited: no.
  // Initial: auto.
  //   http://www.w3.org/TR/CSS2/visuren.html#propdef-right
  SetPropertyDefinition(kRightProperty, "right", false, false,
                        KeywordValue::GetAuto());

  // Inherited: yes.
  // Initial: 8.
  //   http://www.w3.org/TR/css-text-3/#tab-size
  SetPropertyDefinition(kTabSizeProperty, "tab-size", true, false,
                        new IntegerValue(8));

  // Inherited: yes.
  // Initial: a nameless value that acts as 'left' if 'direction' is 'ltr',
  // 'right' if 'direction' is 'rtl'.
  //   http://www.w3.org/TR/CSS21/text.html#propdef-text-align
  // TODO(***REMOVED***): Currently this initial value is fixed to be 'left'. Properly
  // set this according to direction.
  SetPropertyDefinition(kTextAlignProperty, "text-align", true, false,
                        KeywordValue::GetLeft());

  // Inherited: yes.
  // Initial: 0.
  //   http://www.w3.org/TR/css3-text/#text-indent-property
  SetPropertyDefinition(kTextIndentProperty, "text-indent", true, false,
                        new LengthValue(0, kPixelsUnit));

  // Inherited: no.
  // Initial: clip.
  //   http://www.w3.org/TR/css3-ui/#propdef-text-overflow
  SetPropertyDefinition(kTextOverflowProperty, "text-overflow", false, false,
                        KeywordValue::GetClip());

  // Inherited: yes.
  // Initial: none.
  //   http://www.w3.org/TR/css3-text/#text-transform-property
  SetPropertyDefinition(kTextTransformProperty, "text-transform", true, false,
                        KeywordValue::GetNone());

  // Inherited: no.
  // Initial: auto.
  //   http://www.w3.org/TR/CSS2/visuren.html#propdef-top
  SetPropertyDefinition(kTopProperty, "top", false, false,
                        KeywordValue::GetAuto());

  // Inherited: no.
  // Initial: none.
  //   http://www.w3.org/TR/css3-transforms/#transform-property
  SetPropertyDefinition(kTransformProperty, "transform", false, true,
                        KeywordValue::GetNone());

  // Inherited: no.
  // Initial: 0s.
  //   http://www.w3.org/TR/css3-transitions/#transition-delay-property
  SetPropertyDefinition(kTransitionDelayProperty, "transition-delay", false,
                        false, CreateTimeListWithZeroSeconds());

  // Inherited: no.
  // Initial: 0s.
  //   http://www.w3.org/TR/css3-transitions/#transition-duration-property
  SetPropertyDefinition(kTransitionDurationProperty, "transition-duration",
                        false, false, CreateTimeListWithZeroSeconds());

  // Inherited: no.
  // Initial: all.
  //   http://www.w3.org/TR/css3-transitions/#transition-property-property
  SetPropertyDefinition(kTransitionPropertyProperty, "transition-property",
                        false, false, CreatePropertyKeyListWithAll());

  // Inherited: no.
  // Initial: ease.
  //   http://www.w3.org/TR/css3-transitions/#transition-timing-function-property
  SetPropertyDefinition(kTransitionTimingFunctionProperty,
                        "transition-timing-function", false, false,
                        CreateTransitionTimingFunctionListWithEase());

  // Inherited: no.
  // Initial: baseline.
  //   http://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
  SetPropertyDefinition(kVerticalAlignProperty, "vertical-align", false, false,
                        KeywordValue::GetBaseline());

  // Inherited: yes.
  // Initial: visible.
  //   http://www.w3.org/TR/CSS21/visufx.html#propdef-visibility
  SetPropertyDefinition(kVisibilityProperty, "visibility", true, false,
                        KeywordValue::GetVisible());

  // Inherited: yes.
  // Initial: normal.
  //   http://www.w3.org/TR/css3-text/#white-space-property
  SetPropertyDefinition(kWhiteSpaceProperty, "white-space", true, false,
                        KeywordValue::GetNormal());

  // Inherited: no.
  // Initial: auto.
  //   http://www.w3.org/TR/CSS21/visudet.html#the-width-property
  SetPropertyDefinition(kWidthProperty, "width", false, false,
                        KeywordValue::GetAuto());

  // Inherited: no.
  // Initial: auto.
  //   http://www.w3.org/TR/CSS21/visuren.html#z-index
  SetPropertyDefinition(kZIndexProperty, "z-index", false, false,
                        KeywordValue::GetAuto());

  // Inherited: no.
  // Initial: see individual properties.
  //   http://www.w3.org/TR/2013/WD-css3-transitions-20131119/#transition-property-property
  // This property name can appear as a keyword for the transition-property
  // property.
  SetPropertyDefinition(kAllProperty, "all", false, false, NULL);

  // Inherited: no.
  // Initial: see individual properties.
  //   http://www.w3.org/TR/css3-background/#the-background
  // This is a shorthand property.
  SetPropertyDefinition(kBackgroundProperty, "background", false, false, NULL);

  // Inherited: no.
  // Initial: see individual properties.
  //   http://www.w3.org/TR/CSS21/box.html#propdef-margin
  // This is a shorthand property.
  SetPropertyDefinition(kMarginProperty, "margin", false, false, NULL);

  // Inherited: no.
  // Initial: see individual properties.
  //   http://www.w3.org/TR/CSS21/box.html#propdef-padding
  // This is a shorthand property.
  SetPropertyDefinition(kPaddingProperty, "padding", false, false, NULL);

  // Inherited: N/A.
  // Initial: N/A.
  //   http://www.w3.org/TR/css3-fonts/#descdef-src
  // This is a descriptor for @font-face at-rules.
  SetPropertyDefinition(kSrcProperty, "src", false, false, NULL);

  // Inherited: no.
  // Initial: see individual properties.
  //   http://www.w3.org/TR/css3-transitions/#transition-shorthand-property
  // This is a shorthand property.
  SetPropertyDefinition(kTransitionProperty, "transition", false, false, NULL);

  // Inherited: N/A.
  // Initial: U+0-10FFFF.
  //   http://www.w3.org/TR/css3-fonts/#unicode-range-desc
  // This is a descriptor for @font-face at-rules.
  SetPropertyDefinition(kUnicodeRangeProperty, "unicode-range", false, false,
                        new UnicodeRangeValue(0, 0x10FFFF));

  // Inherited: yes.
  // Initial: normal.
  //   http://www.w3.org/TR/css-text-3/#overflow-wrap
  // This is an alias for kOverflowWrap
  SetPropertyDefinition(kWordWrapProperty, "word-wrap", true, false,
                        KeywordValue::GetNormal());

  CompileSetOfAnimatableProperties();
}

void NonTrivialGlobalVariables::CompileSetOfAnimatableProperties() {
  for (int i = 0; i < kMaxEveryPropertyKey + 1; ++i) {
    if (properties[i].animatable) {
      animatable_properties.push_back(static_cast<PropertyKey>(i));
    }
  }
}

base::LazyInstance<NonTrivialGlobalVariables> non_trivial_global_variables =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

const char* GetPropertyName(PropertyKey key) {
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxEveryPropertyKey);
  const char* name = non_trivial_global_variables.Get().properties[key].name;
  return name;
}

const scoped_refptr<PropertyValue>& GetPropertyInitialValue(PropertyKey key) {
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxEveryPropertyKey);
  const scoped_refptr<PropertyValue>& initial_value =
      non_trivial_global_variables.Get().properties[key].initial_value;
  return initial_value;
}

bool GetPropertyInheritance(PropertyKey key) {
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxEveryPropertyKey);
  return non_trivial_global_variables.Get().properties[key].inherited;
}

bool GetPropertyAnimatable(PropertyKey key) {
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxEveryPropertyKey);
  return non_trivial_global_variables.Get().properties[key].animatable;
}

const AnimatablePropertyList& GetAnimatableProperties() {
  return non_trivial_global_variables.Get().animatable_properties;
}

PropertyKey GetLonghandPropertyKey(const std::string& property_name) {
  switch (property_name.size()) {
    case 3:
      if (LowerCaseEqualsASCII(property_name, GetPropertyName(kTopProperty))) {
        return kTopProperty;
      }
      return kNoneProperty;

    case 4:
      if (LowerCaseEqualsASCII(property_name, GetPropertyName(kLeftProperty))) {
        return kLeftProperty;
      }
      return kNoneProperty;

    case 5:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kColorProperty))) {
        return kColorProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kRightProperty))) {
        return kRightProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kWidthProperty))) {
        return kWidthProperty;
      }
      return kNoneProperty;

    case 6:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kBottomProperty))) {
        return kBottomProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kHeightProperty))) {
        return kHeightProperty;
      }
      return kNoneProperty;

    case 7:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kContentProperty))) {
        return kContentProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kDisplayProperty))) {
        return kDisplayProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kOpacityProperty))) {
        return kOpacityProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kZIndexProperty))) {
        return kZIndexProperty;
      }
      return kNoneProperty;

    case 8:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kOverflowProperty))) {
        return kOverflowProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kPositionProperty))) {
        return kPositionProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kTabSizeProperty))) {
        return kTabSizeProperty;
      }
      return kNoneProperty;

    case 9:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kFontSizeProperty))) {
        return kFontSizeProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kMaxWidthProperty))) {
        return kMaxWidthProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kMinWidthProperty))) {
        return kMinWidthProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kTransformProperty))) {
        return kTransformProperty;
      }
      return kNoneProperty;

    case 10:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kFontStyleProperty))) {
        return kFontStyleProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kMarginTopProperty))) {
        return kMarginTopProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kMaxHeightProperty))) {
        return kMaxHeightProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kMinHeightProperty))) {
        return kMinHeightProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kTextAlignProperty))) {
        return kTextAlignProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kVisibilityProperty))) {
        return kVisibilityProperty;
      }
      return kNoneProperty;

    case 11:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kFontFamilyProperty))) {
        return kFontFamilyProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kFontWeightProperty))) {
        return kFontWeightProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kLineHeightProperty))) {
        return kLineHeightProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kMarginLeftProperty))) {
        return kMarginLeftProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kPaddingTopProperty))) {
        return kPaddingTopProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kTextIndentProperty))) {
        return kTextIndentProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kWhiteSpaceProperty))) {
        return kWhiteSpaceProperty;
      }
      return kNoneProperty;

    case 12:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kMarginRightProperty))) {
        return kMarginRightProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kPaddingLeftProperty))) {
        return kPaddingLeftProperty;
      }
      return kNoneProperty;

    case 13:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kBorderRadiusProperty))) {
        return kBorderRadiusProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kMarginBottomProperty))) {
        return kMarginBottomProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kOverflowWrapProperty))) {
        return kOverflowWrapProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kPaddingRightProperty))) {
        return kPaddingRightProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kTextOverflowProperty))) {
        return kTextOverflowProperty;
      }
      return kNoneProperty;

    case 14:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kPaddingBottomProperty))) {
        return kPaddingBottomProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kTextTransformProperty))) {
        return kTextTransformProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kVerticalAlignProperty))) {
        return kVerticalAlignProperty;
      }
      return kNoneProperty;

    case 15:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kBackgroundSizeProperty))) {
        return kBackgroundSizeProperty;
      }
      return kNoneProperty;

    case 16:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kBackgroundColorProperty))) {
        return kBackgroundColorProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kBackgroundImageProperty))) {
        return kBackgroundImageProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kTransitionDelayProperty))) {
        return kTransitionDelayProperty;
      }
      return kNoneProperty;

    case 17:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kBackgroundRepeatProperty))) {
        return kBackgroundRepeatProperty;
      }
      return kNoneProperty;

    case 19:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kBackgroundPositionProperty))) {
        return kBackgroundPositionProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kTransitionDurationProperty))) {
        return kTransitionDurationProperty;
      }
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kTransitionPropertyProperty))) {
        return kTransitionPropertyProperty;
      }
      return kNoneProperty;

    case 26:
      if (LowerCaseEqualsASCII(
              property_name,
              GetPropertyName(kTransitionTimingFunctionProperty))) {
        return kTransitionTimingFunctionProperty;
      }
      return kNoneProperty;

    default:
      return kNoneProperty;
  }
}

}  // namespace cssom
}  // namespace cobalt

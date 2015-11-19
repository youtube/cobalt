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
  PropertyDefinition()
      : name(NULL), inherited(kInheritedNo), animatable(kAnimatableNo) {}

  const char* name;
  Inherited inherited;
  Animatable animatable;
  scoped_refptr<PropertyValue> initial_value;
};

struct NonTrivialGlobalVariables {
  NonTrivialGlobalVariables();

  PropertyDefinition properties[kMaxEveryPropertyKey + 1];
  AnimatablePropertyList animatable_properties;

  void SetPropertyDefinition(
      PropertyKey key, const char* name, Inherited inherited,
      Animatable animatable,
      const scoped_refptr<PropertyValue>& initial_value) {
    PropertyDefinition& definition = properties[key];
    DCHECK(!definition.name) << "Properties can only be defined once.";
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

// Returns a PropertyListValue with only the single specified value.
scoped_refptr<PropertyListValue> CreateSinglePropertyListWithValue(
    const scoped_refptr<PropertyValue>& value) {
  return new PropertyListValue(
      make_scoped_ptr(new PropertyListValue::Builder(1, value)));
}

NonTrivialGlobalVariables::NonTrivialGlobalVariables() {
  // Start by setting all property names to NULL, so we can check that they all
  // get filled in.
  for (int i = 0; i < kMaxEveryPropertyKey; ++i) {
    properties[i].name = NULL;
  }

  // http://www.w3.org/TR/css3-animations/#animation-delay-property
  SetPropertyDefinition(kAnimationDelayProperty, "animation-delay",
                        kInheritedNo, kAnimatableNo,
                        CreateTimeListWithZeroSeconds());

  // http://www.w3.org/TR/css3-animations/#animation-duration-property
  SetPropertyDefinition(kAnimationDurationProperty, "animation-duration",
                        kInheritedNo, kAnimatableNo,
                        CreateTimeListWithZeroSeconds());

  // http://www.w3.org/TR/css3-animations/#animation-fill-mode-property
  SetPropertyDefinition(
      kAnimationFillModeProperty, "animation-fill-mode", kInheritedNo,
      kAnimatableNo,
      CreateSinglePropertyListWithValue(KeywordValue::GetNone()));

  // http://www.w3.org/TR/css3-animations/#animation-iteration-count-property
  SetPropertyDefinition(
      kAnimationIterationCountProperty, "animation-iteration-count",
      kInheritedNo, kAnimatableNo,
      CreateSinglePropertyListWithValue(new NumberValue(1.0f)));

  // http://www.w3.org/TR/css3-animations/#animation-name-property
  SetPropertyDefinition(
      kAnimationNameProperty, "animation-name", kInheritedNo, kAnimatableNo,
      CreateSinglePropertyListWithValue(KeywordValue::GetNone()));

  // http://www.w3.org/TR/css3-animations/#animation-timing-function-property
  SetPropertyDefinition(kAnimationTimingFunctionProperty,
                        "animation-timing-function", kInheritedNo,
                        kAnimatableNo,
                        CreateTransitionTimingFunctionListWithEase());

  // http://www.w3.org/TR/css3-background/#the-background-color
  SetPropertyDefinition(kBackgroundColorProperty, "background-color",
                        kInheritedNo, kAnimatableYes,
                        new RGBAColorValue(0x00000000));

  // http://www.w3.org/TR/css3-background/#background-image
  SetPropertyDefinition(
      kBackgroundImageProperty, "background-image", kInheritedNo, kAnimatableNo,
      CreateSinglePropertyListWithValue(KeywordValue::GetNone()));

  // http://www.w3.org/TR/css3-background/#the-background-position
  scoped_ptr<PropertyListValue::Builder> background_position_builder(
      new PropertyListValue::Builder());
  background_position_builder->reserve(2);
  background_position_builder->push_back(new PercentageValue(0.0f));
  background_position_builder->push_back(new PercentageValue(0.0f));
  scoped_refptr<PropertyListValue> background_position_list(
      new PropertyListValue(background_position_builder.Pass()));
  SetPropertyDefinition(kBackgroundPositionProperty, "background-position",
                        kInheritedNo, kAnimatableNo, background_position_list);

  // The first value is for the horizontal direction, and the second for the
  // vertical one. If only one 'repeat' is given, the second is assumed to be
  // 'repeat'.
  //   http://www.w3.org/TR/css3-background/#the-background-repeat
  scoped_ptr<PropertyListValue::Builder> background_repeat_builder(
      new PropertyListValue::Builder());
  background_repeat_builder->reserve(2);
  background_repeat_builder->push_back(KeywordValue::GetRepeat());
  background_repeat_builder->push_back(KeywordValue::GetRepeat());
  scoped_refptr<PropertyListValue> background_repeat_list(
      new PropertyListValue(background_repeat_builder.Pass()));
  SetPropertyDefinition(kBackgroundRepeatProperty, "background-repeat",
                        kInheritedNo, kAnimatableNo, background_repeat_list);

  // The first value gives the width of the corresponding image, and the second
  // value gives its height. If only one value is given, the second is assumed
  // to be 'auto'.
  //   http://www.w3.org/TR/css3-background/#background-size
  scoped_ptr<PropertyListValue::Builder> background_size_builder(
      new PropertyListValue::Builder());
  background_size_builder->reserve(2);
  background_size_builder->push_back(KeywordValue::GetAuto());
  background_size_builder->push_back(KeywordValue::GetAuto());
  scoped_refptr<PropertyListValue> background_size_list(
      new PropertyListValue(background_size_builder.Pass()));
  SetPropertyDefinition(kBackgroundSizeProperty, "background-size",
                        kInheritedNo, kAnimatableNo, background_size_list);

  // Cobalt only support a single length value that applies to all borders.
  //   http://www.w3.org/TR/css3-background/#the-border-radius
  SetPropertyDefinition(kBorderRadiusProperty, "border-radius", kInheritedNo,
                        kAnimatableNo, new LengthValue(0, kPixelsUnit));

  // http://www.w3.org/TR/CSS2/visuren.html#propdef-bottom
  SetPropertyDefinition(kBottomProperty, "bottom", kInheritedNo, kAnimatableNo,
                        KeywordValue::GetAuto());

  // Opaque black in Chromium and Cobalt.
  //   http://www.w3.org/TR/css3-color/#foreground
  SetPropertyDefinition(kColorProperty, "color", kInheritedYes, kAnimatableYes,
                        new RGBAColorValue(0x000000ff));

  // http://www.w3.org/TR/CSS21/generate.html#content
  SetPropertyDefinition(kContentProperty, "content", kInheritedNo,
                        kAnimatableNo, KeywordValue::GetNormal());

  // http://www.w3.org/TR/CSS21/visuren.html#display-prop
  SetPropertyDefinition(kDisplayProperty, "display", kInheritedNo,
                        kAnimatableNo, KeywordValue::GetInline());

  // Varies by platform in Chromium, Roboto in Cobalt.
  //   http://www.w3.org/TR/css3-fonts/#font-family-prop
  SetPropertyDefinition(
      kFontFamilyProperty, "font-family", kInheritedYes, kAnimatableNo,
      CreateSinglePropertyListWithValue(new StringValue("Roboto")));

  // "medium" translates to 16px in Chromium.
  // Cobalt does not support keyword sizes, so we simply hardcode 16px.
  //   http://www.w3.org/TR/css3-fonts/#font-size-prop
  SetPropertyDefinition(kFontSizeProperty, "font-size", kInheritedYes,
                        kAnimatableNo, new LengthValue(16, kPixelsUnit));

  // http://www.w3.org/TR/css3-fonts/#font-style-prop
  SetPropertyDefinition(kFontStyleProperty, "font-style", kInheritedYes,
                        kAnimatableNo, FontStyleValue::GetNormal());

  // http://www.w3.org/TR/css3-fonts/#font-weight-prop
  SetPropertyDefinition(kFontWeightProperty, "font-weight", kInheritedYes,
                        kAnimatableNo, FontWeightValue::GetNormalAka400());

  // http://www.w3.org/TR/CSS21/visudet.html#the-height-property
  SetPropertyDefinition(kHeightProperty, "height", kInheritedNo, kAnimatableNo,
                        KeywordValue::GetAuto());

  // http://www.w3.org/TR/CSS2/visuren.html#propdef-left
  SetPropertyDefinition(kLeftProperty, "left", kInheritedNo, kAnimatableNo,
                        KeywordValue::GetAuto());

  // http://www.w3.org/TR/CSS21/visudet.html#line-height
  SetPropertyDefinition(kLineHeightProperty, "line-height", kInheritedYes,
                        kAnimatableNo, KeywordValue::GetNormal());

  // http://www.w3.org/TR/CSS21/box.html#margin-properties
  SetPropertyDefinition(kMarginBottomProperty, "margin-bottom", kInheritedNo,
                        kAnimatableNo, new LengthValue(0, kPixelsUnit));

  // http://www.w3.org/TR/CSS21/box.html#margin-properties
  SetPropertyDefinition(kMarginLeftProperty, "margin-left", kInheritedNo,
                        kAnimatableNo, new LengthValue(0, kPixelsUnit));

  // http://www.w3.org/TR/CSS21/box.html#margin-properties
  SetPropertyDefinition(kMarginRightProperty, "margin-right", kInheritedNo,
                        kAnimatableNo, new LengthValue(0, kPixelsUnit));

  // http://www.w3.org/TR/CSS21/box.html#margin-properties
  SetPropertyDefinition(kMarginTopProperty, "margin-top", kInheritedNo,
                        kAnimatableNo, new LengthValue(0, kPixelsUnit));

  // http://www.w3.org/TR/CSS2/visudet.html#propdef-max-height
  SetPropertyDefinition(kMaxHeightProperty, "max-height", kInheritedNo,
                        kAnimatableNo, KeywordValue::GetNone());

  // http://www.w3.org/TR/CSS2/visudet.html#propdef-max-width
  SetPropertyDefinition(kMaxWidthProperty, "max-width", kInheritedNo,
                        kAnimatableNo, KeywordValue::GetNone());

  // http://www.w3.org/TR/CSS2/visudet.html#propdef-min-height
  SetPropertyDefinition(kMinHeightProperty, "min-height", kInheritedNo,
                        kAnimatableNo, new LengthValue(0, kPixelsUnit));

  // http://www.w3.org/TR/CSS2/visudet.html#propdef-min-width
  SetPropertyDefinition(kMinWidthProperty, "min-width", kInheritedNo,
                        kAnimatableNo, new LengthValue(0, kPixelsUnit));

  // http://www.w3.org/TR/css3-color/#opacity
  SetPropertyDefinition(kOpacityProperty, "opacity", kInheritedNo,
                        kAnimatableYes, new NumberValue(1.0f));

  // http://www.w3.org/TR/css-overflow-3/#overflow-properties
  SetPropertyDefinition(kOverflowProperty, "overflow", kInheritedNo,
                        kAnimatableNo, KeywordValue::GetVisible());

  // http://www.w3.org/TR/css-text-3/#overflow-wrap
  SetPropertyDefinition(kOverflowWrapProperty, "overflow-wrap", kInheritedYes,
                        kAnimatableNo, KeywordValue::GetNormal());

  // http://www.w3.org/TR/CSS21/box.html#padding-properties
  SetPropertyDefinition(kPaddingBottomProperty, "padding-bottom", kInheritedNo,
                        kAnimatableNo, new LengthValue(0, kPixelsUnit));

  // http://www.w3.org/TR/CSS21/box.html#padding-properties
  SetPropertyDefinition(kPaddingLeftProperty, "padding-left", kInheritedNo,
                        kAnimatableNo, new LengthValue(0, kPixelsUnit));

  // http://www.w3.org/TR/CSS21/box.html#padding-properties
  SetPropertyDefinition(kPaddingRightProperty, "padding-right", kInheritedNo,
                        kAnimatableNo, new LengthValue(0, kPixelsUnit));

  // http://www.w3.org/TR/CSS21/box.html#padding-properties
  SetPropertyDefinition(kPaddingTopProperty, "padding-top", kInheritedNo,
                        kAnimatableNo, new LengthValue(0, kPixelsUnit));

  // http://www.w3.org/TR/css3-positioning/#position-property
  SetPropertyDefinition(kPositionProperty, "position", kInheritedNo,
                        kAnimatableNo, KeywordValue::GetStatic());

  // http://www.w3.org/TR/CSS2/visuren.html#propdef-right
  SetPropertyDefinition(kRightProperty, "right", kInheritedNo, kAnimatableNo,
                        KeywordValue::GetAuto());

  // http://www.w3.org/TR/css-text-3/#tab-size
  SetPropertyDefinition(kTabSizeProperty, "tab-size", kInheritedYes,
                        kAnimatableNo, new IntegerValue(8));

  // TODO(***REMOVED***): Currently this initial value is fixed to be 'left'. Properly
  // set this according to direction.
  //   http://www.w3.org/TR/CSS21/text.html#propdef-text-align
  SetPropertyDefinition(kTextAlignProperty, "text-align", kInheritedYes,
                        kAnimatableNo, KeywordValue::GetLeft());

  // http://www.w3.org/TR/css3-text/#text-indent-property
  SetPropertyDefinition(kTextIndentProperty, "text-indent", kInheritedYes,
                        kAnimatableNo, new LengthValue(0, kPixelsUnit));

  // http://www.w3.org/TR/css3-ui/#propdef-text-overflow
  SetPropertyDefinition(kTextOverflowProperty, "text-overflow", kInheritedNo,
                        kAnimatableNo, KeywordValue::GetClip());

  // http://www.w3.org/TR/css3-text/#text-transform-property
  SetPropertyDefinition(kTextTransformProperty, "text-transform", kInheritedYes,
                        kAnimatableNo, KeywordValue::GetNone());

  // http://www.w3.org/TR/CSS2/visuren.html#propdef-top
  SetPropertyDefinition(kTopProperty, "top", kInheritedNo, kAnimatableNo,
                        KeywordValue::GetAuto());

  // http://www.w3.org/TR/css3-transforms/#transform-property
  SetPropertyDefinition(kTransformProperty, "transform", kInheritedNo,
                        kAnimatableYes, KeywordValue::GetNone());

  // http://www.w3.org/TR/css3-transitions/#transition-delay-property
  SetPropertyDefinition(kTransitionDelayProperty, "transition-delay",
                        kInheritedNo, kAnimatableNo,
                        CreateTimeListWithZeroSeconds());

  // http://www.w3.org/TR/css3-transitions/#transition-duration-property
  SetPropertyDefinition(kTransitionDurationProperty, "transition-duration",
                        kInheritedNo, kAnimatableNo,
                        CreateTimeListWithZeroSeconds());

  // http://www.w3.org/TR/css3-transitions/#transition-property-property
  SetPropertyDefinition(kTransitionPropertyProperty, "transition-property",
                        kInheritedNo, kAnimatableNo,
                        CreatePropertyKeyListWithAll());

  // http://www.w3.org/TR/css3-transitions/#transition-timing-function-property
  SetPropertyDefinition(kTransitionTimingFunctionProperty,
                        "transition-timing-function", kInheritedNo,
                        kAnimatableNo,
                        CreateTransitionTimingFunctionListWithEase());

  // http://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
  SetPropertyDefinition(kVerticalAlignProperty, "vertical-align", kInheritedNo,
                        kAnimatableNo, KeywordValue::GetBaseline());

  // http://www.w3.org/TR/CSS21/visufx.html#propdef-visibility
  SetPropertyDefinition(kVisibilityProperty, "visibility", kInheritedYes,
                        kAnimatableNo, KeywordValue::GetVisible());

  // http://www.w3.org/TR/css3-text/#white-space-property
  SetPropertyDefinition(kWhiteSpaceProperty, "white-space", kInheritedYes,
                        kAnimatableNo, KeywordValue::GetNormal());

  // http://www.w3.org/TR/CSS21/visudet.html#the-width-property
  SetPropertyDefinition(kWidthProperty, "width", kInheritedNo, kAnimatableNo,
                        KeywordValue::GetAuto());

  // http://www.w3.org/TR/CSS21/visuren.html#z-index
  SetPropertyDefinition(kZIndexProperty, "z-index", kInheritedNo, kAnimatableNo,
                        KeywordValue::GetAuto());

  // This property name can appear as a keyword for the transition-property
  // property.
  //   http://www.w3.org/TR/2013/WD-css3-transitions-20131119/#transition-property-property
  SetPropertyDefinition(kAllProperty, "all", kInheritedNo, kAnimatableNo, NULL);

  // This is a shorthand property.
  //   http://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-shorthand-property
  SetPropertyDefinition(kAnimationProperty, "animation", kInheritedNo,
                        kAnimatableNo, NULL);

  // This is a shorthand property.
  //   http://www.w3.org/TR/css3-background/#the-background
  SetPropertyDefinition(kBackgroundProperty, "background", kInheritedNo,
                        kAnimatableNo, NULL);

  // This is a shorthand property.
  //   http://www.w3.org/TR/CSS21/box.html#propdef-margin
  SetPropertyDefinition(kMarginProperty, "margin", kInheritedNo, kAnimatableNo,
                        NULL);

  // This is a shorthand property.
  //   http://www.w3.org/TR/CSS21/box.html#propdef-padding
  SetPropertyDefinition(kPaddingProperty, "padding", kInheritedNo,
                        kAnimatableNo, NULL);

  // This is a descriptor for @font-face at-rules.
  //   http://www.w3.org/TR/css3-fonts/#descdef-src
  SetPropertyDefinition(kSrcProperty, "src", kInheritedNo, kAnimatableNo, NULL);

  // This is a shorthand property.
  //   http://www.w3.org/TR/css3-transitions/#transition-shorthand-property
  SetPropertyDefinition(kTransitionProperty, "transition", kInheritedNo,
                        kAnimatableNo, NULL);

  // This is a descriptor for @font-face at-rules.
  //   http://www.w3.org/TR/css3-fonts/#unicode-range-desc
  SetPropertyDefinition(kUnicodeRangeProperty, "unicode-range", kInheritedNo,
                        kAnimatableNo, new UnicodeRangeValue(0, 0x10FFFF));

  // This is an alias for kOverflowWrap
  //   http://www.w3.org/TR/css-text-3/#overflow-wrap
  SetPropertyDefinition(kWordWrapProperty, "word-wrap", kInheritedYes,
                        kAnimatableNo, KeywordValue::GetNormal());

  CompileSetOfAnimatableProperties();
}

void NonTrivialGlobalVariables::CompileSetOfAnimatableProperties() {
  for (int i = 0; i < kMaxEveryPropertyKey + 1; ++i) {
    if (properties[i].animatable == kAnimatableYes) {
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

Inherited GetPropertyInheritance(PropertyKey key) {
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxEveryPropertyKey);
  return non_trivial_global_variables.Get().properties[key].inherited;
}

Animatable GetPropertyAnimatable(PropertyKey key) {
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
                               GetPropertyName(kAnimationNameProperty))) {
        return kAnimationNameProperty;
      }
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
                               GetPropertyName(kAnimationDelayProperty))) {
        return kAnimationDelayProperty;
      }
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

    case 18:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kAnimationDurationProperty))) {
        return kAnimationDurationProperty;
      }
      return kNoneProperty;

    case 19:
      if (LowerCaseEqualsASCII(property_name,
                               GetPropertyName(kAnimationFillModeProperty))) {
        return kAnimationFillModeProperty;
      }
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

    case 25:
      if (LowerCaseEqualsASCII(
              property_name,
              GetPropertyName(kAnimationIterationCountProperty))) {
        return kAnimationIterationCountProperty;
      }
      if (LowerCaseEqualsASCII(
              property_name,
              GetPropertyName(kAnimationTimingFunctionProperty))) {
        return kAnimationTimingFunctionProperty;
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

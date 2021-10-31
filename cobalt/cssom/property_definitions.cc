// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/property_definitions.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "cobalt/cssom/calc_value.h"
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
      : name(NULL),
        inherited(kInheritedNo),
        animatable(kAnimatableNo),
        impacts_child_computed_style(kImpactsChildComputedStyleNo),
        impacts_box_generation(kImpactsBoxGenerationNo),
        impacts_box_sizes(kImpactsBoxSizesNo),
        impacts_box_cross_references(kImpactsBoxCrossReferencesNo) {}

  const char* name;
  Inherited inherited;
  Animatable animatable;
  ImpactsChildComputedStyle impacts_child_computed_style;
  ImpactsBoxGeneration impacts_box_generation;
  ImpactsBoxSizes impacts_box_sizes;
  ImpactsBoxCrossReferences impacts_box_cross_references;
  scoped_refptr<PropertyValue> initial_value;
  LonghandPropertySet longhand_properties;
};

struct NonTrivialGlobalVariables {
  NonTrivialGlobalVariables();

  std::vector<PropertyKey> lexicographical_longhand_keys;
  PropertyDefinition properties[kMaxEveryPropertyKey + 1];
  PropertyKeyVector animatable_properties;
  PropertyKeyVector inherited_animatable_properties;

  void SetPropertyDefinition(
      PropertyKey key, const char* name, Inherited inherited,
      Animatable animatable,
      ImpactsChildComputedStyle impacts_child_computed_style,
      ImpactsBoxGeneration impacts_box_generation,
      ImpactsBoxSizes impacts_box_sizes,
      ImpactsBoxCrossReferences impacts_box_cross_references,
      const scoped_refptr<PropertyValue>& initial_value) {
    DCHECK_LT(kNoneProperty, key);
    PropertyDefinition& definition = properties[key];
    DCHECK(!definition.name) << "Properties can only be defined once.";
    definition.name = name;
    definition.inherited = inherited;
    definition.animatable = animatable;
    definition.impacts_child_computed_style = impacts_child_computed_style;
    definition.impacts_box_generation = impacts_box_generation;
    definition.impacts_box_sizes = impacts_box_sizes;
    definition.impacts_box_cross_references = impacts_box_cross_references;
    definition.initial_value = initial_value;
  }

  void SetShorthandPropertyDefinition(
      PropertyKey key, const char* name,
      const LonghandPropertySet& longhand_properties) {
    DCHECK_LE(kFirstShorthandPropertyKey, key);
    DCHECK_GE(kMaxShorthandPropertyKey, key);
    PropertyDefinition& definition = properties[key];
    DCHECK(!definition.name) << "Properties can only be defined once.";
    definition.name = name;
    definition.longhand_properties = longhand_properties;
  }

  // Computes a set of animatable property to allow fast iteration through
  // only animatable properties.
  void CompileSetOfAnimatableProperties();

  // Computes a lexicographically sorted list of longhand properties for
  // serialization of computed styles.
  void CompileSortedLonghandProperties();

  // Helper functor for sorting property definitions by name.
  struct NameLess {
    explicit NameLess(const NonTrivialGlobalVariables& variables)
        : non_trivial_global_variables(variables) {}
    bool operator()(const PropertyKey& a, const PropertyKey& b) {
      return strcmp(non_trivial_global_variables.properties[a].name,
                    non_trivial_global_variables.properties[b].name) < 0;
    }

    const NonTrivialGlobalVariables& non_trivial_global_variables;
  };
};

scoped_refptr<TimeListValue> CreateTimeListWithZeroSeconds() {
  std::unique_ptr<TimeListValue::Builder> time_list(
      new TimeListValue::Builder());
  time_list->push_back(base::TimeDelta());
  return base::WrapRefCounted(new TimeListValue(std::move(time_list)));
}

scoped_refptr<PropertyKeyListValue> CreatePropertyKeyListWithAll() {
  std::unique_ptr<PropertyKeyListValue::Builder> property_list(
      new PropertyKeyListValue::Builder());
  property_list->push_back(kAllProperty);
  return base::WrapRefCounted(
      new PropertyKeyListValue(std::move(property_list)));
}

scoped_refptr<TimingFunctionListValue>
CreateTransitionTimingFunctionListWithEase() {
  std::unique_ptr<TimingFunctionListValue::Builder> timing_function_list(
      new TimingFunctionListValue::Builder());
  timing_function_list->push_back(TimingFunction::GetEase());
  return base::WrapRefCounted(
      new TimingFunctionListValue(std::move(timing_function_list)));
}

// Returns a PropertyListValue with only the single specified value.
scoped_refptr<PropertyListValue> CreateSinglePropertyListWithValue(
    const scoped_refptr<PropertyValue>& value) {
  return new PropertyListValue(
      base::WrapUnique(new PropertyListValue::Builder(1, value)));
}

NonTrivialGlobalVariables::NonTrivialGlobalVariables() {
  // https://www.w3.org/TR/css-flexbox-1/#align-content-property
  SetPropertyDefinition(kAlignContentProperty, "align-content", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetStretch());

  // https://www.w3.org/TR/css-flexbox-1/#align-items-property
  SetPropertyDefinition(kAlignItemsProperty, "align-items", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetStretch());

  // https://www.w3.org/TR/css-flexbox-1/#propdef-align-self
  SetPropertyDefinition(kAlignSelfProperty, "align-self", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetAuto());

  // https://www.w3.org/TR/css3-animations/#animation-delay-property
  SetPropertyDefinition(
      kAnimationDelayProperty, "animation-delay", kInheritedNo, kAnimatableNo,
      kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
      kImpactsBoxCrossReferencesNo, CreateTimeListWithZeroSeconds());

  // https://www.w3.org/TR/css3-animations/#animation-direction-property
  SetPropertyDefinition(
      kAnimationDirectionProperty, "animation-direction", kInheritedNo,
      kAnimatableNo, kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
      kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
      CreateSinglePropertyListWithValue(KeywordValue::GetNormal()));

  // https://www.w3.org/TR/css3-animations/#animation-duration-property
  SetPropertyDefinition(kAnimationDurationProperty, "animation-duration",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        CreateTimeListWithZeroSeconds());

  // https://www.w3.org/TR/css3-animations/#animation-fill-mode-property
  SetPropertyDefinition(
      kAnimationFillModeProperty, "animation-fill-mode", kInheritedNo,
      kAnimatableNo, kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
      kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
      CreateSinglePropertyListWithValue(KeywordValue::GetNone()));

  // https://www.w3.org/TR/css3-animations/#animation-iteration-count-property
  SetPropertyDefinition(
      kAnimationIterationCountProperty, "animation-iteration-count",
      kInheritedNo, kAnimatableNo, kImpactsChildComputedStyleNo,
      kImpactsBoxGenerationNo, kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
      CreateSinglePropertyListWithValue(new NumberValue(1.0f)));

  // https://www.w3.org/TR/css3-animations/#animation-name-property
  SetPropertyDefinition(
      kAnimationNameProperty, "animation-name", kInheritedNo, kAnimatableNo,
      kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
      kImpactsBoxCrossReferencesNo,
      CreateSinglePropertyListWithValue(KeywordValue::GetNone()));

  // https://www.w3.org/TR/css3-animations/#animation-timing-function-property
  SetPropertyDefinition(
      kAnimationTimingFunctionProperty, "animation-timing-function",
      kInheritedNo, kAnimatableNo, kImpactsChildComputedStyleNo,
      kImpactsBoxGenerationNo, kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
      CreateTransitionTimingFunctionListWithEase());

  // https://www.w3.org/TR/css3-background/#the-background-color
  SetPropertyDefinition(kBackgroundColorProperty, "background-color",
                        kInheritedNo, kAnimatableYes,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        new RGBAColorValue(0x00000000));

  // https://www.w3.org/TR/css3-background/#background-image
  SetPropertyDefinition(
      kBackgroundImageProperty, "background-image", kInheritedNo, kAnimatableNo,
      kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
      kImpactsBoxCrossReferencesNo,
      CreateSinglePropertyListWithValue(KeywordValue::GetNone()));

  // https://www.w3.org/TR/css3-background/#the-background-position
  std::unique_ptr<PropertyListValue::Builder> background_position_builder(
      new PropertyListValue::Builder());
  background_position_builder->reserve(2);
  background_position_builder->push_back(
      new CalcValue(new PercentageValue(0.0f)));
  background_position_builder->push_back(
      new CalcValue(new PercentageValue(0.0f)));
  scoped_refptr<PropertyListValue> background_position_list(
      new PropertyListValue(std::move(background_position_builder)));
  SetPropertyDefinition(kBackgroundPositionProperty, "background-position",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        background_position_list);

  // The first value is for the horizontal direction, and the second for the
  // vertical one. If only one 'repeat' is given, the second is assumed to be
  // 'repeat'.
  //   https://www.w3.org/TR/css3-background/#the-background-repeat
  std::unique_ptr<PropertyListValue::Builder> background_repeat_builder(
      new PropertyListValue::Builder());
  background_repeat_builder->reserve(2);
  background_repeat_builder->push_back(KeywordValue::GetRepeat());
  background_repeat_builder->push_back(KeywordValue::GetRepeat());
  scoped_refptr<PropertyListValue> background_repeat_list(
      new PropertyListValue(std::move(background_repeat_builder)));
  SetPropertyDefinition(
      kBackgroundRepeatProperty, "background-repeat", kInheritedNo,
      kAnimatableNo, kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
      kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo, background_repeat_list);

  // The first value gives the width of the corresponding image, and the second
  // value gives its height. If only one value is given, the second is assumed
  // to be 'auto'.
  //   https://www.w3.org/TR/css-backgrounds-3/#the-background-size
  std::unique_ptr<PropertyListValue::Builder> background_size_builder(
      new PropertyListValue::Builder());
  background_size_builder->reserve(2);
  background_size_builder->push_back(KeywordValue::GetAuto());
  background_size_builder->push_back(KeywordValue::GetAuto());
  scoped_refptr<PropertyListValue> background_size_list(
      new PropertyListValue(std::move(background_size_builder)));
  SetPropertyDefinition(
      kBackgroundSizeProperty, "background-size", kInheritedNo, kAnimatableNo,
      kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
      kImpactsBoxCrossReferencesNo, background_size_list);

  // This sets the foreground color of the border specified by the border-style
  // property.
  //   https://www.w3.org/TR/css3-background/#border-color
  SetPropertyDefinition(
      kBorderTopColorProperty, "border-top-color", kInheritedNo, kAnimatableYes,
      kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
      kImpactsBoxCrossReferencesNo, KeywordValue::GetCurrentColor());

  SetPropertyDefinition(kBorderRightColorProperty, "border-right-color",
                        kInheritedNo, kAnimatableYes,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetCurrentColor());

  SetPropertyDefinition(kBorderBottomColorProperty, "border-bottom-color",
                        kInheritedNo, kAnimatableYes,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetCurrentColor());

  SetPropertyDefinition(kBorderLeftColorProperty, "border-left-color",
                        kInheritedNo, kAnimatableYes,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetCurrentColor());

  // https://www.w3.org/TR/css3-background/#border-style
  SetPropertyDefinition(kBorderTopStyleProperty, "border-top-style",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetNone());

  SetPropertyDefinition(kBorderRightStyleProperty, "border-right-style",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetNone());

  SetPropertyDefinition(kBorderBottomStyleProperty, "border-bottom-style",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetNone());

  SetPropertyDefinition(kBorderLeftStyleProperty, "border-left-style",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetNone());

  //  Initial: medium.
  // According to the spec., make the thickness depend on the 'medium' font
  // size: one choice might be 1, 3, & 5px (thin, medium, and thick) when the
  // 'medium' font size is 17 px or less.
  //   https://www.w3.org/TR/css3-background/#border-width
  SetPropertyDefinition(kBorderTopWidthProperty, "border-top-width",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        new LengthValue(3, kPixelsUnit));

  SetPropertyDefinition(kBorderRightWidthProperty, "border-right-width",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        new LengthValue(3, kPixelsUnit));

  SetPropertyDefinition(kBorderBottomWidthProperty, "border-bottom-width",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        new LengthValue(3, kPixelsUnit));

  SetPropertyDefinition(kBorderLeftWidthProperty, "border-left-width",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        new LengthValue(3, kPixelsUnit));

  //   https://www.w3.org/TR/css3-background/#the-border-radius
  SetPropertyDefinition(kBorderTopLeftRadiusProperty, "border-top-left-radius",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        new LengthValue(0, kPixelsUnit));

  SetPropertyDefinition(kBorderTopRightRadiusProperty,
                        "border-top-right-radius", kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        new LengthValue(0, kPixelsUnit));

  SetPropertyDefinition(
      kBorderBottomRightRadiusProperty, "border-bottom-right-radius",
      kInheritedNo, kAnimatableNo, kImpactsChildComputedStyleNo,
      kImpactsBoxGenerationNo, kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
      new LengthValue(0, kPixelsUnit));

  SetPropertyDefinition(
      kBorderBottomLeftRadiusProperty, "border-bottom-left-radius",
      kInheritedNo, kAnimatableNo, kImpactsChildComputedStyleNo,
      kImpactsBoxGenerationNo, kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
      new LengthValue(0, kPixelsUnit));

  // https://www.w3.org/TR/CSS2/visuren.html#propdef-bottom
  SetPropertyDefinition(kBottomProperty, "bottom", kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleYes, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetAuto());

  // https://www.w3.org/TR/css3-background/#the-box-shadow
  SetPropertyDefinition(kBoxShadowProperty, "box-shadow", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetNone());

  // Opaque black in Chromium and Cobalt.
  //   https://www.w3.org/TR/css3-color/#foreground
  SetPropertyDefinition(kColorProperty, "color", kInheritedYes, kAnimatableYes,
                        kImpactsChildComputedStyleYes, kImpactsBoxGenerationYes,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        new RGBAColorValue(0x000000ff));

  // https://www.w3.org/TR/CSS21/generate.html#content
  SetPropertyDefinition(kContentProperty, "content", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationYes, kImpactsBoxSizesNo,
                        kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetNormal());

  // https://www.w3.org/TR/CSS21/visuren.html#display-prop
  SetPropertyDefinition(kDisplayProperty, "display", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationYes, kImpactsBoxSizesNo,
                        kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetInline());

  // https://www.w3.org/TR/filter-effects-1/#FilterProperty
  SetPropertyDefinition(kFilterProperty, "filter", kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetNone());

  // https://www.w3.org/TR/css-flexbox-1/#flex-basis-property
  SetPropertyDefinition(kFlexBasisProperty, "flex-basis", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetAuto());

  // https://www.w3.org/TR/css-flexbox-1/#flex-direction-property
  SetPropertyDefinition(kFlexDirectionProperty, "flex-direction", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetRow());

  // https://www.w3.org/TR/css-flexbox-1/#flex-grow-property
  SetPropertyDefinition(kFlexGrowProperty, "flex-grow", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo, new NumberValue(0));

  // https://www.w3.org/TR/css-flexbox-1/#flex-shrink-property
  SetPropertyDefinition(kFlexShrinkProperty, "flex-shrink", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo, new NumberValue(1));

  // https://www.w3.org/TR/css-flexbox-1/#flex-wrap-property
  SetPropertyDefinition(kFlexWrapProperty, "flex-wrap", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetNowrap());

  // Varies by platform in Chromium, Roboto in Cobalt.
  //   https://www.w3.org/TR/css3-fonts/#font-family-prop
  SetPropertyDefinition(
      kFontFamilyProperty, "font-family", kInheritedYes, kAnimatableNo,
      kImpactsChildComputedStyleNo, kImpactsBoxGenerationYes,
      kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
      CreateSinglePropertyListWithValue(new StringValue("Roboto")));

  // "medium" translates to 16px in Chromium.
  // Cobalt does not support keyword sizes, so we simply hardcode 16px.
  //   https://www.w3.org/TR/css3-fonts/#font-size-prop
  SetPropertyDefinition(kFontSizeProperty, "font-size", kInheritedYes,
                        kAnimatableNo, kImpactsChildComputedStyleYes,
                        kImpactsBoxGenerationYes, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        new LengthValue(16, kPixelsUnit));

  // https://www.w3.org/TR/css3-fonts/#font-style-prop
  SetPropertyDefinition(kFontStyleProperty, "font-style", kInheritedYes,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationYes, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        FontStyleValue::GetNormal());

  // https://www.w3.org/TR/css3-fonts/#font-weight-prop
  SetPropertyDefinition(kFontWeightProperty, "font-weight", kInheritedYes,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationYes, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        FontWeightValue::GetNormalAka400());

  // https://www.w3.org/TR/CSS21/visudet.html#the-height-property
  SetPropertyDefinition(kHeightProperty, "height", kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleYes, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetAuto());

  // https://www.w3.org/TR/intersection-observer/#parse-a-root-margin
  // Not actually a new CSS property but we need it to parse an
  // IntersectionObserver's root margin
  SetPropertyDefinition(kIntersectionObserverRootMarginProperty,
                        "intersection-observer-root-margin", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetAuto());

  // https://www.w3.org/TR/css-flexbox-1/#justify-content-property
  SetPropertyDefinition(kJustifyContentProperty, "justify-content",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetFlexStart());

  // https://www.w3.org/TR/CSS2/visuren.html#propdef-left
  SetPropertyDefinition(kLeftProperty, "left", kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetAuto());

  // https://www.w3.org/TR/CSS21/visudet.html#line-height
  SetPropertyDefinition(kLineHeightProperty, "line-height", kInheritedYes,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetNormal());

  // https://www.w3.org/TR/CSS21/box.html#margin-properties
  SetPropertyDefinition(kMarginBottomProperty, "margin-bottom", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        new LengthValue(0, kPixelsUnit));

  // https://www.w3.org/TR/CSS21/box.html#margin-properties
  SetPropertyDefinition(kMarginLeftProperty, "margin-left", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        new LengthValue(0, kPixelsUnit));

  // https://www.w3.org/TR/CSS21/box.html#margin-properties
  SetPropertyDefinition(kMarginRightProperty, "margin-right", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        new LengthValue(0, kPixelsUnit));

  // https://www.w3.org/TR/CSS21/box.html#margin-properties
  SetPropertyDefinition(kMarginTopProperty, "margin-top", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        new LengthValue(0, kPixelsUnit));

  // https://www.w3.org/TR/CSS2/visudet.html#propdef-max-height
  SetPropertyDefinition(kMaxHeightProperty, "max-height", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetNone());

  // https://www.w3.org/TR/CSS2/visudet.html#propdef-max-width
  SetPropertyDefinition(kMaxWidthProperty, "max-width", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetNone());

  // https://www.w3.org/TR/CSS2/visudet.html#propdef-min-height
  // https://www.w3.org/TR/css-sizing-3/#min-size-properties
  SetPropertyDefinition(kMinHeightProperty, "min-height", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetAuto());

  // https://www.w3.org/TR/CSS2/visudet.html#propdef-min-width
  // https://www.w3.org/TR/css-sizing-3/#min-size-properties
  SetPropertyDefinition(kMinWidthProperty, "min-width", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetAuto());

  // https://www.w3.org/TR/css3-color/#opacity
  SetPropertyDefinition(kOpacityProperty, "opacity", kInheritedNo,
                        kAnimatableYes, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
                        kImpactsBoxCrossReferencesYes, new NumberValue(1.0f));

  // https://www.w3.org/TR/css-flexbox-1/#order-property
  SetPropertyDefinition(kOrderProperty, "order", kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        new IntegerValue(0));

  // https://www.w3.org/TR/CSS21/ui.html#propdef-outline-color
  SetPropertyDefinition(
      kOutlineColorProperty, "outline-color", kInheritedNo, kAnimatableYes,
      kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
      kImpactsBoxCrossReferencesNo, KeywordValue::GetCurrentColor());

  // https://www.w3.org/TR/CSS21/ui.html#propdef-outline-style
  SetPropertyDefinition(kOutlineStyleProperty, "outline-style", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetNone());

  // https://www.w3.org/TR/CSS21/ui.html#propdef-outline-width
  SetPropertyDefinition(kOutlineWidthProperty, "outline-width", kInheritedNo,
                        kAnimatableYes, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        new LengthValue(3, kPixelsUnit));

  // https://www.w3.org/TR/css-overflow-3/#overflow-properties
  SetPropertyDefinition(kOverflowProperty, "overflow", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesYes,
                        KeywordValue::GetVisible());

  // https://www.w3.org/TR/css-text-3/#overflow-wrap
  SetPropertyDefinition(kOverflowWrapProperty, "overflow-wrap", kInheritedYes,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetNormal());

  // https://www.w3.org/TR/CSS21/box.html#padding-properties
  SetPropertyDefinition(kPaddingBottomProperty, "padding-bottom", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        new LengthValue(0, kPixelsUnit));

  // https://www.w3.org/TR/CSS21/box.html#padding-properties
  SetPropertyDefinition(kPaddingLeftProperty, "padding-left", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        new LengthValue(0, kPixelsUnit));

  // https://www.w3.org/TR/CSS21/box.html#padding-properties
  SetPropertyDefinition(kPaddingRightProperty, "padding-right", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        new LengthValue(0, kPixelsUnit));

  // https://www.w3.org/TR/CSS21/box.html#padding-properties
  SetPropertyDefinition(kPaddingTopProperty, "padding-top", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        new LengthValue(0, kPixelsUnit));

  // While only defined in the SVG spec, the pointer-events property has been
  // proposed an commonly implemented to also apply to HTML elements for
  // values of 'none' (element can not be indicated by a pointer) and 'auto'
  // (element can be indicated by a pointer if the element has 'visibility' set
  // to 'visible').
  //   https://www.w3.org/TR/SVG11/interact.html#PointerEventsProperty
  SetPropertyDefinition(kPointerEventsProperty, "pointer-events", kInheritedYes,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetAuto());

  // https://www.w3.org/TR/css3-positioning/#position-property
  SetPropertyDefinition(kPositionProperty, "position", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationYes, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesYes,
                        KeywordValue::GetStatic());

  // https://www.w3.org/TR/CSS2/visuren.html#propdef-right
  SetPropertyDefinition(kRightProperty, "right", kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetAuto());

  //   https://www.w3.org/TR/css-text-3/#text-align-property
  SetPropertyDefinition(kTextAlignProperty, "text-align", kInheritedYes,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetStart());

  //   https://www.w3.org/TR/css-text-decor-3/#text-decoration-color
  SetPropertyDefinition(kTextDecorationColorProperty, "text-decoration-color",
                        kInheritedNo, kAnimatableYes,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetCurrentColor());

  //   https://www.w3.org/TR/css-text-decor-3/#text-decoration-line
  SetPropertyDefinition(kTextDecorationLineProperty, "text-decoration-line",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetNone());

  // https://www.w3.org/TR/CSS21/text.html#propdef-text-indent
  SetPropertyDefinition(kTextIndentProperty, "text-indent", kInheritedYes,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        new LengthValue(0, kPixelsUnit));

  // https://www.w3.org/TR/css3-ui/#propdef-text-overflow
  SetPropertyDefinition(kTextOverflowProperty, "text-overflow", kInheritedNo,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetClip());

  // https://www.w3.org/TR/css-text-decor-3/#text-shadow-property
  SetPropertyDefinition(kTextShadowProperty, "text-shadow", kInheritedYes,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetNone());

  // https://www.w3.org/TR/css3-text/#text-transform-property
  SetPropertyDefinition(kTextTransformProperty, "text-transform", kInheritedYes,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationYes, kImpactsBoxSizesNo,
                        kImpactsBoxCrossReferencesNo, KeywordValue::GetNone());

  // https://www.w3.org/TR/CSS2/visuren.html#propdef-top
  SetPropertyDefinition(kTopProperty, "top", kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleYes, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetAuto());

  // https://www.w3.org/TR/css3-transforms/#transform-property
  SetPropertyDefinition(kTransformProperty, "transform", kInheritedNo,
                        kAnimatableYes, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesYes, KeywordValue::GetNone());

  // https://www.w3.org/TR/css3-transforms/#propdef-transform-origin
  std::unique_ptr<PropertyListValue::Builder> transform_origin_builder(
      new PropertyListValue::Builder());
  transform_origin_builder->reserve(3);
  transform_origin_builder->push_back(new CalcValue(new PercentageValue(0.5f)));
  transform_origin_builder->push_back(new CalcValue(new PercentageValue(0.5f)));
  transform_origin_builder->push_back(new LengthValue(0.0f, kPixelsUnit));
  scoped_refptr<PropertyListValue> transform_origin_list(
      new PropertyListValue(std::move(transform_origin_builder)));
  SetPropertyDefinition(
      kTransformOriginProperty, "transform-origin", kInheritedNo, kAnimatableNo,
      kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
      kImpactsBoxCrossReferencesNo, transform_origin_list);

  // https://www.w3.org/TR/css3-transitions/#transition-delay-property
  SetPropertyDefinition(
      kTransitionDelayProperty, "transition-delay", kInheritedNo, kAnimatableNo,
      kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
      kImpactsBoxCrossReferencesNo, CreateTimeListWithZeroSeconds());

  // https://www.w3.org/TR/css3-transitions/#transition-duration-property
  SetPropertyDefinition(kTransitionDurationProperty, "transition-duration",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        CreateTimeListWithZeroSeconds());

  // https://www.w3.org/TR/css3-transitions/#transition-property-property
  SetPropertyDefinition(kTransitionPropertyProperty, "transition-property",
                        kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
                        CreatePropertyKeyListWithAll());

  // https://www.w3.org/TR/css3-transitions/#transition-timing-function-property
  SetPropertyDefinition(
      kTransitionTimingFunctionProperty, "transition-timing-function",
      kInheritedNo, kAnimatableNo, kImpactsChildComputedStyleNo,
      kImpactsBoxGenerationNo, kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo,
      CreateTransitionTimingFunctionListWithEase());

  // https://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
  SetPropertyDefinition(
      kVerticalAlignProperty, "vertical-align", kInheritedNo, kAnimatableNo,
      kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
      kImpactsBoxCrossReferencesNo, KeywordValue::GetBaseline());

  // https://www.w3.org/TR/CSS21/visufx.html#propdef-visibility
  SetPropertyDefinition(
      kVisibilityProperty, "visibility", kInheritedYes, kAnimatableNo,
      kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
      kImpactsBoxCrossReferencesNo, KeywordValue::GetVisible());

  // https://www.w3.org/TR/css3-text/#white-space-property
  SetPropertyDefinition(kWhiteSpaceProperty, "white-space", kInheritedYes,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationYes, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetNormal());

  // https://www.w3.org/TR/CSS21/visudet.html#the-width-property
  SetPropertyDefinition(kWidthProperty, "width", kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleYes, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesYes, kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetAuto());

  // https://www.w3.org/TR/CSS21/visuren.html#z-index
  SetPropertyDefinition(kZIndexProperty, "z-index", kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesYes,
                        KeywordValue::GetAuto());

  // This property name can appear as a keyword for the transition-property
  // property.
  //   https://www.w3.org/TR/2013/WD-css3-transitions-20131119/#transition-property-property
  SetPropertyDefinition(kAllProperty, "all", kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo, NULL);

  // This is a descriptor for @font-face at-rules.
  //   https://www.w3.org/TR/css3-fonts/#descdef-src
  SetPropertyDefinition(kSrcProperty, "src", kInheritedNo, kAnimatableNo,
                        kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo,
                        kImpactsBoxSizesNo, kImpactsBoxCrossReferencesNo, NULL);

  //   https://www.w3.org/TR/css3-fonts/#unicode-range-desc
  SetPropertyDefinition(
      kUnicodeRangeProperty, "unicode-range", kInheritedNo, kAnimatableNo,
      kImpactsChildComputedStyleNo, kImpactsBoxGenerationNo, kImpactsBoxSizesNo,
      kImpactsBoxCrossReferencesNo, new UnicodeRangeValue(0, 0x10FFFF));

  // This is an alias for kOverflowWrap
  //   https://www.w3.org/TR/css-text-3/#overflow-wrap
  SetPropertyDefinition(kWordWrapProperty, "word-wrap", kInheritedYes,
                        kAnimatableNo, kImpactsChildComputedStyleNo,
                        kImpactsBoxGenerationNo, kImpactsBoxSizesYes,
                        kImpactsBoxCrossReferencesNo,
                        KeywordValue::GetNormal());

  // Shorthand properties.

  //   https://www.w3.org/TR/css3-background/#the-background
  LonghandPropertySet background_longhand_properties;
  background_longhand_properties.insert(kBackgroundColorProperty);
  background_longhand_properties.insert(kBackgroundImageProperty);
  background_longhand_properties.insert(kBackgroundPositionProperty);
  background_longhand_properties.insert(kBackgroundRepeatProperty);
  background_longhand_properties.insert(kBackgroundSizeProperty);
  SetShorthandPropertyDefinition(kBackgroundProperty, "background",
                                 background_longhand_properties);

  //    https://www.w3.org/TR/css3-background/#border-color
  LonghandPropertySet border_color_longhand_properties;
  border_color_longhand_properties.insert(kBorderTopColorProperty);
  border_color_longhand_properties.insert(kBorderRightColorProperty);
  border_color_longhand_properties.insert(kBorderBottomColorProperty);
  border_color_longhand_properties.insert(kBorderLeftColorProperty);
  SetShorthandPropertyDefinition(kBorderColorProperty, "border-color",
                                 border_color_longhand_properties);

  //    https://www.w3.org/TR/css3-background/#border-style
  LonghandPropertySet border_style_longhand_properties;
  border_style_longhand_properties.insert(kBorderTopStyleProperty);
  border_style_longhand_properties.insert(kBorderRightStyleProperty);
  border_style_longhand_properties.insert(kBorderBottomStyleProperty);
  border_style_longhand_properties.insert(kBorderLeftStyleProperty);
  SetShorthandPropertyDefinition(kBorderStyleProperty, "border-style",
                                 border_style_longhand_properties);

  //   https://www.w3.org/TR/css3-background/#border-width
  LonghandPropertySet border_width_longhand_properties;
  border_width_longhand_properties.insert(kBorderTopWidthProperty);
  border_width_longhand_properties.insert(kBorderRightWidthProperty);
  border_width_longhand_properties.insert(kBorderBottomWidthProperty);
  border_width_longhand_properties.insert(kBorderLeftWidthProperty);
  SetShorthandPropertyDefinition(kBorderWidthProperty, "border-width",
                                 border_width_longhand_properties);

  //   https://www.w3.org/TR/css3-background/#border-radius
  LonghandPropertySet border_radius_longhand_properties;
  border_radius_longhand_properties.insert(kBorderTopLeftRadiusProperty);
  border_radius_longhand_properties.insert(kBorderTopRightRadiusProperty);
  border_radius_longhand_properties.insert(kBorderBottomRightRadiusProperty);
  border_radius_longhand_properties.insert(kBorderBottomLeftRadiusProperty);
  SetShorthandPropertyDefinition(kBorderRadiusProperty, "border-radius",
                                 border_radius_longhand_properties);

  //   https://www.w3.org/TR/css-backgrounds-3/#propdef-border
  LonghandPropertySet border_longhand_properties;
  border_longhand_properties.insert(kBorderColorProperty);
  border_longhand_properties.insert(kBorderStyleProperty);
  border_longhand_properties.insert(kBorderWidthProperty);
  SetShorthandPropertyDefinition(kBorderProperty, "border",
                                 border_longhand_properties);

  // Border shorthand properties.
  //  https://www.w3.org/TR/css-backgrounds-3/#the-border-shorthands
  LonghandPropertySet border_top_longhand_properties;
  border_top_longhand_properties.insert(kBorderTopColorProperty);
  border_top_longhand_properties.insert(kBorderTopStyleProperty);
  border_top_longhand_properties.insert(kBorderTopWidthProperty);
  SetShorthandPropertyDefinition(kBorderTopProperty, "border-top",
                                 border_top_longhand_properties);

  LonghandPropertySet border_right_longhand_properties;
  border_right_longhand_properties.insert(kBorderRightColorProperty);
  border_right_longhand_properties.insert(kBorderRightStyleProperty);
  border_right_longhand_properties.insert(kBorderRightWidthProperty);
  SetShorthandPropertyDefinition(kBorderRightProperty, "border-right",
                                 border_right_longhand_properties);

  LonghandPropertySet border_bottom_longhand_properties;
  border_bottom_longhand_properties.insert(kBorderBottomColorProperty);
  border_bottom_longhand_properties.insert(kBorderBottomStyleProperty);
  border_bottom_longhand_properties.insert(kBorderBottomWidthProperty);
  SetShorthandPropertyDefinition(kBorderBottomProperty, "border-bottom",
                                 border_bottom_longhand_properties);

  LonghandPropertySet border_left_longhand_properties;
  border_left_longhand_properties.insert(kBorderLeftColorProperty);
  border_left_longhand_properties.insert(kBorderLeftStyleProperty);
  border_left_longhand_properties.insert(kBorderLeftWidthProperty);
  SetShorthandPropertyDefinition(kBorderLeftProperty, "border-left",
                                 border_left_longhand_properties);

  // https://www.w3.org/TR/css-flexbox-1/#flex-property
  LonghandPropertySet flex_longhand_properties;
  flex_longhand_properties.insert(kFlexGrowProperty);
  flex_longhand_properties.insert(kFlexShrinkProperty);
  flex_longhand_properties.insert(kFlexBasisProperty);
  SetShorthandPropertyDefinition(kFlexProperty, "flex",
                                 flex_longhand_properties);

  // https://www.w3.org/TR/css-flexbox-1/#flex-flow-property
  LonghandPropertySet flex_flow_longhand_properties;
  flex_flow_longhand_properties.insert(kFlexDirectionProperty);
  flex_flow_longhand_properties.insert(kFlexWrapProperty);
  SetShorthandPropertyDefinition(kFlexFlowProperty, "flex-flow",
                                 flex_flow_longhand_properties);

  //   https://www.w3.org/TR/css3-fonts/#font-prop
  LonghandPropertySet font_longhand_properties;
  font_longhand_properties.insert(kFontStyleProperty);
  font_longhand_properties.insert(kFontWeightProperty);
  font_longhand_properties.insert(kFontSizeProperty);
  font_longhand_properties.insert(kFontFamilyProperty);
  SetShorthandPropertyDefinition(kFontProperty, "font",
                                 font_longhand_properties);

  //   https://www.w3.org/TR/CSS21/box.html#propdef-margin
  LonghandPropertySet margin_longhand_properties;
  margin_longhand_properties.insert(kMarginBottomProperty);
  margin_longhand_properties.insert(kMarginLeftProperty);
  margin_longhand_properties.insert(kMarginRightProperty);
  margin_longhand_properties.insert(kMarginTopProperty);
  SetShorthandPropertyDefinition(kMarginProperty, "margin",
                                 margin_longhand_properties);

  //   https://www.w3.org/TR/CSS21/ui.html#propdef-outline
  LonghandPropertySet outline_longhand_properties;
  outline_longhand_properties.insert(kOutlineColorProperty);
  outline_longhand_properties.insert(kOutlineStyleProperty);
  outline_longhand_properties.insert(kOutlineWidthProperty);
  SetShorthandPropertyDefinition(kOutlineProperty, "outline",
                                 outline_longhand_properties);

  //   https://www.w3.org/TR/CSS21/box.html#propdef-padding
  LonghandPropertySet padding_longhand_properties;
  padding_longhand_properties.insert(kPaddingBottomProperty);
  padding_longhand_properties.insert(kPaddingLeftProperty);
  padding_longhand_properties.insert(kPaddingRightProperty);
  padding_longhand_properties.insert(kPaddingTopProperty);
  SetShorthandPropertyDefinition(kPaddingProperty, "padding",
                                 padding_longhand_properties);

  //   https://www.w3.org/TR/css-text-decor-3/#text-decoration
  LonghandPropertySet text_decoration_longhand_properties;
  text_decoration_longhand_properties.insert(kTextDecorationColorProperty);
  text_decoration_longhand_properties.insert(kTextDecorationLineProperty);
  SetShorthandPropertyDefinition(kTextDecorationProperty, "text-decoration",
                                 text_decoration_longhand_properties);

  //   https://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-shorthand-property
  LonghandPropertySet animation_longhand_properties;
  animation_longhand_properties.insert(kAnimationDelayProperty);
  animation_longhand_properties.insert(kAnimationDirectionProperty);
  animation_longhand_properties.insert(kAnimationDurationProperty);
  animation_longhand_properties.insert(kAnimationFillModeProperty);
  animation_longhand_properties.insert(kAnimationIterationCountProperty);
  animation_longhand_properties.insert(kAnimationNameProperty);
  animation_longhand_properties.insert(kAnimationTimingFunctionProperty);
  SetShorthandPropertyDefinition(kAnimationProperty, "animation",
                                 animation_longhand_properties);

  //   https://www.w3.org/TR/css3-transitions/#transition-shorthand-property
  LonghandPropertySet transition_longhand_properties;
  transition_longhand_properties.insert(kTransitionDelayProperty);
  transition_longhand_properties.insert(kTransitionDurationProperty);
  transition_longhand_properties.insert(kTransitionPropertyProperty);
  transition_longhand_properties.insert(kTransitionTimingFunctionProperty);
  SetShorthandPropertyDefinition(kTransitionProperty, "transition",
                                 transition_longhand_properties);

  CompileSetOfAnimatableProperties();
  CompileSortedLonghandProperties();
}  // NOLINT(readability/fn_size)

void NonTrivialGlobalVariables::CompileSetOfAnimatableProperties() {
  for (int i = 0; i < kMaxEveryPropertyKey + 1; ++i) {
    if (properties[i].animatable == kAnimatableYes) {
      animatable_properties.push_back(static_cast<PropertyKey>(i));
      if (properties[i].inherited == kInheritedYes) {
        inherited_animatable_properties.push_back(static_cast<PropertyKey>(i));
      }
    }
  }
}

void NonTrivialGlobalVariables::CompileSortedLonghandProperties() {
  // When serializing the CSS Declaration block returned by getComputedStyle,
  // all supported longhand CSS Properties must be listed in lexicographical
  // order. This prepares the lexicographical_longhand_keys array to contain
  // PropertyKeys in lexicographical order. See
  // https://www.w3.org/TR/2013/WD-cssom-20131205/#dom-window-getcomputedstyle.
  lexicographical_longhand_keys.resize(kMaxLonghandPropertyKey + 1);
  for (size_t i = 0; i < lexicographical_longhand_keys.size(); ++i) {
    lexicographical_longhand_keys[i] = static_cast<PropertyKey>(i);
  }
  std::sort(lexicographical_longhand_keys.begin(),
            lexicographical_longhand_keys.end(), NameLess(*this));
}

base::LazyInstance<NonTrivialGlobalVariables>::DestructorAtExit
    non_trivial_global_variables = LAZY_INSTANCE_INITIALIZER;
}  // namespace

const char* GetPropertyName(PropertyKey key) {
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxEveryPropertyKey);
  const char* name = non_trivial_global_variables.Get().properties[key].name;
  return name;
}

const scoped_refptr<PropertyValue>& GetPropertyInitialValue(PropertyKey key) {
  DCHECK(!IsShorthandProperty(key));
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxEveryPropertyKey);
  const scoped_refptr<PropertyValue>& initial_value =
      non_trivial_global_variables.Get().properties[key].initial_value;
  return initial_value;
}

Inherited GetPropertyInheritance(PropertyKey key) {
  DCHECK(!IsShorthandProperty(key));
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxEveryPropertyKey);
  return non_trivial_global_variables.Get().properties[key].inherited;
}

Animatable GetPropertyAnimatable(PropertyKey key) {
  DCHECK(!IsShorthandProperty(key));
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxEveryPropertyKey);
  return non_trivial_global_variables.Get().properties[key].animatable;
}

ImpactsChildComputedStyle GetPropertyImpactsChildComputedStyle(
    PropertyKey key) {
  DCHECK(!IsShorthandProperty(key));
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxEveryPropertyKey);
  return non_trivial_global_variables.Get()
      .properties[key]
      .impacts_child_computed_style;
}

ImpactsBoxGeneration GetPropertyImpactsBoxGeneration(PropertyKey key) {
  DCHECK(!IsShorthandProperty(key));
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxEveryPropertyKey);
  return non_trivial_global_variables.Get()
      .properties[key]
      .impacts_box_generation;
}

ImpactsBoxSizes GetPropertyImpactsBoxSizes(PropertyKey key) {
  DCHECK(!IsShorthandProperty(key));
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxEveryPropertyKey);
  return non_trivial_global_variables.Get().properties[key].impacts_box_sizes;
}

ImpactsBoxCrossReferences GetPropertyImpactsBoxCrossReferences(
    PropertyKey key) {
  DCHECK(!IsShorthandProperty(key));
  DCHECK_GT(key, kNoneProperty);
  DCHECK_LE(key, kMaxEveryPropertyKey);
  return non_trivial_global_variables.Get()
      .properties[key]
      .impacts_box_cross_references;
}

bool IsShorthandProperty(PropertyKey key) {
  return key >= kFirstShorthandPropertyKey && key <= kMaxShorthandPropertyKey;
}

const LonghandPropertySet& ExpandShorthandProperty(PropertyKey key) {
  DCHECK(IsShorthandProperty(key));
  return non_trivial_global_variables.Get().properties[key].longhand_properties;
}

const PropertyKeyVector& GetAnimatableProperties() {
  return non_trivial_global_variables.Get().animatable_properties;
}

const PropertyKeyVector& GetInheritedAnimatableProperties() {
  return non_trivial_global_variables.Get().inherited_animatable_properties;
}

PropertyKey GetLexicographicalLonghandPropertyKey(const size_t index) {
  DCHECK_LE(
      index,
      non_trivial_global_variables.Get().lexicographical_longhand_keys.size());
  return non_trivial_global_variables.Get()
      .lexicographical_longhand_keys[index];
}

PropertyKey GetPropertyKey(const std::string& property_name) {
  switch (property_name.size()) {
    case 3:
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kTopProperty))) {
        return kTopProperty;
      }
      return kNoneProperty;

    case 4:
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kFlexProperty))) {
        return kFlexProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kFontProperty))) {
        return kFontProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kLeftProperty))) {
        return kLeftProperty;
      }
      return kNoneProperty;

    case 5:
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kColorProperty))) {
        return kColorProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kOrderProperty))) {
        return kOrderProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kRightProperty))) {
        return kRightProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kWidthProperty))) {
        return kWidthProperty;
      }
      return kNoneProperty;

    case 6:
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kBorderProperty))) {
        return kBorderProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kBottomProperty))) {
        return kBottomProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kFilterProperty))) {
        return kFilterProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kHeightProperty))) {
        return kHeightProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kMarginProperty))) {
        return kMarginProperty;
      }
      return kNoneProperty;

    case 7:
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kContentProperty))) {
        return kContentProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kDisplayProperty))) {
        return kDisplayProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kOpacityProperty))) {
        return kOpacityProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kOutlineProperty))) {
        return kOutlineProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kPaddingProperty))) {
        return kPaddingProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kZIndexProperty))) {
        return kZIndexProperty;
      }
      return kNoneProperty;

    case 8:
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kOverflowProperty))) {
        return kOverflowProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kPositionProperty))) {
        return kPositionProperty;
      }
      return kNoneProperty;

    case 9:
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kAnimationProperty))) {
        return kAnimationProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kFlexFlowProperty))) {
        return kFlexFlowProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kFlexGrowProperty))) {
        return kFlexGrowProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kFlexWrapProperty))) {
        return kFlexWrapProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kFontSizeProperty))) {
        return kFontSizeProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kMaxWidthProperty))) {
        return kMaxWidthProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kMinWidthProperty))) {
        return kMinWidthProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kTransformProperty))) {
        return kTransformProperty;
      }
      return kNoneProperty;

    case 10:
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kAlignSelfProperty))) {
        return kAlignSelfProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kBackgroundProperty))) {
        return kBackgroundProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kBorderTopProperty))) {
        return kBorderTopProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kBoxShadowProperty))) {
        return kBoxShadowProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kFlexBasisProperty))) {
        return kFlexBasisProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kFontStyleProperty))) {
        return kFontStyleProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kMarginTopProperty))) {
        return kMarginTopProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kMaxHeightProperty))) {
        return kMaxHeightProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kMinHeightProperty))) {
        return kMinHeightProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kTextAlignProperty))) {
        return kTextAlignProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kTransitionProperty))) {
        return kTransitionProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kVisibilityProperty))) {
        return kVisibilityProperty;
      }
      return kNoneProperty;

    case 11:
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kAlignItemsProperty))) {
        return kAlignItemsProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kBorderLeftProperty))) {
        return kBorderLeftProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kFlexShrinkProperty))) {
        return kFlexShrinkProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kFontFamilyProperty))) {
        return kFontFamilyProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kFontWeightProperty))) {
        return kFontWeightProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kLineHeightProperty))) {
        return kLineHeightProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kMarginLeftProperty))) {
        return kMarginLeftProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kPaddingTopProperty))) {
        return kPaddingTopProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kTextIndentProperty))) {
        return kTextIndentProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kTextShadowProperty))) {
        return kTextShadowProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kWhiteSpaceProperty))) {
        return kWhiteSpaceProperty;
      }
      return kNoneProperty;

    case 12:
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kBorderColorProperty))) {
        return kBorderColorProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kBorderRightProperty))) {
        return kBorderRightProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kBorderStyleProperty))) {
        return kBorderStyleProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kBorderWidthProperty))) {
        return kBorderWidthProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kMarginRightProperty))) {
        return kMarginRightProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kPaddingLeftProperty))) {
        return kPaddingLeftProperty;
      }
      return kNoneProperty;

    case 13:
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kAlignContentProperty))) {
        return kAlignContentProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kBorderBottomProperty))) {
        return kBorderBottomProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kBorderRadiusProperty))) {
        return kBorderRadiusProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kMarginBottomProperty))) {
        return kMarginBottomProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kOutlineColorProperty))) {
        return kOutlineColorProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kOutlineStyleProperty))) {
        return kOutlineStyleProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kOutlineWidthProperty))) {
        return kOutlineWidthProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kOverflowWrapProperty))) {
        return kOverflowWrapProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kPaddingRightProperty))) {
        return kPaddingRightProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kTextOverflowProperty))) {
        return kTextOverflowProperty;
      }
      return kNoneProperty;

    case 14:
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kAnimationNameProperty))) {
        return kAnimationNameProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kFlexDirectionProperty))) {
        return kFlexDirectionProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kPaddingBottomProperty))) {
        return kPaddingBottomProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kPointerEventsProperty))) {
        return kPointerEventsProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kTextTransformProperty))) {
        return kTextTransformProperty;
      }
      if (base::LowerCaseEqualsASCII(property_name,
                                     GetPropertyName(kVerticalAlignProperty))) {
        return kVerticalAlignProperty;
      }
      return kNoneProperty;

    case 15:
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kAnimationDelayProperty))) {
        return kAnimationDelayProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBackgroundSizeProperty))) {
        return kBackgroundSizeProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kJustifyContentProperty))) {
        return kJustifyContentProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kTextDecorationProperty))) {
        return kTextDecorationProperty;
      }
      return kNoneProperty;

    case 16:
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBackgroundColorProperty))) {
        return kBackgroundColorProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderTopColorProperty))) {
        return kBorderTopColorProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderTopStyleProperty))) {
        return kBorderTopStyleProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderTopWidthProperty))) {
        return kBorderTopWidthProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBackgroundImageProperty))) {
        return kBackgroundImageProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kTransformOriginProperty))) {
        return kTransformOriginProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kTransitionDelayProperty))) {
        return kTransitionDelayProperty;
      }
      return kNoneProperty;

    case 17:
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBackgroundRepeatProperty))) {
        return kBackgroundRepeatProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderLeftColorProperty))) {
        return kBorderLeftColorProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderLeftStyleProperty))) {
        return kBorderLeftStyleProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderLeftWidthProperty))) {
        return kBorderLeftWidthProperty;
      }
      return kNoneProperty;

    case 18:
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kAnimationDurationProperty))) {
        return kAnimationDurationProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderRightColorProperty))) {
        return kBorderRightColorProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderRightStyleProperty))) {
        return kBorderRightStyleProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderRightWidthProperty))) {
        return kBorderRightWidthProperty;
      }
      return kNoneProperty;

    case 19:
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kAnimationDirectionProperty))) {
        return kAnimationDirectionProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kAnimationFillModeProperty))) {
        return kAnimationFillModeProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBackgroundPositionProperty))) {
        return kBackgroundPositionProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderBottomColorProperty))) {
        return kBorderBottomColorProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderBottomStyleProperty))) {
        return kBorderBottomStyleProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderBottomWidthProperty))) {
        return kBorderBottomWidthProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kTransitionDurationProperty))) {
        return kTransitionDurationProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kTransitionPropertyProperty))) {
        return kTransitionPropertyProperty;
      }
      return kNoneProperty;

    case 20:
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kTextDecorationLineProperty))) {
        return kTextDecorationLineProperty;
      }
      return kNoneProperty;

    case 21:
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kTextDecorationColorProperty))) {
        return kTextDecorationColorProperty;
      }
      return kNoneProperty;

    case 22:
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderTopLeftRadiusProperty))) {
        return kBorderTopLeftRadiusProperty;
      }
      return kNoneProperty;

    case 23:
      if (base::LowerCaseEqualsASCII(
              property_name, GetPropertyName(kBorderTopRightRadiusProperty))) {
        return kBorderTopRightRadiusProperty;
      }
      return kNoneProperty;

    case 25:
      if (base::LowerCaseEqualsASCII(
              property_name,
              GetPropertyName(kAnimationIterationCountProperty))) {
        return kAnimationIterationCountProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name,
              GetPropertyName(kAnimationTimingFunctionProperty))) {
        return kAnimationTimingFunctionProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name,
              GetPropertyName(kBorderBottomLeftRadiusProperty))) {
        return kBorderBottomLeftRadiusProperty;
      }
      return kNoneProperty;

    case 26:
      if (base::LowerCaseEqualsASCII(
              property_name,
              GetPropertyName(kBorderBottomRightRadiusProperty))) {
        return kBorderBottomRightRadiusProperty;
      }
      if (base::LowerCaseEqualsASCII(
              property_name,
              GetPropertyName(kTransitionTimingFunctionProperty))) {
        return kTransitionTimingFunctionProperty;
      }
      return kNoneProperty;

    case 33:
      if (base::LowerCaseEqualsASCII(
              property_name,
              GetPropertyName(kIntersectionObserverRootMarginProperty))) {
        return kIntersectionObserverRootMarginProperty;
      }
      return kNoneProperty;

    default:
      return kNoneProperty;
  }
}  // NOLINT(readability/fn_size)

// NOLINTNEXTLINE(readability/fn_size)
}  // namespace cssom
// NOLINTNEXTLINE(readability/fn_size)
}  // namespace cobalt

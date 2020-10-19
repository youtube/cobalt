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

#include <memory>

#include "base/memory/ptr_util.h"
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/cobalt_ui_nav_focus_transform_function.h"
#include "cobalt/cssom/cobalt_ui_nav_spotlight_transform_function.h"
#include "cobalt/cssom/filter_function_list_value.h"
#include "cobalt/cssom/font_style_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/integer_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/linear_gradient_value.h"
#include "cobalt/cssom/map_to_mesh_function.h"
#include "cobalt/cssom/matrix_function.h"
#include "cobalt/cssom/media_feature_keyword_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/property_key_list_value.h"
#include "cobalt/cssom/radial_gradient_value.h"
#include "cobalt/cssom/ratio_value.h"
#include "cobalt/cssom/resolution_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/rotate_function.h"
#include "cobalt/cssom/scale_function.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/time_list_value.h"
#include "cobalt/cssom/timing_function.h"
#include "cobalt/cssom/timing_function_list_value.h"
#include "cobalt/cssom/transform_function.h"
#include "cobalt/cssom/transform_function_list_value.h"
#include "cobalt/cssom/translate_function.h"
#include "cobalt/cssom/url_value.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

// These tests help ensure that we property return false when comparing two
// property values of different types for equality.
TEST(PropertyValueIsEqualTest, DifferingTypes) {
  scoped_refptr<KeywordValue> value_a = KeywordValue::GetAuto();
  scoped_refptr<FontWeightValue> value_b = FontWeightValue::GetThinAka100();

  EXPECT_FALSE(value_a->Equals(*value_b));
}

// Type-specific tests.
TEST(PropertyValueIsEqualTest, AbsoluteURLsAreEqual) {
  GURL url_a("https://www.youtube.com");
  scoped_refptr<AbsoluteURLValue> value_a(new AbsoluteURLValue(url_a));
  GURL url_b("https://www.youtube.com");
  scoped_refptr<AbsoluteURLValue> value_b(new AbsoluteURLValue(url_b));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, AbsoluteURLsAreNotEqual) {
  GURL url_a("https://www.youtube.com");
  scoped_refptr<AbsoluteURLValue> value_a(new AbsoluteURLValue(url_a));
  GURL url_b("https://www.google.com");
  scoped_refptr<AbsoluteURLValue> value_b(new AbsoluteURLValue(url_b));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, CobaltUiNavFocusTransformFunctionsAreEqual) {
  CobaltUiNavFocusTransformFunction function_a(1.0f, 1.0f);
  CobaltUiNavFocusTransformFunction function_b(1.0f, 1.0f);

  EXPECT_TRUE(function_a.Equals(function_b));
}

TEST(PropertyValueIsEqualTest, CobaltUiNavFocusTransformFunctionsAreNotEqual) {
  CobaltUiNavFocusTransformFunction function_a(1.0f, 1.0f, 1.0f);
  CobaltUiNavFocusTransformFunction function_b(1.0f, 1.0f, 0.0f);
  CobaltUiNavFocusTransformFunction function_c(1.0f, 2.0f, 1.0f);
  CobaltUiNavFocusTransformFunction function_d(2.0f, 1.0f, 1.0f);

  EXPECT_FALSE(function_a.Equals(function_b));
  EXPECT_FALSE(function_a.Equals(function_c));
  EXPECT_FALSE(function_a.Equals(function_d));
  EXPECT_FALSE(function_b.Equals(function_c));
  EXPECT_FALSE(function_b.Equals(function_d));
  EXPECT_FALSE(function_c.Equals(function_d));
}

TEST(PropertyValueIsEqualTest, CobaltUiNavSpotlightTransformFunctionsAreEqual) {
  CobaltUiNavSpotlightTransformFunction function_a;
  CobaltUiNavSpotlightTransformFunction function_b;

  EXPECT_TRUE(function_a.Equals(function_b));
}

TEST(PropertyValueIsEqualTest,
     CobaltUiNavSpotlightTransformFunctionsAreNotEqual) {
  CobaltUiNavSpotlightTransformFunction function_a(0.0f);
  CobaltUiNavSpotlightTransformFunction function_b(1.0f);

  EXPECT_FALSE(function_a.Equals(function_b));
}

TEST(PropertyValueIsEqualTest, FontStylesAreEqual) {
  scoped_refptr<FontStyleValue> value_a = FontStyleValue::GetItalic();
  scoped_refptr<FontStyleValue> value_b = FontStyleValue::GetItalic();

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, FontStylesAreNotEqual) {
  scoped_refptr<FontStyleValue> value_a = FontStyleValue::GetItalic();
  scoped_refptr<FontStyleValue> value_b = FontStyleValue::GetNormal();

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, FontWeightsAreEqual) {
  scoped_refptr<FontWeightValue> value_a = FontWeightValue::GetThinAka100();
  scoped_refptr<FontWeightValue> value_b = FontWeightValue::GetThinAka100();

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, FontWeightsAreNotEqual) {
  scoped_refptr<FontWeightValue> value_a = FontWeightValue::GetThinAka100();
  scoped_refptr<FontWeightValue> value_b = FontWeightValue::GetLightAka300();

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, KeywordsAreEqual) {
  scoped_refptr<KeywordValue> value_a = KeywordValue::GetAuto();
  scoped_refptr<KeywordValue> value_b = KeywordValue::GetAuto();

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, KeywordsAreNotEqual) {
  scoped_refptr<KeywordValue> value_a = KeywordValue::GetAuto();
  scoped_refptr<KeywordValue> value_b = KeywordValue::GetBlock();

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, IntegersAreEqual) {
  scoped_refptr<IntegerValue> value_a(new IntegerValue(1));
  scoped_refptr<IntegerValue> value_b(new IntegerValue(1));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, IntegersAreNotEqual) {
  scoped_refptr<IntegerValue> value_a(new IntegerValue(1));
  scoped_refptr<IntegerValue> value_b(new IntegerValue(2));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, LengthsAreEqual) {
  scoped_refptr<LengthValue> value_a(new LengthValue(1.5f, kPixelsUnit));
  scoped_refptr<LengthValue> value_b(new LengthValue(1.5f, kPixelsUnit));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, LengthsAreNotEqual) {
  scoped_refptr<LengthValue> value_a(new LengthValue(1.5f, kPixelsUnit));
  scoped_refptr<LengthValue> value_b(
      new LengthValue(1.5f, kFontSizesAkaEmUnit));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, MediaFeatureKeywordsAreEqual) {
  scoped_refptr<MediaFeatureKeywordValue> value_a =
      MediaFeatureKeywordValue::GetLandscape();
  scoped_refptr<MediaFeatureKeywordValue> value_b =
      MediaFeatureKeywordValue::GetLandscape();

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, MediaFeatureKeywordsAreNotEqual) {
  scoped_refptr<MediaFeatureKeywordValue> value_a =
      MediaFeatureKeywordValue::GetLandscape();
  scoped_refptr<MediaFeatureKeywordValue> value_b =
      MediaFeatureKeywordValue::GetPortrait();

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, NumbersAreEqual) {
  scoped_refptr<NumberValue> value_a(new NumberValue(1.0f));
  scoped_refptr<NumberValue> value_b(new NumberValue(1.0f));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, NumbersAreNotEqual) {
  scoped_refptr<NumberValue> value_a(new NumberValue(1.0f));
  scoped_refptr<NumberValue> value_b(new NumberValue(2.0f));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, PercentagesAreEqual) {
  scoped_refptr<PercentageValue> value_a(new PercentageValue(0.5f));
  scoped_refptr<PercentageValue> value_b(new PercentageValue(0.5f));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, PercentagesAreNotEqual) {
  scoped_refptr<PercentageValue> value_a(new PercentageValue(0.25f));
  scoped_refptr<PercentageValue> value_b(new PercentageValue(0.5f));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, PropertyKeyListsAreEqual) {
  PropertyKeyListValue::Builder property_list;
  property_list.push_back(kBackgroundColorProperty);
  property_list.push_back(kOpacityProperty);
  scoped_refptr<PropertyKeyListValue> value_a(new PropertyKeyListValue(
      base::WrapUnique(new PropertyKeyListValue::Builder(property_list))));
  scoped_refptr<PropertyKeyListValue> value_b(new PropertyKeyListValue(
      base::WrapUnique(new PropertyKeyListValue::Builder(property_list))));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, PropertyNameListsAreNotEqual) {
  PropertyKeyListValue::Builder property_list;
  property_list.push_back(kBackgroundColorProperty);
  property_list.push_back(kOpacityProperty);
  scoped_refptr<PropertyKeyListValue> value_a(new PropertyKeyListValue(
      base::WrapUnique(new PropertyKeyListValue::Builder(property_list))));
  property_list.back() = kTransformProperty;
  scoped_refptr<PropertyKeyListValue> value_b(new PropertyKeyListValue(
      base::WrapUnique(new PropertyKeyListValue::Builder(property_list))));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, RatioValuesAreEqual) {
  scoped_refptr<RatioValue> value_a(new RatioValue(math::Rational(16, 9)));
  scoped_refptr<RatioValue> value_b(new RatioValue(math::Rational(16, 9)));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, RatioValuesAreNotEqual) {
  scoped_refptr<RatioValue> value_a(new RatioValue(math::Rational(16, 9)));
  scoped_refptr<RatioValue> value_b(new RatioValue(math::Rational(4, 3)));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, ResolutionValuesAreEqual) {
  scoped_refptr<ResolutionValue> value_a(new ResolutionValue(100.0f, kDPIUnit));
  scoped_refptr<ResolutionValue> value_b(new ResolutionValue(100.0f, kDPIUnit));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, ResolutionValuesAreNotEqual) {
  scoped_refptr<ResolutionValue> value_a(new ResolutionValue(100.0f, kDPIUnit));
  scoped_refptr<ResolutionValue> value_b(new ResolutionValue(120.0f, kDPIUnit));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, RGBAColorsAreEqual) {
  scoped_refptr<RGBAColorValue> value_a(new RGBAColorValue(0x00ff00ff));
  scoped_refptr<RGBAColorValue> value_b(new RGBAColorValue(0x00ff00ff));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, RGBAColorsAreNotEqual) {
  scoped_refptr<RGBAColorValue> value_a(new RGBAColorValue(0x00ff00ff));
  scoped_refptr<RGBAColorValue> value_b(new RGBAColorValue(0xff00ff00));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, LinearGradientValuesAreEqual) {
  scoped_refptr<RGBAColorValue> property_color(
      new RGBAColorValue(100, 0, 50, 255));
  scoped_refptr<LengthValue> property_length(new LengthValue(212, kPixelsUnit));

  ColorStopList color_stop_list_a;
  color_stop_list_a.emplace_back(
      new ColorStop(property_color, property_length));
  ColorStopList color_stop_list_b;
  color_stop_list_b.emplace_back(
      new ColorStop(property_color, property_length));

  scoped_refptr<LinearGradientValue> value_a(
      new LinearGradientValue(123.0f, std::move(color_stop_list_a)));
  scoped_refptr<LinearGradientValue> value_b(
      new LinearGradientValue(123.0f, std::move(color_stop_list_b)));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, LinearGradientValuesAreNotEqual) {
  scoped_refptr<RGBAColorValue> property_color(
      new RGBAColorValue(100, 0, 50, 255));
  scoped_refptr<LengthValue> property_length(new LengthValue(212, kPixelsUnit));

  ColorStopList color_stop_list_a;
  color_stop_list_a.emplace_back(
      new ColorStop(property_color, property_length));
  ColorStopList color_stop_list_b;
  color_stop_list_b.emplace_back(
      new ColorStop(property_color, property_length));

  scoped_refptr<RGBAColorValue> property_color_c(
      new RGBAColorValue(10, 10, 10, 255));
  ColorStopList color_stop_list_c;
  color_stop_list_c.emplace_back(
      new ColorStop(property_color_c, property_length));

  scoped_refptr<LinearGradientValue> value_a(
      new LinearGradientValue(123.0f, std::move(color_stop_list_a)));
  scoped_refptr<LinearGradientValue> value_b(
      new LinearGradientValue(456.0f, std::move(color_stop_list_b)));
  scoped_refptr<LinearGradientValue> value_c(
      new LinearGradientValue(123.0f, std::move(color_stop_list_c)));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, RadialGradientValuesAreEqual) {
  scoped_refptr<RGBAColorValue> property_color(
      new RGBAColorValue(100, 0, 50, 255));
  scoped_refptr<LengthValue> property_length(new LengthValue(212, kPixelsUnit));

  ColorStopList color_stop_list_a;
  color_stop_list_a.emplace_back(
      new ColorStop(property_color, property_length));
  ColorStopList color_stop_list_b;
  color_stop_list_b.emplace_back(
      new ColorStop(property_color, property_length));

  std::unique_ptr<PropertyListValue::Builder> position_a_builder(
      new PropertyListValue::Builder());
  position_a_builder->emplace_back(new LengthValue(1.5f, kPixelsUnit));
  scoped_refptr<PropertyListValue> position_a(
      new PropertyListValue(std::move(position_a_builder)));

  std::unique_ptr<PropertyListValue::Builder> position_b_builder(
      new PropertyListValue::Builder());
  position_b_builder->emplace_back(new LengthValue(1.5f, kPixelsUnit));
  scoped_refptr<PropertyListValue> position_b(
      new PropertyListValue(std::move(position_b_builder)));

  scoped_refptr<RadialGradientValue> value_a(new RadialGradientValue(
      RadialGradientValue::kCircle, RadialGradientValue::kFarthestSide,
      position_a, std::move(color_stop_list_a)));

  scoped_refptr<RadialGradientValue> value_b(new RadialGradientValue(
      RadialGradientValue::kCircle, RadialGradientValue::kFarthestSide,
      position_b, std::move(color_stop_list_b)));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, RadialGradientValuesAreNotEqual) {
  scoped_refptr<RGBAColorValue> property_color(
      new RGBAColorValue(100, 0, 50, 255));
  scoped_refptr<LengthValue> property_length(new LengthValue(212, kPixelsUnit));

  ColorStopList color_stop_list_a;
  color_stop_list_a.emplace_back(
      new ColorStop(property_color, property_length));
  ColorStopList color_stop_list_b;
  color_stop_list_b.emplace_back(
      new ColorStop(property_color, property_length));

  std::unique_ptr<PropertyListValue::Builder> size_a_builder(
      new PropertyListValue::Builder());
  size_a_builder->emplace_back(new LengthValue(1.5f, kPixelsUnit));
  scoped_refptr<PropertyListValue> size_a(
      new PropertyListValue(std::move(size_a_builder)));

  std::unique_ptr<PropertyListValue::Builder> size_b_builder(
      new PropertyListValue::Builder());
  size_b_builder->emplace_back(new LengthValue(2.5f, kPixelsUnit));
  scoped_refptr<PropertyListValue> size_b(
      new PropertyListValue(std::move(size_b_builder)));

  std::unique_ptr<PropertyListValue::Builder> position_a_builder(
      new PropertyListValue::Builder());
  position_a_builder->emplace_back(new LengthValue(3.0f, kFontSizesAkaEmUnit));
  scoped_refptr<PropertyListValue> position_a(
      new PropertyListValue(std::move(position_a_builder)));

  std::unique_ptr<PropertyListValue::Builder> position_b_builder(
      new PropertyListValue::Builder());
  position_b_builder->emplace_back(new LengthValue(1.5f, kPixelsUnit));
  scoped_refptr<PropertyListValue> position_b(
      new PropertyListValue(std::move(position_b_builder)));

  scoped_refptr<RadialGradientValue> value_a(
      new RadialGradientValue(RadialGradientValue::kCircle, size_a, position_a,
                              std::move(color_stop_list_a)));

  scoped_refptr<RadialGradientValue> value_b(
      new RadialGradientValue(RadialGradientValue::kCircle, size_b, position_b,
                              std::move(color_stop_list_b)));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, StringsAreEqual) {
  scoped_refptr<StringValue> value_a(new StringValue("foo"));
  scoped_refptr<StringValue> value_b(new StringValue("foo"));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, StringsAreNotEqual) {
  scoped_refptr<StringValue> value_a(new StringValue("foo"));
  scoped_refptr<StringValue> value_b(new StringValue("bar"));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, TimeListsAreEqual) {
  TimeListValue::Builder time_list;
  time_list.push_back(base::TimeDelta::FromSeconds(3));
  time_list.push_back(base::TimeDelta::FromMilliseconds(2));

  scoped_refptr<TimeListValue> value_a(new TimeListValue(
      base::WrapUnique(new TimeListValue::Builder(time_list))));
  scoped_refptr<TimeListValue> value_b(new TimeListValue(
      base::WrapUnique(new TimeListValue::Builder(time_list))));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, TimeListsAreNotEqual) {
  TimeListValue::Builder time_list;
  time_list.push_back(base::TimeDelta::FromSeconds(3));
  time_list.push_back(base::TimeDelta::FromMilliseconds(2));

  scoped_refptr<TimeListValue> value_a(new TimeListValue(
      base::WrapUnique(new TimeListValue::Builder(time_list))));

  time_list.back() = base::TimeDelta::FromSeconds(2);

  scoped_refptr<TimeListValue> value_b(new TimeListValue(
      base::WrapUnique(new TimeListValue::Builder(time_list))));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, TimingFunctionListsAreEqual) {
  std::unique_ptr<TimingFunctionListValue::Builder> timing_function_list_a(
      new TimingFunctionListValue::Builder());
  timing_function_list_a->push_back(TimingFunction::GetLinear());
  timing_function_list_a->push_back(
      new SteppingTimingFunction(3, SteppingTimingFunction::kEnd));
  scoped_refptr<TimingFunctionListValue> value_a(
      new TimingFunctionListValue(std::move(timing_function_list_a)));

  std::unique_ptr<TimingFunctionListValue::Builder> timing_function_list_b(
      new TimingFunctionListValue::Builder());
  timing_function_list_b->push_back(
      new CubicBezierTimingFunction(0.0f, 0.0f, 1.0f, 1.0f));
  timing_function_list_b->push_back(
      new SteppingTimingFunction(3, SteppingTimingFunction::kEnd));
  scoped_refptr<TimingFunctionListValue> value_b(
      new TimingFunctionListValue(std::move(timing_function_list_b)));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, TimingFunctionListsAreNotEqual) {
  std::unique_ptr<TimingFunctionListValue::Builder> timing_function_list_a(
      new TimingFunctionListValue::Builder());
  timing_function_list_a->push_back(TimingFunction::GetLinear());
  timing_function_list_a->push_back(
      new SteppingTimingFunction(3, SteppingTimingFunction::kEnd));
  scoped_refptr<TimingFunctionListValue> value_a(
      new TimingFunctionListValue(std::move(timing_function_list_a)));

  std::unique_ptr<TimingFunctionListValue::Builder> timing_function_list_b(
      new TimingFunctionListValue::Builder());
  timing_function_list_b->push_back(
      new CubicBezierTimingFunction(0.0f, 0.5f, 1.0f, 1.0f));
  timing_function_list_b->push_back(
      new SteppingTimingFunction(3, SteppingTimingFunction::kEnd));
  scoped_refptr<TimingFunctionListValue> value_b(
      new TimingFunctionListValue(std::move(timing_function_list_b)));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, MatrixFunctionsAreEqual) {
  MatrixFunction function_a(0.0f, 1.0f, 0.1f, 1.1f, 0.2f, 1.2f);
  MatrixFunction function_b(0.0f, 1.0f, 0.1f, 1.1f, 0.2f, 1.2f);

  EXPECT_TRUE(function_a.Equals(function_b));
}

TEST(PropertyValueIsEqualTest, MatrixFunctionsAreNotEqual) {
  MatrixFunction function_a(0.0f, 1.0f, 0.1f, 1.1f, 0.2f, 1.2f);
  MatrixFunction function_b(5.0f, 4.0f, 3.1f, 6.1f, 7.2f, 8.2f);

  EXPECT_FALSE(function_a.Equals(function_b));
}

TEST(PropertyValueIsEqualTest, RotateFunctionsAreEqual) {
  RotateFunction function_a(3.1415f);
  RotateFunction function_b(3.1415f);

  EXPECT_TRUE(function_a.Equals(function_b));
}

TEST(PropertyValueIsEqualTest, RotateFunctionsAreNotEqual) {
  RotateFunction function_a(3.1415f);
  RotateFunction function_b(2.7183f);

  EXPECT_FALSE(function_a.Equals(function_b));
}

TEST(PropertyValueIsEqualTest, ScaleFunctionsAreEqual) {
  ScaleFunction function_a(12.34f, 56.78f);
  ScaleFunction function_b(12.34f, 56.78f);

  EXPECT_TRUE(function_a.Equals(function_b));
}

TEST(PropertyValueIsEqualTest, ScaleFunctionsAreNotEqual) {
  ScaleFunction function_a(12.34f, 56.78f);
  ScaleFunction function_b(98.76f, 54.32f);

  EXPECT_FALSE(function_a.Equals(function_b));
}

TEST(PropertyValueIsEqualTest, TranslateFunctionsAreEqual) {
  scoped_refptr<LengthValue> property_a(
      new LengthValue(3, kFontSizesAkaEmUnit));
  TranslateFunction function_a(TranslateFunction::kYAxis, property_a);
  scoped_refptr<LengthValue> property_b(
      new LengthValue(3, kFontSizesAkaEmUnit));
  TranslateFunction function_b(TranslateFunction::kYAxis, property_b);

  EXPECT_TRUE(function_a.Equals(function_b));
}

TEST(PropertyValueIsEqualTest, TranslateFunctionsAreNotEqual) {
  scoped_refptr<LengthValue> property_a(
      new LengthValue(3, kFontSizesAkaEmUnit));
  TranslateFunction function_a(TranslateFunction::kYAxis, property_a);
  scoped_refptr<LengthValue> property_b(new LengthValue(3, kPixelsUnit));
  TranslateFunction function_b(TranslateFunction::kYAxis, property_b);
  scoped_refptr<LengthValue> property_c(new LengthValue(4, kPixelsUnit));
  TranslateFunction function_c(TranslateFunction::kYAxis, property_c);

  EXPECT_FALSE(function_a.Equals(function_b));
  EXPECT_FALSE(function_b.Equals(function_c));
}

TEST(PropertyValueIsEqualTest, TransformListsAreEqual) {
  TransformFunctionListValue::Builder transform_list_a;
  transform_list_a.emplace_back(new TranslateFunction(
      TranslateFunction::kXAxis, new LengthValue(1, kPixelsUnit)));
  transform_list_a.emplace_back(new ScaleFunction(2.0f, 2.0f));
  transform_list_a.emplace_back(new RotateFunction(1.0f));
  transform_list_a.emplace_back(new CobaltUiNavSpotlightTransformFunction);
  scoped_refptr<TransformFunctionListValue> value_a(
      new TransformFunctionListValue(std::move(transform_list_a)));

  TransformFunctionListValue::Builder transform_list_b;
  transform_list_b.emplace_back(new TranslateFunction(
      TranslateFunction::kXAxis, new LengthValue(1, kPixelsUnit)));
  transform_list_b.emplace_back(new ScaleFunction(2.0f, 2.0f));
  transform_list_b.emplace_back(new RotateFunction(1.0f));
  transform_list_b.emplace_back(new CobaltUiNavSpotlightTransformFunction);
  scoped_refptr<TransformFunctionListValue> value_b(
      new TransformFunctionListValue(std::move(transform_list_b)));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, TransformListsAreNotEqual) {
  TransformFunctionListValue::Builder transform_list_a;
  transform_list_a.emplace_back(new TranslateFunction(
      TranslateFunction::kXAxis, new LengthValue(1, kPixelsUnit)));
  transform_list_a.emplace_back(new ScaleFunction(2.0f, 2.0f));
  transform_list_a.emplace_back(new RotateFunction(1.0f));
  scoped_refptr<TransformFunctionListValue> value_a(
      new TransformFunctionListValue(std::move(transform_list_a)));

  TransformFunctionListValue::Builder transform_list_b;
  transform_list_b.emplace_back(new TranslateFunction(
      TranslateFunction::kXAxis, new LengthValue(1, kPixelsUnit)));
  transform_list_b.emplace_back(new ScaleFunction(1.0f, 2.0f));
  transform_list_b.emplace_back(new RotateFunction(1.0f));
  scoped_refptr<TransformFunctionListValue> value_b(
      new TransformFunctionListValue(std::move(transform_list_b)));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, URLsAreEqual) {
  std::string url_a("youtube/video.webm");
  scoped_refptr<URLValue> value_a(new URLValue(url_a));
  std::string url_b("youtube/video.webm");
  scoped_refptr<URLValue> value_b(new URLValue(url_b));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, URLsAreNotEqual) {
  std::string url_a("youtube/video.webm");
  scoped_refptr<URLValue> value_a(new URLValue(url_a));
  std::string url_b("google/search.png");
  scoped_refptr<URLValue> value_b(new URLValue(url_b));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, FilterListsAreEqual) {
  FilterFunctionListValue::Builder filter_list_a;
  filter_list_a.emplace_back(new MapToMeshFunction(
      new URLValue("somemesh.msh"),
      MapToMeshFunction::ResolutionMatchedMeshListBuilder(), 0.70707f, 6.28f,
      glm::mat4(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 9.0f, 0.0f, 0.0f,
                1.0f, 0.07878f, 0.0f, 0.0f, 0.0f, 1.0f),
      KeywordValue::GetMonoscopic()));
  filter_list_a.emplace_back(new MapToMeshFunction(
      new URLValue("sphere.msh"),
      MapToMeshFunction::ResolutionMatchedMeshListBuilder(), 0.676f, 6.28f,
      glm::mat4(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f),
      KeywordValue::GetStereoscopicLeftRight()));
  filter_list_a.emplace_back(new MapToMeshFunction(
      MapToMeshFunction::kEquirectangular, 0.676f, 6.28f,
      glm::mat4(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f),
      KeywordValue::GetStereoscopicLeftRight()));
  scoped_refptr<FilterFunctionListValue> value_a(
      new FilterFunctionListValue(std::move(filter_list_a)));

  FilterFunctionListValue::Builder filter_list_b;
  filter_list_b.emplace_back(new MapToMeshFunction(
      new URLValue("somemesh.msh"),
      MapToMeshFunction::ResolutionMatchedMeshListBuilder(), 0.70707f, 6.28f,
      glm::mat4(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 9.0f, 0.0f, 0.0f,
                1.0f, 0.07878f, 0.0f, 0.0f, 0.0f, 1.0f),
      KeywordValue::GetMonoscopic()));
  filter_list_b.emplace_back(new MapToMeshFunction(
      new URLValue("sphere.msh"),
      MapToMeshFunction::ResolutionMatchedMeshListBuilder(), 0.676f, 6.28f,
      glm::mat4(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f),
      KeywordValue::GetStereoscopicLeftRight()));
  filter_list_b.emplace_back(new MapToMeshFunction(
      MapToMeshFunction::kEquirectangular, 0.676f, 6.28f,
      glm::mat4(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f),
      KeywordValue::GetStereoscopicLeftRight()));
  scoped_refptr<FilterFunctionListValue> value_b(
      new FilterFunctionListValue(std::move(filter_list_b)));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, FilterListsAreNotEqual) {
  FilterFunctionListValue::Builder filter_list_a;
  filter_list_a.emplace_back(new MapToMeshFunction(
      new URLValue("format.msh"),
      MapToMeshFunction::ResolutionMatchedMeshListBuilder(), 8.5f, 3.14f,
      glm::mat4(1.0f, 0.0f, 0.0f, 2.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f),
      KeywordValue::GetMonoscopic()));
  scoped_refptr<FilterFunctionListValue> value_a(
      new FilterFunctionListValue(std::move(filter_list_a)));

  FilterFunctionListValue::Builder filter_list_b;
  filter_list_b.emplace_back(new MapToMeshFunction(
      new URLValue("format.msh"),
      MapToMeshFunction::ResolutionMatchedMeshListBuilder(), 8.5f, 3.14f,
      glm::mat4(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f),
      KeywordValue::GetMonoscopic()));
  scoped_refptr<FilterFunctionListValue> value_b(
      new FilterFunctionListValue(std::move(filter_list_b)));

  EXPECT_FALSE(value_a->Equals(*value_b));

  FilterFunctionListValue::Builder filter_list_c;
  filter_list_c.emplace_back(new MapToMeshFunction(
      MapToMeshFunction::kEquirectangular, 8.5f, 3.14f,
      glm::mat4(1.0f, 0.0f, 0.0f, 2.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f),
      KeywordValue::GetMonoscopic()));
  scoped_refptr<FilterFunctionListValue> value_c(
      new FilterFunctionListValue(std::move(filter_list_c)));

  FilterFunctionListValue::Builder filter_list_d;
  filter_list_d.emplace_back(new MapToMeshFunction(
      new URLValue("format.msh"),
      MapToMeshFunction::ResolutionMatchedMeshListBuilder(), 8.5f, 3.14f,
      glm::mat4(1.0f, 0.0f, 0.0f, 2.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f),
      KeywordValue::GetMonoscopic()));
  scoped_refptr<FilterFunctionListValue> value_d(
      new FilterFunctionListValue(std::move(filter_list_d)));

  EXPECT_FALSE(value_c->Equals(*value_d));
}

}  // namespace cssom
}  // namespace cobalt

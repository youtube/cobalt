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

#include "cobalt/cssom/property_value.h"

#include "base/time.h"
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/font_style_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/integer_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/linear_gradient_value.h"
#include "cobalt/cssom/matrix_function.h"
#include "cobalt/cssom/media_feature_keyword_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/property_key_list_value.h"
#include "cobalt/cssom/property_list_value.h"
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
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(PropertyValueToStringTest, AbsoluteURLValue) {
  GURL url("https://www.youtube.com");
  scoped_refptr<AbsoluteURLValue> property(new AbsoluteURLValue(url));
  EXPECT_EQ(property->ToString(), "url(https://www.youtube.com/)");
}

TEST(PropertyValueToStringTest, FontStyleValue) {
  scoped_refptr<FontStyleValue> property = FontStyleValue::GetItalic();
  EXPECT_EQ(property->ToString(), "italic");
}

TEST(PropertyValueToStringTest, FontWeightValue) {
  scoped_refptr<FontWeightValue> property = FontWeightValue::GetBoldAka700();
  EXPECT_EQ(property->ToString(), "bold");
}

TEST(PropertyValueToStringTest, IntegerValue) {
  scoped_refptr<IntegerValue> property(new IntegerValue(1024));
  EXPECT_EQ(property->ToString(), "1024");
}

TEST(PropertyValueToStringTest, IntegerValueNegative) {
  scoped_refptr<IntegerValue> property(new IntegerValue(-512));
  EXPECT_EQ(property->ToString(), "-512");
}

TEST(PropertyValueToStringTest, KeywordValue) {
  scoped_refptr<KeywordValue> property = KeywordValue::GetHidden();
  EXPECT_EQ(property->ToString(), "hidden");
}

TEST(PropertyValueToStringTest, LengthValuePx) {
  scoped_refptr<LengthValue> property(new LengthValue(128, kPixelsUnit));
  EXPECT_EQ(property->ToString(), "128px");
}

TEST(PropertyValueToStringTest, LengthValueEm) {
  scoped_refptr<LengthValue> property =
      new LengthValue(1.5f, kFontSizesAkaEmUnit);
  EXPECT_EQ(property->ToString(), "1.5em");
}

TEST(PropertyValueToStringTest, MediaFeatureKeywordValue) {
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetProgressive();
  EXPECT_EQ(property->ToString(), "progressive");
}

TEST(PropertyValueToStringTest, NumberValue) {
  scoped_refptr<NumberValue> property(new NumberValue(123.0f));
  EXPECT_EQ(property->ToString(), "123");
}

TEST(PropertyValueToStringTest, NumberValueNegative) {
  scoped_refptr<NumberValue> property(new NumberValue(-456.0f));
  EXPECT_EQ(property->ToString(), "-456");
}

TEST(PropertyValueToStringTest, NumberValueDecimal) {
  scoped_refptr<NumberValue> property(new NumberValue(123.45f));
  EXPECT_EQ(property->ToString(), "123.45");
}

TEST(PropertyValueToStringTest, PercentageValue0) {
  scoped_refptr<PercentageValue> property(new PercentageValue(0.0f));
  EXPECT_EQ(property->ToString(), "0%");
}

TEST(PropertyValueToStringTest, PercentageValue50) {
  scoped_refptr<PercentageValue> property(new PercentageValue(0.50f));
  EXPECT_EQ(property->ToString(), "50%");
}

TEST(PropertyValueToStringTest, PercentageValue100) {
  scoped_refptr<PercentageValue> property(new PercentageValue(1.0f));
  EXPECT_EQ(property->ToString(), "100%");
}

TEST(PropertyValueToStringTest, PercentageValueDecimal) {
  scoped_refptr<PercentageValue> property(new PercentageValue(0.505f));
  EXPECT_EQ(property->ToString(), "50.5%");
}

TEST(PropertyValueToStringTest, PropertyListValue) {
  scoped_ptr<PropertyListValue::Builder> property_value_builder(
      new PropertyListValue::Builder());
  property_value_builder->push_back(new PercentageValue(0.50f));
  property_value_builder->push_back(new LengthValue(128, kPixelsUnit));
  scoped_refptr<PropertyListValue> property(
      new PropertyListValue(property_value_builder.Pass()));

  EXPECT_EQ(property->ToString(), "50%, 128px");
}

TEST(PropertyValueToStringTest, PropertyKeyListValue) {
  PropertyKeyListValue::Builder property_list;
  property_list.push_back(kBackgroundColorProperty);
  property_list.push_back(kOpacityProperty);
  scoped_refptr<PropertyKeyListValue> property(new PropertyKeyListValue(
      make_scoped_ptr(new PropertyKeyListValue::Builder(property_list))));

  EXPECT_EQ(property->ToString(), "background-color, opacity");
}

TEST(PropertyValueToStringTest, RadialGradientValueSizeKeyword) {
  ColorStopList color_stop_list;
  scoped_refptr<RGBAColorValue> property_color_1(
      new RGBAColorValue(100, 0, 50, 255));
  scoped_refptr<LengthValue> property_length_1(
      new LengthValue(212, kPixelsUnit));
  color_stop_list.push_back(new ColorStop(property_color_1, property_length_1));
  scoped_refptr<RGBAColorValue> property_color_2(
      new RGBAColorValue(55, 66, 77, 255));
  scoped_refptr<LengthValue> property_length_2(
      new LengthValue(42, kPixelsUnit));
  color_stop_list.push_back(new ColorStop(property_color_2, property_length_2));

  scoped_ptr<PropertyListValue::Builder> position_value_builder(
      new PropertyListValue::Builder());
  position_value_builder->push_back(new PercentageValue(0.50f));
  position_value_builder->push_back(new LengthValue(128, kPixelsUnit));
  scoped_refptr<PropertyListValue> position_property(
      new PropertyListValue(position_value_builder.Pass()));

  scoped_refptr<RadialGradientValue> property(new RadialGradientValue(
      RadialGradientValue::kEllipse, RadialGradientValue::kClosestCorner,
      position_property, color_stop_list.Pass()));

  EXPECT_EQ(property->ToString(),
            "ellipse closest-corner at 50% 128px, rgb(100, 0, 50) 212px, "
            "rgb(55, 66, 77) 42px");
}

TEST(PropertyValueToStringTest, RadialGradientValueSizeValue) {
  ColorStopList color_stop_list;
  scoped_refptr<RGBAColorValue> property_color_1(
      new RGBAColorValue(100, 0, 50, 255));
  scoped_refptr<LengthValue> property_length_1(
      new LengthValue(212, kPixelsUnit));
  color_stop_list.push_back(new ColorStop(property_color_1, property_length_1));
  scoped_refptr<RGBAColorValue> property_color_2(
      new RGBAColorValue(55, 66, 77, 255));
  scoped_refptr<LengthValue> property_length_2(
      new LengthValue(42, kPixelsUnit));
  color_stop_list.push_back(new ColorStop(property_color_2, property_length_2));

  scoped_ptr<PropertyListValue::Builder> size_value_builder(
      new PropertyListValue::Builder());
  size_value_builder->push_back(new LengthValue(0.5, kFontSizesAkaEmUnit));
  scoped_refptr<PropertyListValue> size_property(
      new PropertyListValue(size_value_builder.Pass()));

  scoped_ptr<PropertyListValue::Builder> position_value_builder(
      new PropertyListValue::Builder());
  position_value_builder->push_back(KeywordValue::GetCenter());
  position_value_builder->push_back(KeywordValue::GetTop());
  scoped_refptr<PropertyListValue> position_property(
      new PropertyListValue(position_value_builder.Pass()));

  scoped_refptr<RadialGradientValue> property(
      new RadialGradientValue(RadialGradientValue::kCircle, size_property,
                              position_property, color_stop_list.Pass()));

  EXPECT_EQ(property->ToString(),
            "circle 0.5em at center top, rgb(100, 0, 50) 212px, rgb(55, 66, "
            "77) 42px");
}

TEST(PropertyValueToStringTest, RatioValue) {
  scoped_refptr<RatioValue> property(new RatioValue(math::Rational(16, 9)));
  EXPECT_EQ(property->ToString(), "16/9");
}

TEST(PropertyValueToStringTest, ResolutionValue) {
  scoped_refptr<ResolutionValue> property(
      new ResolutionValue(100.0f, kDPIUnit));
  EXPECT_EQ(property->ToString(), "100dpi");
}

TEST(PropertyValueToStringTest, RGBColorValue) {
  scoped_refptr<RGBAColorValue> property(new RGBAColorValue(123, 99, 255, 255));
  EXPECT_EQ(property->ToString(), "rgb(123, 99, 255)");
}

TEST(PropertyValueToStringTest, RGBAColorValue) {
  scoped_refptr<RGBAColorValue> property(new RGBAColorValue(53, 35, 192, 64));
  // Note: The alpha value of 64 specifies an alpha of 64/255 = 0.2509804.
  EXPECT_EQ(property->ToString(), "rgba(53, 35, 192, 0.2509804)");
}

TEST(PropertyValueToStringTest, LinearGradientValueAngle) {
  ColorStopList color_stop_list;
  scoped_refptr<RGBAColorValue> property_color_1(
      new RGBAColorValue(100, 0, 50, 255));
  scoped_refptr<LengthValue> property_length_1(
      new LengthValue(212, kPixelsUnit));
  color_stop_list.push_back(new ColorStop(property_color_1, property_length_1));
  scoped_refptr<RGBAColorValue> property_color_2(
      new RGBAColorValue(55, 66, 77, 255));
  scoped_refptr<LengthValue> property_length_2(
      new LengthValue(42, kPixelsUnit));
  color_stop_list.push_back(new ColorStop(property_color_2, property_length_2));

  scoped_refptr<LinearGradientValue> property(
      new LinearGradientValue(123.0f, color_stop_list.Pass()));

  EXPECT_EQ(property->ToString(),
            "123rad, rgb(100, 0, 50) 212px, rgb(55, 66, 77) 42px");
}

TEST(PropertyValueToStringTest, LinearGradientValueSideOrCorner) {
  ColorStopList color_stop_list;
  scoped_refptr<RGBAColorValue> property_color_1(
      new RGBAColorValue(100, 0, 50, 255));
  scoped_refptr<LengthValue> property_length_1(
      new LengthValue(212, kPixelsUnit));
  color_stop_list.push_back(new ColorStop(property_color_1, property_length_1));
  scoped_refptr<RGBAColorValue> property_color_2(
      new RGBAColorValue(55, 66, 77, 255));
  scoped_refptr<LengthValue> property_length_2(
      new LengthValue(42, kPixelsUnit));
  color_stop_list.push_back(new ColorStop(property_color_2, property_length_2));

  scoped_refptr<LinearGradientValue> property(new LinearGradientValue(
      LinearGradientValue::kBottom, color_stop_list.Pass()));

  EXPECT_EQ(property->ToString(),
            "to bottom, rgb(100, 0, 50) 212px, rgb(55, 66, 77) 42px");
}

TEST(PropertyValueToStringTest, StringValue) {
  scoped_refptr<StringValue> property(new StringValue("cobalt"));
  EXPECT_EQ(property->ToString(), "'cobalt'");
}

TEST(PropertyValueToStringTest, TimeListValue) {
  TimeListValue::Builder time_list;
  time_list.push_back(base::TimeDelta::FromSeconds(3));
  time_list.push_back(base::TimeDelta::FromMilliseconds(14));

  scoped_refptr<TimeListValue> property(new TimeListValue(
      make_scoped_ptr(new TimeListValue::Builder(time_list))));
  EXPECT_EQ(property->ToString(), "3s, 14ms");
}

TEST(PropertyValueToStringTest, TimingFunctionListValue) {
  scoped_ptr<TimingFunctionListValue::Builder> timing_function_list(
      new TimingFunctionListValue::Builder());
  timing_function_list->push_back(
      new CubicBezierTimingFunction(0.0f, 0.5f, 1.0f, 1.0f));
  timing_function_list->push_back(
      new SteppingTimingFunction(3, SteppingTimingFunction::kEnd));
  scoped_refptr<TimingFunctionListValue> property(
      new TimingFunctionListValue(timing_function_list.Pass()));
  EXPECT_EQ(property->ToString(), "cubic-bezier(0,0.5,1,1), steps(3, end)");
}

TEST(PropertyValueToStringTest, MatrixFunction) {
  MatrixFunction function(0.0f, 1.0f, 0.1f, 1.1f, 0.2f, 1.2f);
  EXPECT_EQ(function.ToString(), "matrix(0, 1, 0.1, 1.1, 0.2, 1.2)");
}

TEST(PropertyValueToStringTest, RotateFunction) {
  RotateFunction function(3.1415f);
  EXPECT_EQ(function.ToString(), "rotate(3.1415rad)");
}

TEST(PropertyValueToStringTest, ScaleFunction) {
  ScaleFunction function(12.34f, 56.78f);
  EXPECT_EQ(function.ToString(), "scale(12.34, 56.78)");
}

TEST(PropertyValueToStringTest, TranslateFunction) {
  scoped_refptr<LengthValue> property(new LengthValue(3, kFontSizesAkaEmUnit));
  TranslateFunction function(TranslateFunction::kYAxis, property);
  EXPECT_EQ(function.ToString(), "translateY(3em)");
}

TEST(PropertyValueToStringTest, TransformFunctionListValue) {
  TransformFunctionListValue::Builder transform_list;
  transform_list.push_back(new TranslateFunction(
      TranslateFunction::kXAxis, new LengthValue(1, kPixelsUnit)));
  transform_list.push_back(new ScaleFunction(2.0f, 2.0f));
  transform_list.push_back(new RotateFunction(1.0f));
  scoped_refptr<TransformFunctionListValue> property(
      new TransformFunctionListValue(transform_list.Pass()));

  EXPECT_EQ(property->ToString(), "translateX(1px) scale(2, 2) rotate(1rad)");
}

TEST(PropertyValueToStringTest, URLValue) {
  scoped_refptr<URLValue> property(new URLValue("foo.png"));
  EXPECT_EQ(property->ToString(), "url(foo.png)");
}

}  // namespace cssom
}  // namespace cobalt

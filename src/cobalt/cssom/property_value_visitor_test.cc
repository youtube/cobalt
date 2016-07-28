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

#include "cobalt/cssom/property_value_visitor.h"

#include <vector>

#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/calc_value.h"
#include "cobalt/cssom/font_style_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/integer_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/linear_gradient_value.h"
#include "cobalt/cssom/local_src_value.h"
#include "cobalt/cssom/media_feature_keyword_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_key_list_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/radial_gradient_value.h"
#include "cobalt/cssom/ratio_value.h"
#include "cobalt/cssom/resolution_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/shadow_value.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/time_list_value.h"
#include "cobalt/cssom/timing_function.h"
#include "cobalt/cssom/timing_function_list_value.h"
#include "cobalt/cssom/transform_function.h"
#include "cobalt/cssom/transform_function_list_value.h"
#include "cobalt/cssom/transform_matrix_function_value.h"
#include "cobalt/cssom/unicode_range_value.h"
#include "cobalt/cssom/url_src_value.h"
#include "cobalt/cssom/url_value.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

class MockPropertyValueVisitor : public PropertyValueVisitor {
 public:
  MOCK_METHOD1(VisitAbsoluteURL, void(AbsoluteURLValue* absolute_url_value));
  MOCK_METHOD1(VisitCalc, void(CalcValue* calc_value));
  MOCK_METHOD1(VisitFontStyle, void(FontStyleValue* font_style_value));
  MOCK_METHOD1(VisitFontWeight, void(FontWeightValue* font_weight_value));
  MOCK_METHOD1(VisitInteger, void(IntegerValue* integer_value));
  MOCK_METHOD1(VisitKeyword, void(KeywordValue* keyword_value));
  MOCK_METHOD1(VisitLength, void(LengthValue* length_value));
  MOCK_METHOD1(VisitLinearGradient,
               void(LinearGradientValue* linear_gradient_value));
  MOCK_METHOD1(VisitList, void(TimeListValue* time_list_value));
  MOCK_METHOD1(VisitLocalSrc, void(LocalSrcValue* local_src_value));
  MOCK_METHOD1(VisitMediaFeatureKeywordValue,
               void(MediaFeatureKeywordValue* media_feature_keyword_value));
  MOCK_METHOD1(VisitNumber, void(NumberValue* number_value));
  MOCK_METHOD1(VisitPercentage, void(PercentageValue* percentage_value));
  MOCK_METHOD1(VisitPropertyKeyList,
               void(PropertyKeyListValue* property_key_list_value));
  MOCK_METHOD1(VisitPropertyList, void(PropertyListValue* property_list_value));
  MOCK_METHOD1(VisitRadialGradient,
               void(RadialGradientValue* radial_gradient_value));
  MOCK_METHOD1(VisitRatio, void(RatioValue* ratio_value));
  MOCK_METHOD1(VisitResolution, void(ResolutionValue* resolution_value));
  MOCK_METHOD1(VisitRGBAColor, void(RGBAColorValue* color_value));
  MOCK_METHOD1(VisitShadow, void(ShadowValue* shadow_value));
  MOCK_METHOD1(VisitString, void(StringValue* string_value));
  MOCK_METHOD1(VisitTimeList, void(TimeListValue* time_list_value));
  MOCK_METHOD1(VisitTimingFunctionList,
               void(TimingFunctionListValue* timing_function_list_value));
  MOCK_METHOD1(VisitTransformFunctionList,
               void(TransformFunctionListValue* transform_list_value));
  MOCK_METHOD1(
      VisitTransformMatrixFunction,
      void(TransformMatrixFunctionValue* transform_matrix_function_value));
  MOCK_METHOD1(VisitUnicodeRange, void(UnicodeRangeValue* unicode_range_value));
  MOCK_METHOD1(VisitURL, void(URLValue* url_value));
  MOCK_METHOD1(VisitUrlSrc, void(UrlSrcValue* url_src_value));
};

TEST(PropertyValueVisitorTest, VisitsAbsoluteURLValue) {
  scoped_refptr<AbsoluteURLValue> url_absolute_value =
      new AbsoluteURLValue(GURL("file:///sample.png"));
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitAbsoluteURL(url_absolute_value.get()));
  url_absolute_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsCalcValue) {
  scoped_refptr<CalcValue> calc_value = new CalcValue(
      new LengthValue(50.0f, kPixelsUnit), new PercentageValue(0.2f));
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitCalc(calc_value.get()));
  calc_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsFontStyleValue) {
  scoped_refptr<FontStyleValue> font_style_value = FontStyleValue::GetNormal();
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitFontStyle(font_style_value.get()));
  font_style_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsFontWeightValue) {
  scoped_refptr<FontWeightValue> font_weight_value =
      FontWeightValue::GetNormalAka400();
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitFontWeight(font_weight_value.get()));
  font_weight_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsIntegerValue) {
  scoped_refptr<IntegerValue> integer_value = new IntegerValue(101);
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitInteger(integer_value.get()));
  integer_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsKeywordValue) {
  scoped_refptr<KeywordValue> inherit_value = KeywordValue::GetInherit();
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitKeyword(inherit_value.get()));
  inherit_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsLinearGradientValue) {
  scoped_refptr<LinearGradientValue> linear_gradient_value =
      new LinearGradientValue(LinearGradientValue::kTopLeft,
                              ScopedVector<ColorStop>());
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitLinearGradient(linear_gradient_value.get()));
  linear_gradient_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsLengthValue) {
  scoped_refptr<LengthValue> length_value = new LengthValue(10, kPixelsUnit);
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitLength(length_value.get()));
  length_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsLocalSrcValue) {
  scoped_refptr<LocalSrcValue> local_src_value = new LocalSrcValue("Gentium");
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitLocalSrc(local_src_value.get()));
  local_src_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsMediaFeatureKeywordValue) {
  scoped_refptr<MediaFeatureKeywordValue> landscape_value =
      MediaFeatureKeywordValue::GetLandscape();
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor,
              VisitMediaFeatureKeywordValue(landscape_value.get()));
  landscape_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsNumberValue) {
  scoped_refptr<NumberValue> number_value = new NumberValue(299792458.0f);
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitNumber(number_value.get()));
  number_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsPercentageValue) {
  scoped_refptr<PercentageValue> percentage_value = new PercentageValue(1);
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitPercentage(percentage_value.get()));
  percentage_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsPropertyKeyListValue) {
  scoped_refptr<PropertyKeyListValue> property_key_list_value =
      new PropertyKeyListValue(
          make_scoped_ptr(new PropertyKeyListValue::Builder()));
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor,
              VisitPropertyKeyList(property_key_list_value.get()));
  property_key_list_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsPropertyListValue) {
  scoped_refptr<PropertyListValue> property_list_value = new PropertyListValue(
      make_scoped_ptr(new ScopedRefListValue<PropertyValue>::Builder()));
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitPropertyList(property_list_value.get()));
  property_list_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsRadialGradientValue) {
  scoped_refptr<RadialGradientValue> radial_gradient_value =
      new RadialGradientValue(RadialGradientValue::kCircle,
                              RadialGradientValue::kClosestCorner, NULL,
                              ScopedVector<ColorStop>());
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitRadialGradient(radial_gradient_value.get()));
  radial_gradient_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsRatioValue) {
  scoped_refptr<RatioValue> ratio_value =
      new RatioValue(math::Rational(1280, 720));
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitRatio(ratio_value.get()));
  ratio_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsResolutionValue) {
  scoped_refptr<ResolutionValue> resolution_value =
      new ResolutionValue(300, kDPIUnit);
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitResolution(resolution_value.get()));
  resolution_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsRGBAColorValue) {
  scoped_refptr<RGBAColorValue> color_value = new RGBAColorValue(0x0047abff);
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitRGBAColor(color_value.get()));
  color_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsShadowValue) {
  scoped_refptr<ShadowValue> shadow_value =
      new ShadowValue(std::vector<scoped_refptr<LengthValue> >(),
                      new RGBAColorValue(0x0047abff), false);
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitShadow(shadow_value.get()));
  shadow_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsStringValue) {
  scoped_refptr<StringValue> string_value = new StringValue("Roboto");
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitString(string_value.get()));
  string_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsTimeListValue) {
  scoped_refptr<TimeListValue> time_list_value =
      new TimeListValue(make_scoped_ptr(new TimeListValue::Builder()));
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitTimeList(time_list_value.get()));
  time_list_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsTimingFunctionListValue) {
  scoped_refptr<TimingFunctionListValue> timing_function_list_value =
      new TimingFunctionListValue(
          make_scoped_ptr(new TimingFunctionListValue::Builder()));
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor,
              VisitTimingFunctionList(timing_function_list_value.get()));
  timing_function_list_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsTransformListValue) {
  scoped_refptr<TransformFunctionListValue> transform_list_value =
      new TransformFunctionListValue(TransformFunctionListValue::Builder());
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor,
              VisitTransformFunctionList(transform_list_value.get()));
  transform_list_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsTransformMatrixFunctionValue) {
  scoped_refptr<TransformMatrixFunctionValue> transform_matrix_function_value =
      new TransformMatrixFunctionValue(TransformMatrix());
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitTransformMatrixFunction(
                                transform_matrix_function_value.get()));
  transform_matrix_function_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsUnicodeRangeValue) {
  scoped_refptr<UnicodeRangeValue> unicode_range_value =
      new UnicodeRangeValue(0, 0x10FFFF);
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitUnicodeRange(unicode_range_value.get()));
  unicode_range_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsURLValue) {
  scoped_refptr<URLValue> url_value = new URLValue("");
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitURL(url_value.get()));
  url_value->Accept(&mock_visitor);
}

TEST(PropertyValueVisitorTest, VisitsUrlSrcValue) {
  scoped_refptr<UrlSrcValue> url_src_value =
      new UrlSrcValue(new URLValue(""), "'Roboto'");
  MockPropertyValueVisitor mock_visitor;
  EXPECT_CALL(mock_visitor, VisitUrlSrc(url_src_value.get()));
  url_src_value->Accept(&mock_visitor);
}

}  // namespace cssom
}  // namespace cobalt

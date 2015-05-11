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

#include "cobalt/cssom/const_string_list_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/property_names.h"
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

TEST(PropertyValueIsEqualTest, LengthsAreEqual) {
  scoped_refptr<LengthValue> value_a(
      new LengthValue(1.5f, kPixelsUnit));
  scoped_refptr<LengthValue> value_b(
      new LengthValue(1.5f, kPixelsUnit));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, LengthsAreNotEqual) {
  scoped_refptr<LengthValue> value_a(
      new LengthValue(1.5f, kPixelsUnit));
  scoped_refptr<LengthValue> value_b(
      new LengthValue(1.5f, kFontSizesAkaEmUnit));

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

TEST(PropertyValueIsEqualTest, PropertyNameListsAreEqual) {
  ConstStringListValue::Builder property_list;
  property_list.push_back(kBackgroundColorPropertyName);
  property_list.push_back(kOpacityPropertyName);
  scoped_refptr<ConstStringListValue> value_a(new ConstStringListValue(
      make_scoped_ptr(new ConstStringListValue::Builder(property_list))));
  scoped_refptr<ConstStringListValue> value_b(new ConstStringListValue(
      make_scoped_ptr(new ConstStringListValue::Builder(property_list))));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, PropertyNameListsAreNotEqual) {
  ConstStringListValue::Builder property_list;
  property_list.push_back(kBackgroundColorPropertyName);
  property_list.push_back(kOpacityPropertyName);
  scoped_refptr<ConstStringListValue> value_a(new ConstStringListValue(
      make_scoped_ptr(new ConstStringListValue::Builder(property_list))));
  property_list.back() = kTransformPropertyName;
  scoped_refptr<ConstStringListValue> value_b(new ConstStringListValue(
      make_scoped_ptr(new ConstStringListValue::Builder(property_list))));

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
      make_scoped_ptr(new TimeListValue::Builder(time_list))));
  scoped_refptr<TimeListValue> value_b(new TimeListValue(
      make_scoped_ptr(new TimeListValue::Builder(time_list))));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, TimeListsAreNotEqual) {
  TimeListValue::Builder time_list;
  time_list.push_back(base::TimeDelta::FromSeconds(3));
  time_list.push_back(base::TimeDelta::FromMilliseconds(2));

  scoped_refptr<TimeListValue> value_a(new TimeListValue(
      make_scoped_ptr(new TimeListValue::Builder(time_list))));

  time_list.back() = base::TimeDelta::FromSeconds(2);

  scoped_refptr<TimeListValue> value_b(new TimeListValue(
      make_scoped_ptr(new TimeListValue::Builder(time_list))));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, TimingFunctionListsAreEqual) {
  scoped_ptr<TimingFunctionListValue::Builder>
      timing_function_list_a(new TimingFunctionListValue::Builder());
  timing_function_list_a->push_back(TimingFunction::GetLinear());
  timing_function_list_a->push_back(
      new SteppingTimingFunction(3, SteppingTimingFunction::kEnd));
  scoped_refptr<TimingFunctionListValue> value_a(
      new TimingFunctionListValue(timing_function_list_a.Pass()));

  scoped_ptr<TimingFunctionListValue::Builder>
      timing_function_list_b(new TimingFunctionListValue::Builder());
  timing_function_list_b->push_back(
      new CubicBezierTimingFunction(0.0f, 0.0f, 1.0f, 1.0f));
  timing_function_list_b->push_back(
      new SteppingTimingFunction(3, SteppingTimingFunction::kEnd));
  scoped_refptr<TimingFunctionListValue> value_b(
      new TimingFunctionListValue(timing_function_list_b.Pass()));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, TimingFunctionListsAreNotEqual) {
  scoped_ptr<TimingFunctionListValue::Builder>
      timing_function_list_a(new TimingFunctionListValue::Builder());
  timing_function_list_a->push_back(TimingFunction::GetLinear());
  timing_function_list_a->push_back(
      new SteppingTimingFunction(3, SteppingTimingFunction::kEnd));
  scoped_refptr<TimingFunctionListValue> value_a(
      new TimingFunctionListValue(timing_function_list_a.Pass()));

  scoped_ptr<TimingFunctionListValue::Builder>
      timing_function_list_b(new TimingFunctionListValue::Builder());
  timing_function_list_b->push_back(
      new CubicBezierTimingFunction(0.0f, 0.5f, 1.0f, 1.0f));
  timing_function_list_b->push_back(
      new SteppingTimingFunction(3, SteppingTimingFunction::kEnd));
  scoped_refptr<TimingFunctionListValue> value_b(
      new TimingFunctionListValue(timing_function_list_b.Pass()));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, TransformListsAreEqual) {
  TransformFunctionListValue::Builder transform_list_a;
  transform_list_a.push_back(
      new TranslateXFunction(new LengthValue(1, kPixelsUnit)));
  transform_list_a.push_back(new ScaleFunction(2.0f, 2.0f));
  transform_list_a.push_back(new RotateFunction(1.0f));
  scoped_refptr<TransformFunctionListValue> value_a(
      new TransformFunctionListValue(transform_list_a.Pass()));

  TransformFunctionListValue::Builder transform_list_b;
  transform_list_b.push_back(
      new TranslateXFunction(new LengthValue(1, kPixelsUnit)));
  transform_list_b.push_back(new ScaleFunction(2.0f, 2.0f));
  transform_list_b.push_back(new RotateFunction(1.0f));
  scoped_refptr<TransformFunctionListValue> value_b(
      new TransformFunctionListValue(transform_list_b.Pass()));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, TransformListsAreNotEqual) {
  TransformFunctionListValue::Builder transform_list_a;
  transform_list_a.push_back(
      new TranslateXFunction(new LengthValue(1, kPixelsUnit)));
  transform_list_a.push_back(new ScaleFunction(2.0f, 2.0f));
  transform_list_a.push_back(new RotateFunction(1.0f));
  scoped_refptr<TransformFunctionListValue> value_a(
      new TransformFunctionListValue(transform_list_a.Pass()));

  TransformFunctionListValue::Builder transform_list_b;
  transform_list_b.push_back(
      new TranslateXFunction(new LengthValue(1, kPixelsUnit)));
  transform_list_b.push_back(new ScaleFunction(1.0f, 2.0f));
  transform_list_b.push_back(new RotateFunction(1.0f));
  scoped_refptr<TransformFunctionListValue> value_b(
      new TransformFunctionListValue(transform_list_b.Pass()));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

}  // namespace cssom
}  // namespace cobalt

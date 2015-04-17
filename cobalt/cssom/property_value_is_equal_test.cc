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

#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/property_name_list_value.h"
#include "cobalt/cssom/property_names.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/rotate_function.h"
#include "cobalt/cssom/scale_function.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/time_list_value.h"
#include "cobalt/cssom/transform_function.h"
#include "cobalt/cssom/transform_list_value.h"
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
  PropertyNameListValue::PropertyNameList property_list;
  property_list.push_back(kBackgroundColorPropertyName);
  property_list.push_back(kOpacityPropertyName);
  scoped_refptr<PropertyNameListValue> value_a(
      new PropertyNameListValue(make_scoped_ptr(
          new PropertyNameListValue::PropertyNameList(property_list))));
  scoped_refptr<PropertyNameListValue> value_b(
      new PropertyNameListValue(make_scoped_ptr(
          new PropertyNameListValue::PropertyNameList(property_list))));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, PropertyNameListsAreNotEqual) {
  PropertyNameListValue::PropertyNameList property_list;
  property_list.push_back(kBackgroundColorPropertyName);
  property_list.push_back(kOpacityPropertyName);
  scoped_refptr<PropertyNameListValue> value_a(
      new PropertyNameListValue(make_scoped_ptr(
          new PropertyNameListValue::PropertyNameList(property_list))));
  property_list.back() = kTransformPropertyName;
  scoped_refptr<PropertyNameListValue> value_b(
      new PropertyNameListValue(make_scoped_ptr(
          new PropertyNameListValue::PropertyNameList(property_list))));

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
  TimeListValue::TimeList time_list;
  Time next_time;
  next_time.value = 3;
  next_time.unit = kSecondsUnit;
  time_list.push_back(next_time);
  next_time.value = 2;
  next_time.unit = kMillisecondsUnit;
  time_list.push_back(next_time);

  scoped_refptr<TimeListValue> value_a(new TimeListValue(make_scoped_ptr(
      new TimeListValue::TimeList(time_list))));
  scoped_refptr<TimeListValue> value_b(new TimeListValue(make_scoped_ptr(
      new TimeListValue::TimeList(time_list))));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, TimeListsAreNotEqual) {
  TimeListValue::TimeList time_list;
  Time next_time;
  next_time.value = 3;
  next_time.unit = kSecondsUnit;
  time_list.push_back(next_time);
  next_time.value = 2;
  next_time.unit = kMillisecondsUnit;
  time_list.push_back(next_time);

  scoped_refptr<TimeListValue> value_a(new TimeListValue(make_scoped_ptr(
      new TimeListValue::TimeList(time_list))));

  next_time.value = 2;
  next_time.unit = kSecondsUnit;
  time_list.back() = next_time;

  scoped_refptr<TimeListValue> value_b(new TimeListValue(make_scoped_ptr(
      new TimeListValue::TimeList(time_list))));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, TransformListsAreEqual) {
  TransformListValue::TransformFunctions transform_list_a;
  transform_list_a.push_back(
      new TranslateXFunction(new LengthValue(1, kPixelsUnit)));
  transform_list_a.push_back(new ScaleFunction(2.0f, 2.0f));
  transform_list_a.push_back(new RotateFunction(1.0f));
  scoped_refptr<TransformListValue> value_a(new TransformListValue(
      transform_list_a.Pass()));

  TransformListValue::TransformFunctions transform_list_b;
  transform_list_b.push_back(
      new TranslateXFunction(new LengthValue(1, kPixelsUnit)));
  transform_list_b.push_back(new ScaleFunction(2.0f, 2.0f));
  transform_list_b.push_back(new RotateFunction(1.0f));
  scoped_refptr<TransformListValue> value_b(new TransformListValue(
      transform_list_b.Pass()));

  EXPECT_TRUE(value_a->Equals(*value_b));
}

TEST(PropertyValueIsEqualTest, TransformListsAreNotEqual) {
  TransformListValue::TransformFunctions transform_list_a;
  transform_list_a.push_back(
      new TranslateXFunction(new LengthValue(1, kPixelsUnit)));
  transform_list_a.push_back(new ScaleFunction(2.0f, 2.0f));
  transform_list_a.push_back(new RotateFunction(1.0f));
  scoped_refptr<TransformListValue> value_a(new TransformListValue(
      transform_list_a.Pass()));

  TransformListValue::TransformFunctions transform_list_b;
  transform_list_b.push_back(
      new TranslateXFunction(new LengthValue(1, kPixelsUnit)));
  transform_list_b.push_back(new ScaleFunction(1.0f, 2.0f));
  transform_list_b.push_back(new RotateFunction(1.0f));
  scoped_refptr<TransformListValue> value_b(new TransformListValue(
      transform_list_b.Pass()));

  EXPECT_FALSE(value_a->Equals(*value_b));
}

}  // namespace cssom
}  // namespace cobalt

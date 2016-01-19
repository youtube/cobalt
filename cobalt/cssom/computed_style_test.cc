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

#include "cobalt/cssom/computed_style.h"

#include <vector>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/calc_value.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/shadow_value.h"
#include "cobalt/cssom/url_value.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(PromoteToComputedStyle, FontWeightShouldBeBoldAsSpecified) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_font_weight(cssom::FontWeightValue::GetBoldAka700());

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());

  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  EXPECT_EQ(cssom::FontWeightValue::GetBoldAka700(),
            computed_style->font_weight().get());
}

TEST(PromoteToComputedStyle, FontSizeInEmShouldBeRelativeToInheritedValue) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_font_size(
      new cssom::LengthValue(1.5f, cssom::kFontSizesAkaEmUnit));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->set_font_size(
      new cssom::LengthValue(100, cssom::kPixelsUnit));

  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  cssom::LengthValue* computed_font_size =
      base::polymorphic_downcast<cssom::LengthValue*>(
          computed_style->font_size().get());
  EXPECT_EQ(150, computed_font_size->value());
  EXPECT_EQ(cssom::kPixelsUnit, computed_font_size->unit());
}

TEST(PromoteToComputedStyle, FontSizeInPixelsShouldBeLeftAsSpecified) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_font_size(new cssom::LengthValue(50, cssom::kPixelsUnit));

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());

  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  cssom::LengthValue* computed_font_size =
      base::polymorphic_downcast<cssom::LengthValue*>(
          computed_style->font_size().get());
  EXPECT_EQ(50, computed_font_size->value());
  EXPECT_EQ(cssom::kPixelsUnit, computed_font_size->unit());
}

TEST(PromoteToComputedStyle, NormalLineHeightShouldBeLeftAsSpecified) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_line_height(cssom::KeywordValue::GetNormal());

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());

  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  EXPECT_EQ(cssom::KeywordValue::GetNormal(), computed_style->line_height());
}

TEST(PromoteToComputedStyle, LineHeightInEmShouldBeComputedAfterFontSize) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_font_size(
      new cssom::LengthValue(2, cssom::kFontSizesAkaEmUnit));
  computed_style->set_line_height(
      new cssom::LengthValue(1.5f, cssom::kFontSizesAkaEmUnit));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->set_font_size(
      new cssom::LengthValue(100, cssom::kPixelsUnit));

  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  cssom::LengthValue* computed_line_height =
      base::polymorphic_downcast<cssom::LengthValue*>(
          computed_style->line_height().get());
  EXPECT_EQ(300, computed_line_height->value());
  EXPECT_EQ(cssom::kPixelsUnit, computed_line_height->unit());
}

TEST(PromoteToComputedStyle, TextIndentInEmShouldBeComputedAfterFontSize) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_font_size(
      new cssom::LengthValue(2, cssom::kFontSizesAkaEmUnit));
  computed_style->set_text_indent(
      new cssom::LengthValue(1.5f, cssom::kFontSizesAkaEmUnit));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->set_font_size(
      new cssom::LengthValue(100, cssom::kPixelsUnit));

  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  cssom::LengthValue* computed_text_indent =
      base::polymorphic_downcast<cssom::LengthValue*>(
          computed_style->text_indent().get());
  EXPECT_EQ(300, computed_text_indent->value());
  EXPECT_EQ(cssom::kPixelsUnit, computed_text_indent->unit());
}

TEST(PromoteToComputedStyle, BackgroundImageRelativeURL) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  scoped_ptr<cssom::PropertyListValue::Builder> background_image_builder(
      new cssom::PropertyListValue::Builder());
  background_image_builder->push_back(
      new cssom::URLValue("../test/sample.png"));
  scoped_refptr<cssom::PropertyListValue> background_image(
      new cssom::PropertyListValue(background_image_builder.Pass()));
  computed_style->set_background_image(background_image);

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());

  GURLMap property_key_to_base_url_map;
  property_key_to_base_url_map[cssom::kBackgroundImageProperty] =
      GURL("file:///computed_style_test/style_sheet.css");

  PromoteToComputedStyle(computed_style, parent_computed_style,
                         &property_key_to_base_url_map);

  scoped_refptr<cssom::PropertyListValue> background_image_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->background_image().get());
  ASSERT_TRUE(background_image_list);
  ASSERT_EQ(1, background_image_list->value().size());

  GURL value = base::polymorphic_downcast<cssom::AbsoluteURLValue*>(
                   background_image_list->value()[0].get())->value();

  EXPECT_EQ("file:///test/sample.png", value.spec());
}

TEST(PromoteToComputedStyle, BackgroundImageNone) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  scoped_ptr<cssom::PropertyListValue::Builder> background_image_builder(
      new cssom::PropertyListValue::Builder());
  background_image_builder->push_back(cssom::KeywordValue::GetNone());
  scoped_refptr<cssom::PropertyListValue> background_image(
      new cssom::PropertyListValue(background_image_builder.Pass()));
  computed_style->set_background_image(background_image);

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());

  GURLMap property_key_to_base_url_map;
  property_key_to_base_url_map[cssom::kBackgroundImageProperty] =
      GURL("file:///computed_style_test/document.html");

  PromoteToComputedStyle(computed_style, parent_computed_style,
                         &property_key_to_base_url_map);

  scoped_refptr<cssom::PropertyListValue> background_image_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->background_image().get());
  ASSERT_TRUE(background_image_list);
  ASSERT_EQ(1, background_image_list->value().size());

  EXPECT_EQ(cssom::KeywordValue::GetNone(), background_image_list->value()[0]);
}

TEST(PromoteToComputedStyle, BackgroundPositionOneValueWithoutKeywordValue) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // background-position: 3em;
  scoped_ptr<cssom::PropertyListValue::Builder> background_position_builder(
      new cssom::PropertyListValue::Builder());
  background_position_builder->push_back(
      new cssom::LengthValue(3, cssom::kFontSizesAkaEmUnit));
  scoped_refptr<cssom::PropertyListValue> background_position(
      new cssom::PropertyListValue(background_position_builder.Pass()));
  computed_style->set_background_position(background_position);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> background_position_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  cssom::CalcValue* left_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[0].get());
  const cssom::LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(48.0f, left_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, left_value->percentage_value()->value());

  cssom::CalcValue* right_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[1].get());
  const cssom::LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionOneKeywordValue) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // background-position: bottom;
  scoped_ptr<cssom::PropertyListValue::Builder> background_position_builder(
      new cssom::PropertyListValue::Builder());
  background_position_builder->push_back(cssom::KeywordValue::GetBottom());
  scoped_refptr<cssom::PropertyListValue> background_position(
      new cssom::PropertyListValue(background_position_builder.Pass()));
  computed_style->set_background_position(background_position);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> background_position_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  cssom::CalcValue* left_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[0].get());
  const cssom::LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, left_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, left_value->percentage_value()->value());

  cssom::CalcValue* right_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[1].get());
  const cssom::LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(1.0f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionTwoValuesWithoutKeywordValue) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // background-position: 3em 40px;
  scoped_ptr<cssom::PropertyListValue::Builder> background_position_builder(
      new cssom::PropertyListValue::Builder());
  background_position_builder->push_back(
      new cssom::LengthValue(3, cssom::kFontSizesAkaEmUnit));
  background_position_builder->push_back(
      new cssom::LengthValue(40, cssom::kPixelsUnit));
  scoped_refptr<cssom::PropertyListValue> background_position(
      new cssom::PropertyListValue(background_position_builder.Pass()));
  computed_style->set_background_position(background_position);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> background_position_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  cssom::CalcValue* left_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[0].get());
  const cssom::LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(48.0f, left_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, left_value->percentage_value()->value());

  cssom::CalcValue* right_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[1].get());
  const cssom::LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(40.0f, right_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionTwoValuesWithOneKeyword) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // background-position: 67% center;
  scoped_ptr<cssom::PropertyListValue::Builder> background_position_builder(
      new cssom::PropertyListValue::Builder());
  background_position_builder->push_back(new cssom::PercentageValue(0.67f));
  background_position_builder->push_back(cssom::KeywordValue::GetCenter());
  scoped_refptr<cssom::PropertyListValue> background_position(
      new cssom::PropertyListValue(background_position_builder.Pass()));
  computed_style->set_background_position(background_position);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> background_position_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  cssom::CalcValue* left_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[0].get());
  const cssom::LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, left_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.67f, left_value->percentage_value()->value());

  cssom::CalcValue* right_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[1].get());
  const cssom::LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.50f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionTwoValuesWithTwoKeywords) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // background-position: right bottom;
  scoped_ptr<cssom::PropertyListValue::Builder> background_position_builder(
      new cssom::PropertyListValue::Builder());
  background_position_builder->push_back(cssom::KeywordValue::GetRight());
  background_position_builder->push_back(cssom::KeywordValue::GetBottom());
  scoped_refptr<cssom::PropertyListValue> background_position(
      new cssom::PropertyListValue(background_position_builder.Pass()));
  computed_style->set_background_position(background_position);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> background_position_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  cssom::CalcValue* left_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[0].get());
  const cssom::LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, left_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(1.0f, left_value->percentage_value()->value());

  cssom::CalcValue* right_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[1].get());
  const cssom::LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(1.0f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionTwoValuesWithTwoCenterKeywords) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // background-position: center center;
  scoped_ptr<cssom::PropertyListValue::Builder> background_position_builder(
      new cssom::PropertyListValue::Builder());
  background_position_builder->push_back(cssom::KeywordValue::GetCenter());
  background_position_builder->push_back(cssom::KeywordValue::GetCenter());
  scoped_refptr<cssom::PropertyListValue> background_position(
      new cssom::PropertyListValue(background_position_builder.Pass()));
  computed_style->set_background_position(background_position);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> background_position_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  cssom::CalcValue* left_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[0].get());
  const cssom::LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, left_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, left_value->percentage_value()->value());

  cssom::CalcValue* right_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[1].get());
  const cssom::LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionWithThreeValues) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // background-position: top 80% left;
  scoped_ptr<cssom::PropertyListValue::Builder> background_position_builder(
      new cssom::PropertyListValue::Builder());
  background_position_builder->push_back(cssom::KeywordValue::GetTop());
  background_position_builder->push_back(new cssom::PercentageValue(0.8f));
  background_position_builder->push_back(cssom::KeywordValue::GetLeft());
  scoped_refptr<cssom::PropertyListValue> background_position(
      new cssom::PropertyListValue(background_position_builder.Pass()));
  computed_style->set_background_position(background_position);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> background_position_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  cssom::CalcValue* left_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[0].get());
  const cssom::LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, left_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, left_value->percentage_value()->value());

  cssom::CalcValue* right_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[1].get());
  const cssom::LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.8f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionWithThreeValuesHaveCenter) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // background-position: center left 80%;
  scoped_ptr<cssom::PropertyListValue::Builder> background_position_builder(
      new cssom::PropertyListValue::Builder());
  background_position_builder->push_back(cssom::KeywordValue::GetCenter());
  background_position_builder->push_back(cssom::KeywordValue::GetLeft());
  background_position_builder->push_back(new cssom::PercentageValue(0.8f));
  scoped_refptr<cssom::PropertyListValue> background_position(
      new cssom::PropertyListValue(background_position_builder.Pass()));
  computed_style->set_background_position(background_position);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> background_position_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  cssom::CalcValue* left_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[0].get());
  const cssom::LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, left_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.8f, left_value->percentage_value()->value());

  cssom::CalcValue* right_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[1].get());
  const cssom::LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionWithFourValues) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // background-position: bottom 80% right 50px;
  scoped_ptr<cssom::PropertyListValue::Builder> background_position_builder(
      new cssom::PropertyListValue::Builder());
  background_position_builder->push_back(cssom::KeywordValue::GetBottom());
  background_position_builder->push_back(new cssom::PercentageValue(0.8f));
  background_position_builder->push_back(cssom::KeywordValue::GetRight());
  background_position_builder->push_back(
      new cssom::LengthValue(50, cssom::kPixelsUnit));
  scoped_refptr<cssom::PropertyListValue> background_position(
      new cssom::PropertyListValue(background_position_builder.Pass()));
  computed_style->set_background_position(background_position);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> background_position_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  cssom::CalcValue* left_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[0].get());
  const cssom::LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(-50.0f, left_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(1.0f, left_value->percentage_value()->value());

  cssom::CalcValue* right_value = base::polymorphic_downcast<cssom::CalcValue*>(
      background_position_list->value()[1].get());
  const cssom::LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.2f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundSizeEmToPixel) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  scoped_ptr<cssom::PropertyListValue::Builder> background_size_builder(
      new cssom::PropertyListValue::Builder());
  background_size_builder->push_back(
      new cssom::LengthValue(3, cssom::kFontSizesAkaEmUnit));
  background_size_builder->push_back(
      new cssom::LengthValue(40, cssom::kPixelsUnit));
  scoped_refptr<cssom::PropertyListValue> background_size(
      new cssom::PropertyListValue(background_size_builder.Pass()));
  computed_style->set_background_size(background_size);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> background_size_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->background_size().get());
  ASSERT_TRUE(background_size_list);
  ASSERT_EQ(2, background_size_list->value().size());

  cssom::LengthValue* first_value =
      base::polymorphic_downcast<cssom::LengthValue*>(
          background_size_list->value()[0].get());
  EXPECT_FLOAT_EQ(48.0f, first_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, first_value->unit());

  cssom::LengthValue* second_value =
      base::polymorphic_downcast<cssom::LengthValue*>(
          background_size_list->value()[1].get());
  EXPECT_FLOAT_EQ(40.0f, second_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, second_value->unit());
}

TEST(PromoteToComputedStyle, BackgroundSizeKeywordNotChanged) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_background_size(cssom::KeywordValue::GetContain());

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  EXPECT_EQ(cssom::KeywordValue::GetContain(),
            computed_style->background_size());
}

TEST(PromoteToComputedStyle, BorderRadiusEmToPixel) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_border_radius(
      new cssom::LengthValue(3, cssom::kFontSizesAkaEmUnit));

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  cssom::LengthValue* border_radius =
      base::polymorphic_downcast<cssom::LengthValue*>(
          computed_style->border_radius().get());
  EXPECT_FLOAT_EQ(48.0f, border_radius->value());
  EXPECT_EQ(cssom::kPixelsUnit, border_radius->unit());
}

TEST(PromoteToComputedStyle, BorderColorWithCurrentColorValue) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_border_left_color(cssom::KeywordValue::GetCurrentColor());
  computed_style->set_color(cssom::RGBAColorValue::GetAqua());

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::RGBAColorValue> border_color =
      dynamic_cast<cssom::RGBAColorValue*>(
          computed_style->border_left_color().get());
  ASSERT_TRUE(border_color);
  EXPECT_EQ(0x00FFFFFF, border_color->value());
}

TEST(PromoteToComputedStyle, BorderWidthWithBorderStyleNone) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_border_top_style(cssom::KeywordValue::GetNone());
  computed_style->set_border_top_width(
      new cssom::LengthValue(2, cssom::kFontSizesAkaEmUnit));

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::LengthValue> border_top_width =
      dynamic_cast<cssom::LengthValue*>(
          computed_style->border_top_width().get());
  ASSERT_TRUE(border_top_width);
  EXPECT_EQ(0, border_top_width->value());
  EXPECT_EQ(cssom::kPixelsUnit, border_top_width->unit());
}

TEST(PromoteToComputedStyle, BorderWidthInEmShouldBeComputedAfterFontSize) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_border_left_style(cssom::KeywordValue::GetSolid());
  computed_style->set_font_size(
      new cssom::LengthValue(2, cssom::kFontSizesAkaEmUnit));
  computed_style->set_border_left_width(
      new cssom::LengthValue(2, cssom::kFontSizesAkaEmUnit));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->set_font_size(
      new cssom::LengthValue(100, cssom::kPixelsUnit));
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::LengthValue> border_left_width =
      dynamic_cast<cssom::LengthValue*>(
          computed_style->border_left_width().get());
  EXPECT_EQ(400, border_left_width->value());
  EXPECT_EQ(cssom::kPixelsUnit, border_left_width->unit());
}

TEST(PromoteToComputedStyle, BoxShadowWithEmLengthAndColor) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  scoped_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths;
  lengths.push_back(new cssom::LengthValue(100, cssom::kPixelsUnit));
  lengths.push_back(new cssom::LengthValue(10, cssom::kFontSizesAkaEmUnit));

  scoped_refptr<cssom::ShadowValue> shadow(
      new cssom::ShadowValue(lengths, cssom::RGBAColorValue::GetAqua()));
  builder->push_back(shadow);

  computed_style->set_box_shadow(new PropertyListValue(builder.Pass()));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->set_font_size(
      new cssom::LengthValue(50, cssom::kPixelsUnit));
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> box_shadow_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->box_shadow().get());
  DCHECK_EQ(1u, box_shadow_list->value().size());

  scoped_refptr<cssom::ShadowValue> box_shadow =
      dynamic_cast<cssom::ShadowValue*>(box_shadow_list->value()[0].get());

  float length_value[] = {100.0f, 500.0f};
  for (size_t i = 0; i < box_shadow->lengths().size(); ++i) {
    scoped_refptr<cssom::LengthValue> length =
        dynamic_cast<cssom::LengthValue*>(box_shadow->lengths()[i].get());
    EXPECT_EQ(length_value[i], length->value());
    EXPECT_EQ(cssom::kPixelsUnit, length->unit());
  }

  scoped_refptr<cssom::RGBAColorValue> color =
      dynamic_cast<cssom::RGBAColorValue*>(box_shadow->color().get());
  ASSERT_TRUE(color);
  EXPECT_EQ(0x00FFFFFF, color->value());

  EXPECT_FALSE(box_shadow->has_inset());
}

TEST(PromoteToComputedStyle, BoxShadowWithInset) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  scoped_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths;
  lengths.push_back(new cssom::LengthValue(100, cssom::kPixelsUnit));
  lengths.push_back(new cssom::LengthValue(30, cssom::kPixelsUnit));

  scoped_refptr<cssom::ShadowValue> shadow(new cssom::ShadowValue(
      lengths, cssom::RGBAColorValue::GetAqua(), true /*has_inset*/));
  builder->push_back(shadow);

  computed_style->set_box_shadow(new PropertyListValue(builder.Pass()));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> box_shadow_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->box_shadow().get());
  DCHECK_EQ(1u, box_shadow_list->value().size());

  scoped_refptr<cssom::ShadowValue> box_shadow =
      dynamic_cast<cssom::ShadowValue*>(box_shadow_list->value()[0].get());

  float length_value[] = {100.0f, 30.0f};
  for (size_t i = 0; i < box_shadow->lengths().size(); ++i) {
    scoped_refptr<cssom::LengthValue> length =
        dynamic_cast<cssom::LengthValue*>(box_shadow->lengths()[i].get());
    EXPECT_EQ(length_value[i], length->value());
    EXPECT_EQ(cssom::kPixelsUnit, length->unit());
  }

  scoped_refptr<cssom::RGBAColorValue> color =
      dynamic_cast<cssom::RGBAColorValue*>(box_shadow->color().get());
  ASSERT_TRUE(color);
  EXPECT_EQ(0x00FFFFFF, color->value());

  EXPECT_TRUE(box_shadow->has_inset());
}

TEST(PromoteToComputedStyle, BoxShadowWithoutColor) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  scoped_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths;
  lengths.push_back(new cssom::LengthValue(100, cssom::kPixelsUnit));
  lengths.push_back(new cssom::LengthValue(200, cssom::kPixelsUnit));

  scoped_refptr<cssom::ShadowValue> shadow(
      new cssom::ShadowValue(lengths, NULL));
  builder->push_back(shadow);

  computed_style->set_box_shadow(new PropertyListValue(builder.Pass()));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->set_color(new cssom::RGBAColorValue(0x0047abff));
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> box_shadow_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->box_shadow().get());
  DCHECK_EQ(1u, box_shadow_list->value().size());

  scoped_refptr<cssom::ShadowValue> box_shadow =
      dynamic_cast<cssom::ShadowValue*>(box_shadow_list->value()[0].get());

  float length_value[] = {100.0f, 200.0f};
  for (size_t i = 0; i < box_shadow->lengths().size(); ++i) {
    scoped_refptr<cssom::LengthValue> length =
        dynamic_cast<cssom::LengthValue*>(box_shadow->lengths()[i].get());
    EXPECT_EQ(length_value[i], length->value());
    EXPECT_EQ(cssom::kPixelsUnit, length->unit());
  }

  scoped_refptr<cssom::RGBAColorValue> color =
      dynamic_cast<cssom::RGBAColorValue*>(box_shadow->color().get());
  ASSERT_TRUE(color);
  EXPECT_EQ(0x0047abff, color->value());

  EXPECT_FALSE(box_shadow->has_inset());
}

TEST(PromoteToComputedStyle, BoxShadowWithShadowList) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  scoped_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths_1;
  lengths_1.push_back(new cssom::LengthValue(100, cssom::kPixelsUnit));
  lengths_1.push_back(new cssom::LengthValue(30, cssom::kPixelsUnit));
  lengths_1.push_back(new cssom::LengthValue(3, cssom::kFontSizesAkaEmUnit));
  scoped_refptr<cssom::ShadowValue> shadow_1(
      new cssom::ShadowValue(lengths_1, NULL, true /*has_inset*/));
  builder->push_back(shadow_1);

  std::vector<scoped_refptr<LengthValue> > lengths_2;
  lengths_2.push_back(new cssom::LengthValue(2, cssom::kFontSizesAkaEmUnit));
  lengths_2.push_back(new cssom::LengthValue(40, cssom::kPixelsUnit));
  scoped_refptr<cssom::ShadowValue> shadow_2(new cssom::ShadowValue(
      lengths_2, cssom::RGBAColorValue::GetNavy(), false /*has_inset*/));

  builder->push_back(shadow_2);

  computed_style->set_box_shadow(new PropertyListValue(builder.Pass()));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->set_font_size(
      new cssom::LengthValue(50, cssom::kPixelsUnit));
  parent_computed_style->set_color(new cssom::RGBAColorValue(0x0047ABFF));
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> box_shadow_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->box_shadow().get());
  DCHECK_EQ(2u, box_shadow_list->value().size());

  float expected_length_value[2][3] = {
      {100.0f, 30.0f, 150.0f}, {100.0f, 40.0f, 0.0f},
  };
  float expected_color[2] = {0x0047ABFF, 0x000080FF};
  bool expected_has_inset[2] = {true, false};

  for (size_t i = 0; i < box_shadow_list->value().size(); ++i) {
    scoped_refptr<cssom::ShadowValue> box_shadow =
        dynamic_cast<cssom::ShadowValue*>(box_shadow_list->value()[i].get());

    if (i == 0) {
      EXPECT_EQ(3, box_shadow->lengths().size());
    } else {
      EXPECT_EQ(2, box_shadow->lengths().size());
    }

    for (size_t j = 0; j < box_shadow->lengths().size(); ++j) {
      scoped_refptr<cssom::LengthValue> length =
          dynamic_cast<cssom::LengthValue*>(box_shadow->lengths()[j].get());
      EXPECT_EQ(expected_length_value[i][j], length->value());
      EXPECT_EQ(cssom::kPixelsUnit, length->unit());
    }

    scoped_refptr<cssom::RGBAColorValue> color =
        dynamic_cast<cssom::RGBAColorValue*>(box_shadow->color().get());
    ASSERT_TRUE(color);
    EXPECT_EQ(expected_color[i], color->value());

    EXPECT_EQ(expected_has_inset[i], box_shadow->has_inset());
  }
}

TEST(PromoteToComputedStyle, BoxShadowWithNone) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  computed_style->set_box_shadow(cssom::KeywordValue::GetNone());

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  EXPECT_EQ(cssom::KeywordValue::GetNone(), computed_style->box_shadow());
}

TEST(PromoteToComputedStyle, TextShadowWithEmLengthAndColor) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  scoped_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths;
  lengths.push_back(new cssom::LengthValue(100, cssom::kPixelsUnit));
  lengths.push_back(new cssom::LengthValue(10, cssom::kFontSizesAkaEmUnit));

  scoped_refptr<cssom::ShadowValue> shadow(
      new cssom::ShadowValue(lengths, cssom::RGBAColorValue::GetAqua()));
  builder->push_back(shadow);

  computed_style->set_text_shadow(new PropertyListValue(builder.Pass()));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->set_font_size(
      new cssom::LengthValue(50, cssom::kPixelsUnit));
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> text_shadow_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->text_shadow().get());
  DCHECK_EQ(1u, text_shadow_list->value().size());

  scoped_refptr<cssom::ShadowValue> text_shadow =
      dynamic_cast<cssom::ShadowValue*>(text_shadow_list->value()[0].get());

  float length_value[] = {100.0f, 500.0f};
  for (size_t i = 0; i < text_shadow->lengths().size(); ++i) {
    scoped_refptr<cssom::LengthValue> length =
        dynamic_cast<cssom::LengthValue*>(text_shadow->lengths()[i].get());
    EXPECT_EQ(length_value[i], length->value());
    EXPECT_EQ(cssom::kPixelsUnit, length->unit());
  }

  scoped_refptr<cssom::RGBAColorValue> color =
      dynamic_cast<cssom::RGBAColorValue*>(text_shadow->color().get());
  ASSERT_TRUE(color);
  EXPECT_EQ(0x00FFFFFF, color->value());

  EXPECT_FALSE(text_shadow->has_inset());
}

TEST(PromoteToComputedStyle, TextShadowWithoutColor) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  scoped_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths;
  lengths.push_back(new cssom::LengthValue(100, cssom::kPixelsUnit));
  lengths.push_back(new cssom::LengthValue(200, cssom::kPixelsUnit));

  scoped_refptr<cssom::ShadowValue> shadow(
      new cssom::ShadowValue(lengths, NULL));
  builder->push_back(shadow);

  computed_style->set_text_shadow(new PropertyListValue(builder.Pass()));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->set_color(new cssom::RGBAColorValue(0x0047abff));
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> text_shadow_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->text_shadow().get());
  DCHECK_EQ(1u, text_shadow_list->value().size());

  scoped_refptr<cssom::ShadowValue> text_shadow =
      dynamic_cast<cssom::ShadowValue*>(text_shadow_list->value()[0].get());

  float length_value[] = {100.0f, 200.0f};
  for (size_t i = 0; i < text_shadow->lengths().size(); ++i) {
    scoped_refptr<cssom::LengthValue> length =
        dynamic_cast<cssom::LengthValue*>(text_shadow->lengths()[i].get());
    EXPECT_EQ(length_value[i], length->value());
    EXPECT_EQ(cssom::kPixelsUnit, length->unit());
  }

  scoped_refptr<cssom::RGBAColorValue> color =
      dynamic_cast<cssom::RGBAColorValue*>(text_shadow->color().get());
  ASSERT_TRUE(color);
  EXPECT_EQ(0x0047abff, color->value());

  EXPECT_FALSE(text_shadow->has_inset());
}

TEST(PromoteToComputedStyle, TextShadowWithShadowList) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  scoped_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths_1;
  lengths_1.push_back(new cssom::LengthValue(100, cssom::kPixelsUnit));
  lengths_1.push_back(new cssom::LengthValue(30, cssom::kPixelsUnit));
  lengths_1.push_back(new cssom::LengthValue(3, cssom::kFontSizesAkaEmUnit));
  scoped_refptr<cssom::ShadowValue> shadow_1(
      new cssom::ShadowValue(lengths_1, NULL));
  builder->push_back(shadow_1);

  std::vector<scoped_refptr<LengthValue> > lengths_2;
  lengths_2.push_back(new cssom::LengthValue(2, cssom::kFontSizesAkaEmUnit));
  lengths_2.push_back(new cssom::LengthValue(40, cssom::kPixelsUnit));
  scoped_refptr<cssom::ShadowValue> shadow_2(
      new cssom::ShadowValue(lengths_2, cssom::RGBAColorValue::GetNavy()));

  builder->push_back(shadow_2);

  computed_style->set_text_shadow(new PropertyListValue(builder.Pass()));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->set_font_size(
      new cssom::LengthValue(50, cssom::kPixelsUnit));
  parent_computed_style->set_color(new cssom::RGBAColorValue(0x0047ABFF));
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> text_shadow_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->text_shadow().get());
  DCHECK_EQ(2u, text_shadow_list->value().size());

  float expected_length_value[2][3] = {
      {100.0f, 30.0f, 150.0f}, {100.0f, 40.0f, 0.0f},
  };
  float expected_color[2] = {0x0047ABFF, 0x000080FF};

  for (size_t i = 0; i < text_shadow_list->value().size(); ++i) {
    scoped_refptr<cssom::ShadowValue> text_shadow =
        dynamic_cast<cssom::ShadowValue*>(text_shadow_list->value()[i].get());

    if (i == 0) {
      EXPECT_EQ(3, text_shadow->lengths().size());
    } else {
      EXPECT_EQ(2, text_shadow->lengths().size());
    }

    for (size_t j = 0; j < text_shadow->lengths().size(); ++j) {
      scoped_refptr<cssom::LengthValue> length =
          dynamic_cast<cssom::LengthValue*>(text_shadow->lengths()[j].get());
      EXPECT_EQ(expected_length_value[i][j], length->value());
      EXPECT_EQ(cssom::kPixelsUnit, length->unit());
    }

    scoped_refptr<cssom::RGBAColorValue> color =
        dynamic_cast<cssom::RGBAColorValue*>(text_shadow->color().get());
    ASSERT_TRUE(color);
    EXPECT_EQ(expected_color[i], color->value());

    EXPECT_FALSE(text_shadow->has_inset());
  }
}

TEST(PromoteToComputedStyle, TextShadowWithNone) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  computed_style->set_text_shadow(cssom::KeywordValue::GetNone());

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  EXPECT_EQ(cssom::KeywordValue::GetNone(), computed_style->text_shadow());
}

TEST(PromoteToComputedStyle, HeightPercentageInUnspecifiedHeightBlockIsAuto) {
  // If the height is specified as a percentage and the height of the containing
  // block is not specified explicitly, and this element is not absolutely
  // positioned, the value computes to 'auto'.
  //   https://www.w3.org/TR/CSS2/visudet.html#the-height-property
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_height(new PercentageValue(0.50f));

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  EXPECT_EQ(cssom::KeywordValue::GetAuto(), parent_computed_style->height());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  EXPECT_EQ(cssom::KeywordValue::GetAuto(), computed_style->height());
}

TEST(PromoteToComputedStyle,
     MaxHeightPercentageInUnspecifiedHeightBlockIsNone) {
  // If the max-height is specified as a percentage and the height of the
  // containing block is not specified explicitly, and this element is not
  // absolutely positioned, the percentage value is treated as '0'.
  //   https://www.w3.org/TR/CSS2/visudet.html#propdef-max-height
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_max_height(new PercentageValue(0.50f));

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  EXPECT_EQ(cssom::KeywordValue::GetAuto(), parent_computed_style->height());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  EXPECT_EQ(cssom::KeywordValue::GetNone(), computed_style->max_height());
}

TEST(PromoteToComputedStyle,
     MinHeightPercentageInUnspecifiedHeightBlockIsZero) {
  // If the min-height is specified as a percentage and the height of the
  // containing block is not specified explicitly, and this element is not
  // absolutely positioned, the percentage value is treated as 'none'.
  //   https://www.w3.org/TR/CSS2/visudet.html#propdef-min-height
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_min_height(new PercentageValue(0.50f));

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  EXPECT_EQ(cssom::KeywordValue::GetAuto(), parent_computed_style->height());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  cssom::LengthValue* computed_min_height =
      base::polymorphic_downcast<cssom::LengthValue*>(
          computed_style->min_height().get());
  EXPECT_EQ(0, computed_min_height->value());
  EXPECT_EQ(cssom::kPixelsUnit, computed_min_height->unit());
}

TEST(PromoteToComputedStyle, MaxWidthPercentageInNegativeWidthBlockIsZero) {
  // If the max-width is specified as a percentage and the containing block's
  // width is negative, the used value is zero.
  //  https://www.w3.org/TR/CSS2/visudet.html#propdef-max-width
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_max_width(new PercentageValue(0.50f));

  scoped_refptr<const cssom::CSSStyleDeclarationData>
      grandparent_computed_style(new cssom::CSSStyleDeclarationData());

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->set_width(new LengthValue(-16, kPixelsUnit));
  PromoteToComputedStyle(parent_computed_style, grandparent_computed_style,
                         NULL);
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  cssom::LengthValue* computed_max_width =
      base::polymorphic_downcast<cssom::LengthValue*>(
          computed_style->max_width().get());
  EXPECT_EQ(0, computed_max_width->value());
  EXPECT_EQ(cssom::kPixelsUnit, computed_max_width->unit());
}

TEST(PromoteToComputedStyle, MinWidthPercentageInNegativeWidthBlockIsZero) {
  // If the min-width is specified as a percentage and the containing block's
  // width is negative, the used value is zero.
  //  https://www.w3.org/TR/CSS2/visudet.html#propdef-min-width
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_min_width(new PercentageValue(0.50f));

  scoped_refptr<const cssom::CSSStyleDeclarationData>
      grandparent_computed_style(new cssom::CSSStyleDeclarationData());

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->set_width(new LengthValue(-16, kPixelsUnit));
  PromoteToComputedStyle(parent_computed_style, grandparent_computed_style,
                         NULL);
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  cssom::LengthValue* computed_min_width =
      base::polymorphic_downcast<cssom::LengthValue*>(
          computed_style->min_width().get());
  EXPECT_EQ(0, computed_min_width->value());
  EXPECT_EQ(cssom::kPixelsUnit, computed_min_width->unit());
}

TEST(PromoteToComputedStyle, LineHeightPercentageIsRelativeToFontSize) {
  // The computed value of the property is this percentage multiplied by the
  // element's computed font size. Negative values are illegal.
  //   https://www.w3.org/TR/CSS21/visudet.html#line-height
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->set_font_size(
      new cssom::LengthValue(100, cssom::kPixelsUnit));
  computed_style->set_line_height(new PercentageValue(0.75f));

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());

  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  cssom::LengthValue* computed_line_height =
      base::polymorphic_downcast<cssom::LengthValue*>(
          computed_style->line_height().get());
  EXPECT_EQ(75, computed_line_height->value());
  EXPECT_EQ(cssom::kPixelsUnit, computed_line_height->unit());
}

TEST(PromoteToComputedStyle, TransformOriginOneValueWithKeyword) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // transform-origin: bottom;
  scoped_ptr<cssom::PropertyListValue::Builder> transform_origin_builder(
      new cssom::PropertyListValue::Builder());
  transform_origin_builder->push_back(cssom::KeywordValue::GetBottom());
  scoped_refptr<cssom::PropertyListValue> transform_origin(
      new cssom::PropertyListValue(transform_origin_builder.Pass()));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> transform_origin_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  cssom::CalcValue* first_value = base::polymorphic_downcast<cssom::CalcValue*>(
      transform_origin_list->value()[0].get());
  const cssom::LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, first_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, first_value->percentage_value()->value());

  cssom::CalcValue* second_value =
      base::polymorphic_downcast<cssom::CalcValue*>(
          transform_origin_list->value()[1].get());
  const cssom::LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, second_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(1.0f, second_value->percentage_value()->value());

  cssom::LengthValue* third_value =
      base::polymorphic_downcast<cssom::LengthValue*>(
          transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(0.0f, third_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, third_value->unit());
}

TEST(PromoteToComputedStyle, TransformOriginOneValueWithoutKeyword) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // transform-origin: 3em;
  scoped_ptr<cssom::PropertyListValue::Builder> transform_origin_builder(
      new cssom::PropertyListValue::Builder());
  transform_origin_builder->push_back(
      new cssom::LengthValue(3, cssom::kFontSizesAkaEmUnit));
  scoped_refptr<cssom::PropertyListValue> transform_origin(
      new cssom::PropertyListValue(transform_origin_builder.Pass()));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> transform_origin_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  cssom::CalcValue* first_value = base::polymorphic_downcast<cssom::CalcValue*>(
      transform_origin_list->value()[0].get());
  const cssom::LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(48.0f, first_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, first_value->percentage_value()->value());

  cssom::CalcValue* second_value =
      base::polymorphic_downcast<cssom::CalcValue*>(
          transform_origin_list->value()[1].get());
  const cssom::LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, second_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, second_value->percentage_value()->value());

  cssom::LengthValue* third_value =
      base::polymorphic_downcast<cssom::LengthValue*>(
          transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(0.0f, third_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, third_value->unit());
}

TEST(PromoteToComputedStyle, TransformOriginTwoValuesWithoutKeywordValue) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // transform-origin: 3em 40px;
  scoped_ptr<cssom::PropertyListValue::Builder> transform_origin_builder(
      new cssom::PropertyListValue::Builder());
  transform_origin_builder->push_back(
      new cssom::LengthValue(3, cssom::kFontSizesAkaEmUnit));
  transform_origin_builder->push_back(
      new cssom::LengthValue(40, cssom::kPixelsUnit));
  scoped_refptr<cssom::PropertyListValue> transform_origin(
      new cssom::PropertyListValue(transform_origin_builder.Pass()));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> transform_origin_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  cssom::CalcValue* first_value = base::polymorphic_downcast<cssom::CalcValue*>(
      transform_origin_list->value()[0].get());
  const cssom::LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(48.0f, first_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, first_value->percentage_value()->value());

  cssom::CalcValue* second_value =
      base::polymorphic_downcast<cssom::CalcValue*>(
          transform_origin_list->value()[1].get());
  const cssom::LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(40.0f, second_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, second_value->percentage_value()->value());

  cssom::LengthValue* third_value =
      base::polymorphic_downcast<cssom::LengthValue*>(
          transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(0.0f, third_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, third_value->unit());
}

TEST(PromoteToComputedStyle, TransformOriginTwoValuesWithOneKeyword) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // transform-origin: right 20%;
  scoped_ptr<cssom::PropertyListValue::Builder> transform_origin_builder(
      new cssom::PropertyListValue::Builder());
  transform_origin_builder->push_back(cssom::KeywordValue::GetRight());
  transform_origin_builder->push_back(new cssom::PercentageValue(0.2f));
  scoped_refptr<cssom::PropertyListValue> transform_origin(
      new cssom::PropertyListValue(transform_origin_builder.Pass()));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> transform_origin_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  cssom::CalcValue* first_value = base::polymorphic_downcast<cssom::CalcValue*>(
      transform_origin_list->value()[0].get());
  const cssom::LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, first_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(1.0f, first_value->percentage_value()->value());

  cssom::CalcValue* second_value =
      base::polymorphic_downcast<cssom::CalcValue*>(
          transform_origin_list->value()[1].get());
  const cssom::LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, second_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(0.2f, second_value->percentage_value()->value());

  cssom::LengthValue* third_value =
      base::polymorphic_downcast<cssom::LengthValue*>(
          transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(0.0f, third_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, third_value->unit());
}

TEST(PromoteToComputedStyle, TransformOriginTwoValuesWithoutKeyword) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // transform-origin: 60% 80%;
  scoped_ptr<cssom::PropertyListValue::Builder> transform_origin_builder(
      new cssom::PropertyListValue::Builder());
  transform_origin_builder->push_back(new cssom::PercentageValue(0.6f));
  transform_origin_builder->push_back(new cssom::PercentageValue(0.8f));
  scoped_refptr<cssom::PropertyListValue> transform_origin(
      new cssom::PropertyListValue(transform_origin_builder.Pass()));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> transform_origin_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  cssom::CalcValue* first_value = base::polymorphic_downcast<cssom::CalcValue*>(
      transform_origin_list->value()[0].get());
  const cssom::LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, first_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(0.6f, first_value->percentage_value()->value());

  cssom::CalcValue* second_value =
      base::polymorphic_downcast<cssom::CalcValue*>(
          transform_origin_list->value()[1].get());
  const cssom::LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, second_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(0.8f, second_value->percentage_value()->value());

  cssom::LengthValue* third_value =
      base::polymorphic_downcast<cssom::LengthValue*>(
          transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(0.0f, third_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, third_value->unit());
}

TEST(PromoteToComputedStyle, TransformOriginTwoKeywordValues) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // transform-origin: top center;
  scoped_ptr<cssom::PropertyListValue::Builder> transform_origin_builder(
      new cssom::PropertyListValue::Builder());
  transform_origin_builder->push_back(cssom::KeywordValue::GetTop());
  transform_origin_builder->push_back(cssom::KeywordValue::GetCenter());
  scoped_refptr<cssom::PropertyListValue> transform_origin(
      new cssom::PropertyListValue(transform_origin_builder.Pass()));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> transform_origin_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  cssom::CalcValue* first_value = base::polymorphic_downcast<cssom::CalcValue*>(
      transform_origin_list->value()[0].get());
  const cssom::LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, first_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, first_value->percentage_value()->value());

  cssom::CalcValue* second_value =
      base::polymorphic_downcast<cssom::CalcValue*>(
          transform_origin_list->value()[1].get());
  const cssom::LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, second_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, second_value->percentage_value()->value());

  cssom::LengthValue* third_value =
      base::polymorphic_downcast<cssom::LengthValue*>(
          transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(0.0f, third_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, third_value->unit());
}

TEST(PromoteToComputedStyle, TransformOriginThreeValues) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());

  // transform-origin: 30px top 50px;
  scoped_ptr<cssom::PropertyListValue::Builder> transform_origin_builder(
      new cssom::PropertyListValue::Builder());
  transform_origin_builder->push_back(
      new cssom::LengthValue(30.0f, cssom::kPixelsUnit));
  transform_origin_builder->push_back(cssom::KeywordValue::GetTop());
  transform_origin_builder->push_back(
      new cssom::LengthValue(50.0f, cssom::kPixelsUnit));
  scoped_refptr<cssom::PropertyListValue> transform_origin(
      new cssom::PropertyListValue(transform_origin_builder.Pass()));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  scoped_refptr<cssom::PropertyListValue> transform_origin_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  cssom::CalcValue* first_value = base::polymorphic_downcast<cssom::CalcValue*>(
      transform_origin_list->value()[0].get());
  const cssom::LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(30.0f, first_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, first_value->percentage_value()->value());

  cssom::CalcValue* second_value =
      base::polymorphic_downcast<cssom::CalcValue*>(
          transform_origin_list->value()[1].get());
  const cssom::LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, second_length_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, second_value->percentage_value()->value());

  cssom::LengthValue* third_value =
      base::polymorphic_downcast<cssom::LengthValue*>(
          transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(50.0f, third_value->value());
  EXPECT_EQ(cssom::kPixelsUnit, third_value->unit());
}

}  // namespace cssom
}  // namespace cobalt

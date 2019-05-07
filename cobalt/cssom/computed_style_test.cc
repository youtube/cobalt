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

#include "cobalt/cssom/computed_style.h"

#include <memory>
#include <vector>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/calc_value.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/css_computed_style_declaration.h"
#include "cobalt/cssom/font_style_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/integer_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/shadow_value.h"
#include "cobalt/cssom/transform_function_list_value.h"
#include "cobalt/cssom/translate_function.h"
#include "cobalt/cssom/url_value.h"
#include "cobalt/math/size.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {
namespace {

const math::Size kNullSize;

scoped_refptr<CSSComputedStyleDeclaration> CreateComputedStyleDeclaration(
    scoped_refptr<CSSComputedStyleData> computed_style) {
  scoped_refptr<CSSComputedStyleDeclaration> computed_style_declaration(
      new CSSComputedStyleDeclaration());
  computed_style_declaration->SetData(computed_style);
  return computed_style_declaration;
}

TEST(PromoteToComputedStyle, UnknownPropertyValueShouldBeEmpty) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> computed_style_declaration(
      CreateComputedStyleDeclaration(computed_style));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_EQ(
      computed_style_declaration->GetPropertyValue("cobalt_cobalt_cobalt"), "");
}

TEST(PromoteToComputedStyle, FlexBasisPercentage) {
  // The computed value for flex-basis is the specified keyword or a computed
  // <length-percentage> value.
  //   https://www.w3.org/TR/css-flexbox-1/#flex-basis-property
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_flex_basis(new PercentageValue(0.50f));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  EXPECT_EQ(KeywordValue::GetAuto(), parent_computed_style->width());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  PercentageValue* flex_basis = base::polymorphic_downcast<PercentageValue*>(
      computed_style->flex_basis().get());
  ASSERT_TRUE(flex_basis);
  EXPECT_FLOAT_EQ(0.50f, flex_basis->value());
}

TEST(PromoteToComputedStyle, FlexBasisKeyword) {
  // The computed value for flex-basis is the specified keyword or a computed
  // <length-percentage> value.
  //   https://www.w3.org/TR/css-flexbox-1/#flex-basis-property
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_flex_basis(KeywordValue::GetContent());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);
  ASSERT_TRUE(computed_style->IsDeclared(PropertyKey::kFlexBasisProperty));
  EXPECT_EQ(KeywordValue::GetContent(), computed_style->flex_basis());
}

TEST(PromoteToComputedStyle, FlexBasisLength) {
  // The computed value for flex-basis is the specified keyword or a computed
  // <length-percentage> value.
  //   https://www.w3.org/TR/css-flexbox-1/#flex-basis-property
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_flex_basis(new LengthValue(100, kPixelsUnit));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  EXPECT_EQ(KeywordValue::GetAuto(), parent_computed_style->width());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  ASSERT_TRUE(computed_style->IsDeclared(PropertyKey::kFlexBasisProperty));
  LengthValue* flex_basis = base::polymorphic_downcast<LengthValue*>(
      computed_style->flex_basis().get());
  ASSERT_TRUE(flex_basis);
  EXPECT_EQ(100, flex_basis->value());
  EXPECT_EQ(kPixelsUnit, flex_basis->unit());
}

TEST(PromoteToComputedStyle, FlexGrowNumber) {
  // The computed value for flex-grow is the specified number.
  //   https://www.w3.org/TR/css-flexbox-1/#flex-grow-property
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_flex_grow(new NumberValue(10.0f));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  ASSERT_TRUE(computed_style->IsDeclared(PropertyKey::kFlexGrowProperty));
  NumberValue* flex_grow = base::polymorphic_downcast<NumberValue*>(
      computed_style->flex_grow().get());
  ASSERT_TRUE(flex_grow);
  EXPECT_EQ(10.0f, flex_grow->value());
}

TEST(PromoteToComputedStyle, FlexShrinkNumber) {
  // The computed value for flex-grow is the specified number.
  //   https://www.w3.org/TR/css-flexbox-1/#flex-shrink-property
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_flex_shrink(new NumberValue(5.0f));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  ASSERT_TRUE(computed_style->IsDeclared(PropertyKey::kFlexShrinkProperty));
  NumberValue* flex_shrink = base::polymorphic_downcast<NumberValue*>(
      computed_style->flex_shrink().get());
  ASSERT_TRUE(flex_shrink);
  EXPECT_EQ(5.0f, flex_shrink->value());
}

TEST(PromoteToComputedStyle, FontWeightShouldBeBoldAsSpecified) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_font_weight(FontWeightValue::GetBoldAka700());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_EQ(FontWeightValue::GetBoldAka700(),
            computed_style->font_weight().get());
}

TEST(PromoteToComputedStyle, LengthValueInEmShouldBeRelativeToParentFontSize) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_font_size(new LengthValue(1.5f, kFontSizesAkaEmUnit));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_font_size(new LengthValue(100, kPixelsUnit));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  LengthValue* computed_font_size = base::polymorphic_downcast<LengthValue*>(
      computed_style->font_size().get());
  EXPECT_EQ(150, computed_font_size->value());
  EXPECT_EQ(kPixelsUnit, computed_font_size->unit());
}

TEST(PromoteToComputedStyle, LengthValueInRemShouldBeRelativeToRootFontSize) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_font_size(
      new LengthValue(1.5f, kRootElementFontSizesAkaRemUnit));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_font_size(new LengthValue(100, kPixelsUnit));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  scoped_refptr<MutableCSSComputedStyleData> root_computed_style(
      new MutableCSSComputedStyleData());
  root_computed_style->set_font_size(new LengthValue(200, kPixelsUnit));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         root_computed_style, kNullSize, NULL);

  LengthValue* computed_font_size = base::polymorphic_downcast<LengthValue*>(
      computed_style->font_size().get());
  EXPECT_EQ(300, computed_font_size->value());
  EXPECT_EQ(kPixelsUnit, computed_font_size->unit());
}

TEST(PromoteToComputedStyle, LengthValueInVwVhShouldBeRelativeToViewportSize) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_font_size(
      new LengthValue(2.0f, kViewportWidthPercentsAkaVwUnit));
  computed_style->set_line_height(
      new LengthValue(2.0f, kViewportHeightPercentsAkaVhUnit));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, math::Size(360, 240), NULL);

  LengthValue* computed_font_size = base::polymorphic_downcast<LengthValue*>(
      computed_style->font_size().get());
  EXPECT_EQ(7.2f, computed_font_size->value());
  EXPECT_EQ(kPixelsUnit, computed_font_size->unit());

  LengthValue* computed_line_height = base::polymorphic_downcast<LengthValue*>(
      computed_style->line_height().get());
  EXPECT_EQ(4.8f, computed_line_height->value());
  EXPECT_EQ(kPixelsUnit, computed_line_height->unit());
}

TEST(PromoteToComputedStyle, LengthValueInPixelsShouldBeLeftAsSpecified) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_font_size(new LengthValue(50, kPixelsUnit));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  LengthValue* computed_font_size = base::polymorphic_downcast<LengthValue*>(
      computed_style->font_size().get());
  EXPECT_EQ(50, computed_font_size->value());
  EXPECT_EQ(kPixelsUnit, computed_font_size->unit());
}

TEST(PromoteToComputedStyle, NormalLineHeightShouldBeLeftAsSpecified) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_line_height(KeywordValue::GetNormal());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_EQ(KeywordValue::GetNormal(), computed_style->line_height());
}

TEST(PromoteToComputedStyle, LineHeightInEmShouldBeComputedAfterFontSize) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_font_size(new LengthValue(2, kFontSizesAkaEmUnit));
  computed_style->set_line_height(new LengthValue(1.5f, kFontSizesAkaEmUnit));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_font_size(new LengthValue(100, kPixelsUnit));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  LengthValue* computed_line_height = base::polymorphic_downcast<LengthValue*>(
      computed_style->line_height().get());
  EXPECT_EQ(300, computed_line_height->value());
  EXPECT_EQ(kPixelsUnit, computed_line_height->unit());
}

TEST(PromoteToComputedStyle, TextIndentInEmShouldBeComputedAfterFontSize) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_font_size(new LengthValue(2, kFontSizesAkaEmUnit));
  computed_style->set_text_indent(new LengthValue(1.5f, kFontSizesAkaEmUnit));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_font_size(new LengthValue(100, kPixelsUnit));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  LengthValue* computed_text_indent = base::polymorphic_downcast<LengthValue*>(
      computed_style->text_indent().get());
  EXPECT_EQ(300, computed_text_indent->value());
  EXPECT_EQ(kPixelsUnit, computed_text_indent->unit());
}

TEST(PromoteToComputedStyle, BackgroundImageRelativeURL) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  std::unique_ptr<PropertyListValue::Builder> background_image_builder(
      new PropertyListValue::Builder());
  background_image_builder->push_back(new URLValue("../test/sample.png"));
  scoped_refptr<PropertyListValue> background_image(
      new PropertyListValue(std::move(background_image_builder)));
  computed_style->set_background_image(background_image);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  GURLMap property_key_to_base_url_map;
  property_key_to_base_url_map[kBackgroundImageProperty] =
      GURL("file:///computed_style_test/style_sheet.css");

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize,
                         &property_key_to_base_url_map);

  scoped_refptr<PropertyListValue> background_image_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->background_image().get());
  ASSERT_TRUE(background_image_list);
  ASSERT_EQ(1, background_image_list->value().size());

  GURL value = base::polymorphic_downcast<AbsoluteURLValue*>(
                   background_image_list->value()[0].get())
                   ->value();

  EXPECT_EQ("file:///test/sample.png", value.spec());
}

TEST(PromoteToComputedStyle, BackgroundImageNone) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  std::unique_ptr<PropertyListValue::Builder> background_image_builder(
      new PropertyListValue::Builder());
  background_image_builder->push_back(KeywordValue::GetNone());
  scoped_refptr<PropertyListValue> background_image(
      new PropertyListValue(std::move(background_image_builder)));
  computed_style->set_background_image(background_image);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  GURLMap property_key_to_base_url_map;
  property_key_to_base_url_map[kBackgroundImageProperty] =
      GURL("file:///computed_style_test/document.html");

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize,
                         &property_key_to_base_url_map);

  scoped_refptr<PropertyListValue> background_image_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->background_image().get());
  ASSERT_TRUE(background_image_list);
  ASSERT_EQ(1, background_image_list->value().size());

  EXPECT_EQ(KeywordValue::GetNone(), background_image_list->value()[0]);
}

TEST(PromoteToComputedStyle, BackgroundPositionWithInitialValue) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  scoped_refptr<PropertyValue> background_position(
      GetPropertyInitialValue(kBackgroundPositionProperty));
  computed_style->set_background_position(background_position);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> background_position_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  CalcValue* left_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[0].get());
  const LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, left_length_value->value());
  EXPECT_EQ(kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, left_value->percentage_value()->value());

  CalcValue* right_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[1].get());
  const LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionOneValueWithoutKeywordValue) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // background-position: 3em;
  std::unique_ptr<PropertyListValue::Builder> background_position_builder(
      new PropertyListValue::Builder());
  background_position_builder->push_back(
      new LengthValue(3, kFontSizesAkaEmUnit));
  scoped_refptr<PropertyListValue> background_position(
      new PropertyListValue(std::move(background_position_builder)));
  computed_style->set_background_position(background_position);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> background_position_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  CalcValue* left_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[0].get());
  const LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(48.0f, left_length_value->value());
  EXPECT_EQ(kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, left_value->percentage_value()->value());

  CalcValue* right_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[1].get());
  const LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionOneKeywordValue) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // background-position: bottom;
  std::unique_ptr<PropertyListValue::Builder> background_position_builder(
      new PropertyListValue::Builder());
  background_position_builder->push_back(KeywordValue::GetBottom());
  scoped_refptr<PropertyListValue> background_position(
      new PropertyListValue(std::move(background_position_builder)));
  computed_style->set_background_position(background_position);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> background_position_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  CalcValue* left_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[0].get());
  const LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, left_length_value->value());
  EXPECT_EQ(kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, left_value->percentage_value()->value());

  CalcValue* right_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[1].get());
  const LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(1.0f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionTwoValuesWithoutKeywordValue) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // background-position: 3em 40px;
  std::unique_ptr<PropertyListValue::Builder> background_position_builder(
      new PropertyListValue::Builder());
  background_position_builder->push_back(
      new LengthValue(3, kFontSizesAkaEmUnit));
  background_position_builder->push_back(new LengthValue(40, kPixelsUnit));
  scoped_refptr<PropertyListValue> background_position(
      new PropertyListValue(std::move(background_position_builder)));
  computed_style->set_background_position(background_position);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> background_position_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  CalcValue* left_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[0].get());
  const LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(48.0f, left_length_value->value());
  EXPECT_EQ(kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, left_value->percentage_value()->value());

  CalcValue* right_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[1].get());
  const LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(40.0f, right_length_value->value());
  EXPECT_EQ(kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionTwoValuesWithOneKeyword) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // background-position: 67% center;
  std::unique_ptr<PropertyListValue::Builder> background_position_builder(
      new PropertyListValue::Builder());
  background_position_builder->push_back(new PercentageValue(0.67f));
  background_position_builder->push_back(KeywordValue::GetCenter());
  scoped_refptr<PropertyListValue> background_position(
      new PropertyListValue(std::move(background_position_builder)));
  computed_style->set_background_position(background_position);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> background_position_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  CalcValue* left_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[0].get());
  const LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, left_length_value->value());
  EXPECT_EQ(kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.67f, left_value->percentage_value()->value());

  CalcValue* right_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[1].get());
  const LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.50f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionTwoValuesWithTwoKeywords) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // background-position: right bottom;
  std::unique_ptr<PropertyListValue::Builder> background_position_builder(
      new PropertyListValue::Builder());
  background_position_builder->push_back(KeywordValue::GetRight());
  background_position_builder->push_back(KeywordValue::GetBottom());
  scoped_refptr<PropertyListValue> background_position(
      new PropertyListValue(std::move(background_position_builder)));
  computed_style->set_background_position(background_position);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> background_position_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  CalcValue* left_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[0].get());
  const LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, left_length_value->value());
  EXPECT_EQ(kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(1.0f, left_value->percentage_value()->value());

  CalcValue* right_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[1].get());
  const LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(1.0f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionTwoValuesWithTwoCenterKeywords) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // background-position: center center;
  std::unique_ptr<PropertyListValue::Builder> background_position_builder(
      new PropertyListValue::Builder());
  background_position_builder->push_back(KeywordValue::GetCenter());
  background_position_builder->push_back(KeywordValue::GetCenter());
  scoped_refptr<PropertyListValue> background_position(
      new PropertyListValue(std::move(background_position_builder)));
  computed_style->set_background_position(background_position);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> background_position_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  CalcValue* left_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[0].get());
  const LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, left_length_value->value());
  EXPECT_EQ(kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, left_value->percentage_value()->value());

  CalcValue* right_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[1].get());
  const LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionWithThreeValues) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // background-position: top 80% left;
  std::unique_ptr<PropertyListValue::Builder> background_position_builder(
      new PropertyListValue::Builder());
  background_position_builder->push_back(KeywordValue::GetTop());
  background_position_builder->push_back(new PercentageValue(0.8f));
  background_position_builder->push_back(KeywordValue::GetLeft());
  scoped_refptr<PropertyListValue> background_position(
      new PropertyListValue(std::move(background_position_builder)));
  computed_style->set_background_position(background_position);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> background_position_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  CalcValue* left_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[0].get());
  const LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, left_length_value->value());
  EXPECT_EQ(kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, left_value->percentage_value()->value());

  CalcValue* right_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[1].get());
  const LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.8f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionWithThreeValuesHaveCenter) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // background-position: center left 80%;
  std::unique_ptr<PropertyListValue::Builder> background_position_builder(
      new PropertyListValue::Builder());
  background_position_builder->push_back(KeywordValue::GetCenter());
  background_position_builder->push_back(KeywordValue::GetLeft());
  background_position_builder->push_back(new PercentageValue(0.8f));
  scoped_refptr<PropertyListValue> background_position(
      new PropertyListValue(std::move(background_position_builder)));
  computed_style->set_background_position(background_position);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> background_position_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  CalcValue* left_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[0].get());
  const LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, left_length_value->value());
  EXPECT_EQ(kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(0.8f, left_value->percentage_value()->value());

  CalcValue* right_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[1].get());
  const LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundPositionWithFourValues) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // background-position: bottom 80% right 50px;
  std::unique_ptr<PropertyListValue::Builder> background_position_builder(
      new PropertyListValue::Builder());
  background_position_builder->push_back(KeywordValue::GetBottom());
  background_position_builder->push_back(new PercentageValue(0.8f));
  background_position_builder->push_back(KeywordValue::GetRight());
  background_position_builder->push_back(new LengthValue(50, kPixelsUnit));
  scoped_refptr<PropertyListValue> background_position(
      new PropertyListValue(std::move(background_position_builder)));
  computed_style->set_background_position(background_position);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> background_position_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->background_position().get());
  ASSERT_TRUE(background_position_list);
  ASSERT_EQ(2, background_position_list->value().size());

  CalcValue* left_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[0].get());
  const LengthValue* left_length_value = left_value->length_value();
  EXPECT_FLOAT_EQ(-50.0f, left_length_value->value());
  EXPECT_EQ(kPixelsUnit, left_length_value->unit());
  EXPECT_FLOAT_EQ(1.0f, left_value->percentage_value()->value());

  CalcValue* right_value = base::polymorphic_downcast<CalcValue*>(
      background_position_list->value()[1].get());
  const LengthValue* right_length_value = right_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, right_length_value->value());
  EXPECT_EQ(kPixelsUnit, right_length_value->unit());
  EXPECT_FLOAT_EQ(0.2f, right_value->percentage_value()->value());
}

TEST(PromoteToComputedStyle, BackgroundSizeEmToPixel) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  std::unique_ptr<PropertyListValue::Builder> background_size_builder(
      new PropertyListValue::Builder());
  background_size_builder->push_back(new LengthValue(3, kFontSizesAkaEmUnit));
  background_size_builder->push_back(new LengthValue(40, kPixelsUnit));
  scoped_refptr<PropertyListValue> background_size(
      new PropertyListValue(std::move(background_size_builder)));
  computed_style->set_background_size(background_size);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> background_size_list =
      dynamic_cast<PropertyListValue*>(computed_style->background_size().get());
  ASSERT_TRUE(background_size_list);
  ASSERT_EQ(2, background_size_list->value().size());

  LengthValue* first_value = base::polymorphic_downcast<LengthValue*>(
      background_size_list->value()[0].get());
  EXPECT_FLOAT_EQ(48.0f, first_value->value());
  EXPECT_EQ(kPixelsUnit, first_value->unit());

  LengthValue* second_value = base::polymorphic_downcast<LengthValue*>(
      background_size_list->value()[1].get());
  EXPECT_FLOAT_EQ(40.0f, second_value->value());
  EXPECT_EQ(kPixelsUnit, second_value->unit());
}

TEST(PromoteToComputedStyle, BackgroundSizeKeywordNotChanged) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_background_size(KeywordValue::GetContain());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_EQ(KeywordValue::GetContain(), computed_style->background_size());
}

TEST(PromoteToComputedStyle, BorderRadiusEmToPixel) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_border_top_left_radius(
      new LengthValue(3, kFontSizesAkaEmUnit));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  LengthValue* border_top_left_radius =
      base::polymorphic_downcast<LengthValue*>(
          computed_style->border_top_left_radius().get());
  ASSERT_TRUE(border_top_left_radius);
  EXPECT_FLOAT_EQ(48.0f, border_top_left_radius->value());
  EXPECT_EQ(kPixelsUnit, border_top_left_radius->unit());
}

TEST(PromoteToComputedStyle, BorderColorWithInitialValue) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_border_bottom_color(KeywordValue::GetInitial());
  computed_style->set_color(RGBAColorValue::GetAqua());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<RGBAColorValue> border_bottom_color =
      dynamic_cast<RGBAColorValue*>(
          computed_style->border_bottom_color().get());
  ASSERT_TRUE(border_bottom_color);
  EXPECT_EQ(0x00FFFFFF, border_bottom_color->value());
}

TEST(PromoteToComputedStyle, BorderColorWithCurrentColorValue) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_border_left_color(KeywordValue::GetCurrentColor());
  computed_style->set_color(RGBAColorValue::GetAqua());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<RGBAColorValue> border_color =
      dynamic_cast<RGBAColorValue*>(computed_style->border_left_color().get());
  ASSERT_TRUE(border_color);
  EXPECT_EQ(0x00FFFFFF, border_color->value());
}

TEST(PromoteToComputedStyle, BorderWidthWithBorderStyleNone) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_border_top_style(KeywordValue::GetNone());
  computed_style->set_border_top_width(new LengthValue(2, kFontSizesAkaEmUnit));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<LengthValue> border_top_width =
      dynamic_cast<LengthValue*>(computed_style->border_top_width().get());
  ASSERT_TRUE(border_top_width);
  EXPECT_EQ(0, border_top_width->value());
  EXPECT_EQ(kPixelsUnit, border_top_width->unit());
}

TEST(PromoteToComputedStyle, BorderWidthInEmShouldBeComputedAfterFontSize) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_border_left_style(KeywordValue::GetSolid());
  computed_style->set_font_size(new LengthValue(2, kFontSizesAkaEmUnit));
  computed_style->set_border_left_width(
      new LengthValue(2, kFontSizesAkaEmUnit));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_font_size(new LengthValue(100, kPixelsUnit));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<LengthValue> border_left_width =
      dynamic_cast<LengthValue*>(computed_style->border_left_width().get());
  EXPECT_EQ(400, border_left_width->value());
  EXPECT_EQ(kPixelsUnit, border_left_width->unit());
}

TEST(PromoteToComputedStyle, BoxShadowWithEmLengthAndColor) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  std::unique_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths;
  lengths.push_back(new LengthValue(100, kPixelsUnit));
  lengths.push_back(new LengthValue(10, kFontSizesAkaEmUnit));

  scoped_refptr<ShadowValue> shadow(
      new ShadowValue(lengths, RGBAColorValue::GetAqua(), false));
  builder->push_back(shadow);

  computed_style->set_box_shadow(new PropertyListValue(std::move(builder)));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_font_size(new LengthValue(50, kPixelsUnit));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> box_shadow_list =
      dynamic_cast<PropertyListValue*>(computed_style->box_shadow().get());
  DCHECK_EQ(1u, box_shadow_list->value().size());

  scoped_refptr<ShadowValue> box_shadow =
      dynamic_cast<ShadowValue*>(box_shadow_list->value()[0].get());

  EXPECT_EQ(100.0f, box_shadow->offset_x()->value());
  EXPECT_EQ(kPixelsUnit, box_shadow->offset_x()->unit());

  EXPECT_EQ(500.0f, box_shadow->offset_y()->value());
  EXPECT_EQ(kPixelsUnit, box_shadow->offset_y()->unit());

  scoped_refptr<RGBAColorValue> color =
      dynamic_cast<RGBAColorValue*>(box_shadow->color().get());
  ASSERT_TRUE(color);
  EXPECT_EQ(0x00FFFFFF, color->value());

  EXPECT_FALSE(box_shadow->has_inset());
}

TEST(PromoteToComputedStyle, BoxShadowWithInset) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  std::unique_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths;
  lengths.push_back(new LengthValue(100, kPixelsUnit));
  lengths.push_back(new LengthValue(30, kPixelsUnit));

  scoped_refptr<ShadowValue> shadow(
      new ShadowValue(lengths, RGBAColorValue::GetAqua(), true /*has_inset*/));
  builder->push_back(shadow);

  computed_style->set_box_shadow(new PropertyListValue(std::move(builder)));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> box_shadow_list =
      dynamic_cast<PropertyListValue*>(computed_style->box_shadow().get());
  DCHECK_EQ(1u, box_shadow_list->value().size());

  scoped_refptr<ShadowValue> box_shadow =
      dynamic_cast<ShadowValue*>(box_shadow_list->value()[0].get());

  EXPECT_EQ(100.0f, box_shadow->offset_x()->value());
  EXPECT_EQ(kPixelsUnit, box_shadow->offset_x()->unit());

  EXPECT_EQ(30.0f, box_shadow->offset_y()->value());
  EXPECT_EQ(kPixelsUnit, box_shadow->offset_y()->unit());

  scoped_refptr<RGBAColorValue> color =
      dynamic_cast<RGBAColorValue*>(box_shadow->color().get());
  ASSERT_TRUE(color);
  EXPECT_EQ(0x00FFFFFF, color->value());

  EXPECT_TRUE(box_shadow->has_inset());
}

TEST(PromoteToComputedStyle, BoxShadowWithoutColor) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  std::unique_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths;
  lengths.push_back(new LengthValue(100, kPixelsUnit));
  lengths.push_back(new LengthValue(200, kPixelsUnit));

  scoped_refptr<ShadowValue> shadow(new ShadowValue(lengths, NULL, false));
  builder->push_back(shadow);

  computed_style->set_box_shadow(new PropertyListValue(std::move(builder)));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_color(new RGBAColorValue(0x0047abff));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> box_shadow_list =
      dynamic_cast<PropertyListValue*>(computed_style->box_shadow().get());
  DCHECK_EQ(1u, box_shadow_list->value().size());

  scoped_refptr<ShadowValue> box_shadow =
      dynamic_cast<ShadowValue*>(box_shadow_list->value()[0].get());

  EXPECT_EQ(100.0f, box_shadow->offset_x()->value());
  EXPECT_EQ(kPixelsUnit, box_shadow->offset_x()->unit());

  EXPECT_EQ(200.0f, box_shadow->offset_y()->value());
  EXPECT_EQ(kPixelsUnit, box_shadow->offset_y()->unit());

  ASSERT_TRUE(box_shadow->color());
  EXPECT_EQ(0x0047abff, box_shadow->color()->value());

  EXPECT_FALSE(box_shadow->has_inset());
}

TEST(PromoteToComputedStyle, BoxShadowWithShadowList) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  std::unique_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths_1;
  lengths_1.push_back(new LengthValue(100, kPixelsUnit));
  lengths_1.push_back(new LengthValue(30, kPixelsUnit));
  lengths_1.push_back(new LengthValue(3, kFontSizesAkaEmUnit));
  scoped_refptr<ShadowValue> shadow_1(
      new ShadowValue(lengths_1, NULL, true /*has_inset*/));
  builder->push_back(shadow_1);

  std::vector<scoped_refptr<LengthValue> > lengths_2;
  lengths_2.push_back(new LengthValue(2, kFontSizesAkaEmUnit));
  lengths_2.push_back(new LengthValue(40, kPixelsUnit));
  scoped_refptr<ShadowValue> shadow_2(new ShadowValue(
      lengths_2, RGBAColorValue::GetNavy(), false /*has_inset*/));

  builder->push_back(shadow_2);

  computed_style->set_box_shadow(new PropertyListValue(std::move(builder)));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_font_size(new LengthValue(50, kPixelsUnit));
  parent_computed_style->set_color(new RGBAColorValue(0x0047ABFF));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> box_shadow_list =
      dynamic_cast<PropertyListValue*>(computed_style->box_shadow().get());
  DCHECK_EQ(2u, box_shadow_list->value().size());

  float expected_length_value[2][3] = {
      {100.0f, 30.0f, 150.0f}, {100.0f, 40.0f, 0.0f},
  };
  float expected_color[2] = {0x0047ABFF, 0x000080FF};
  bool expected_has_inset[2] = {true, false};

  for (size_t i = 0; i < box_shadow_list->value().size(); ++i) {
    scoped_refptr<ShadowValue> box_shadow =
        dynamic_cast<ShadowValue*>(box_shadow_list->value()[i].get());

    if (i == 0) {
      EXPECT_TRUE(box_shadow->offset_x());
      EXPECT_TRUE(box_shadow->offset_y());
      EXPECT_TRUE(box_shadow->blur_radius());
      EXPECT_FALSE(box_shadow->spread_radius());
    } else {
      EXPECT_TRUE(box_shadow->offset_x());
      EXPECT_TRUE(box_shadow->offset_y());
      EXPECT_FALSE(box_shadow->blur_radius());
      EXPECT_FALSE(box_shadow->spread_radius());
    }

    for (int j = 0; j < ShadowValue::kMaxLengths; ++j) {
      scoped_refptr<LengthValue> length = box_shadow->lengths()[j];
      if (length) {
        EXPECT_EQ(expected_length_value[i][j], length->value());
        EXPECT_EQ(kPixelsUnit, length->unit());
      }
    }

    ASSERT_TRUE(box_shadow->color());
    EXPECT_EQ(expected_color[i], box_shadow->color()->value());

    EXPECT_EQ(expected_has_inset[i], box_shadow->has_inset());
  }
}

TEST(PromoteToComputedStyle, BoxShadowWithNone) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_box_shadow(KeywordValue::GetNone());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_EQ(KeywordValue::GetNone(), computed_style->box_shadow());
}

TEST(PromoteToComputedStyle,
     DisplayIsNotBlockifiedForInitialPositionInFlowInlineElement) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetInline());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_TRUE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetInline(), computed_style->display());
}

TEST(PromoteToComputedStyle,
     DisplayIsNotBlockifiedForRelativePositionInFlowBlockElement) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetInline());
  computed_style->set_position(KeywordValue::GetRelative());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_TRUE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetInline(), computed_style->display());
}

TEST(PromoteToComputedStyle,
     DisplayIsNotInlinifiedForRelativePositionInFlowBlockElement) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetBlock());
  computed_style->set_position(KeywordValue::GetRelative());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_FALSE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle,
     DisplayIsNotBlockifiedForStaticPositionInFlowBlockElement) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetInline());
  computed_style->set_position(KeywordValue::GetStatic());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_TRUE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetInline(), computed_style->display());
}

TEST(PromoteToComputedStyle,
     DisplayIsNotInlinifiedForStaticPositionInFlowBlockElement) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetBlock());
  computed_style->set_position(KeywordValue::GetStatic());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_FALSE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle, DisplayIsNotInlinifiedForInFlowBlockElement) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetBlock());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_FALSE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle,
     DisplayIsBlockifiedForAbsolutelyPositionedInlineElement) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetInline());
  computed_style->set_position(KeywordValue::GetAbsolute());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_TRUE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle,
     DisplayIsNotInlinifiedForAbsolutelyPositionedBlockElement) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetBlock());
  computed_style->set_position(KeywordValue::GetAbsolute());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_FALSE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle,
     DisplayIsBlockifiedForFixedPositionedInlineElement) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetInline());
  computed_style->set_position(KeywordValue::GetFixed());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_TRUE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle,
     DisplayIsNotInlinifiedForFixedPositionedBlockElement) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetBlock());
  computed_style->set_position(KeywordValue::GetFixed());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_FALSE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle, DisplayNotSetIsBlockified) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_position(KeywordValue::GetAbsolute());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_TRUE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle, DisplayInitialIsBlockified) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_position(KeywordValue::GetInitial());
  computed_style->set_position(KeywordValue::GetAbsolute());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_TRUE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle, DisplayInheritIsBlockified) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_position(KeywordValue::GetInherit());
  computed_style->set_position(KeywordValue::GetAbsolute());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_TRUE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle, DisplayInlineBlockIsBlockified) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetInlineBlock());
  computed_style->set_position(KeywordValue::GetAbsolute());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_TRUE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle, DisplayInlineFlexIsBlockified) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetInlineFlex());
  computed_style->set_position(KeywordValue::GetAbsolute());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_TRUE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetFlex(), computed_style->display());
}

TEST(PromoteToComputedStyle, DisplayIsBlockifiedForFlexItemInline) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetInline());

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_display(KeywordValue::GetInlineFlex());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_TRUE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle, DisplayNotInlinifiedForFlexItemBlock) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetBlock());

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_display(KeywordValue::GetFlex());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_FALSE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle,
     DisplayIsBlockifiedForAbsolutelyPositionedFlexContainerChild) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetInline());
  computed_style->set_position(KeywordValue::GetAbsolute());

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_display(KeywordValue::GetFlex());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_TRUE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle,
     DisplayIsBlockifiedForFixedPositionedFlexContainerChild) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_display(KeywordValue::GetInline());
  computed_style->set_position(KeywordValue::GetFixed());

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_display(KeywordValue::GetFlex());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_TRUE(computed_style->is_inline_before_blockification());
  EXPECT_EQ(KeywordValue::GetBlock(), computed_style->display());
}

TEST(PromoteToComputedStyle, OrderInteger) {
  // The computed value for order is the specified integer.
  //   https://www.w3.org/TR/css-flexbox-1/#order-property
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_order(new IntegerValue(-5));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  IntegerValue* order =
      base::polymorphic_downcast<IntegerValue*>(computed_style->order().get());
  ASSERT_TRUE(order);
  EXPECT_FLOAT_EQ(-5, order->value());
}

TEST(PromoteToComputedStyle, OutlineColorWithCurrentColorValue) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_outline_color(KeywordValue::GetCurrentColor());
  computed_style->set_color(RGBAColorValue::GetAqua());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<RGBAColorValue> outline_color =
      dynamic_cast<RGBAColorValue*>(computed_style->outline_color().get());
  ASSERT_TRUE(outline_color);
  EXPECT_EQ(0x00FFFFFF, outline_color->value());
}

TEST(PromoteToComputedStyle, OutlineWidthWithOutlineStyleNone) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_outline_style(KeywordValue::GetNone());
  computed_style->set_outline_width(new LengthValue(2, kFontSizesAkaEmUnit));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<LengthValue> outline_width =
      dynamic_cast<LengthValue*>(computed_style->outline_width().get());
  ASSERT_TRUE(outline_width);
  EXPECT_EQ(0, outline_width->value());
  EXPECT_EQ(kPixelsUnit, outline_width->unit());
}

TEST(PromoteToComputedStyle, OutlineWidthInEmShouldBeComputedAfterFontSize) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_outline_style(KeywordValue::GetSolid());
  computed_style->set_font_size(new LengthValue(2, kFontSizesAkaEmUnit));
  computed_style->set_outline_width(new LengthValue(2, kFontSizesAkaEmUnit));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_font_size(new LengthValue(100, kPixelsUnit));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<LengthValue> outline_width =
      dynamic_cast<LengthValue*>(computed_style->outline_width().get());
  EXPECT_EQ(400, outline_width->value());
  EXPECT_EQ(kPixelsUnit, outline_width->unit());
}

TEST(PromoteToComputedStyle, TextDecorationWithCurrentColor) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_text_decoration_color(KeywordValue::GetCurrentColor());
  computed_style->set_color(RGBAColorValue::GetAqua());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<RGBAColorValue> text_decoration_color =
      dynamic_cast<RGBAColorValue*>(
          computed_style->text_decoration_color().get());
  ASSERT_TRUE(text_decoration_color);
  EXPECT_EQ(0x00FFFFFF, text_decoration_color->value());
}

TEST(PromoteToComputedStyle, TextShadowWithEmLengthAndColor) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  std::unique_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths;
  lengths.push_back(new LengthValue(100, kPixelsUnit));
  lengths.push_back(new LengthValue(10, kFontSizesAkaEmUnit));

  scoped_refptr<ShadowValue> shadow(
      new ShadowValue(lengths, RGBAColorValue::GetAqua(), false));
  builder->push_back(shadow);

  computed_style->set_text_shadow(new PropertyListValue(std::move(builder)));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_font_size(new LengthValue(50, kPixelsUnit));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> text_shadow_list =
      dynamic_cast<PropertyListValue*>(computed_style->text_shadow().get());
  DCHECK_EQ(1u, text_shadow_list->value().size());

  scoped_refptr<ShadowValue> text_shadow =
      dynamic_cast<ShadowValue*>(text_shadow_list->value()[0].get());

  EXPECT_EQ(100.0f, text_shadow->offset_x()->value());
  EXPECT_EQ(kPixelsUnit, text_shadow->offset_x()->unit());

  EXPECT_EQ(500.0f, text_shadow->offset_y()->value());
  EXPECT_EQ(kPixelsUnit, text_shadow->offset_y()->unit());

  ASSERT_TRUE(text_shadow->color());
  EXPECT_EQ(0x00FFFFFF, text_shadow->color()->value());

  EXPECT_FALSE(text_shadow->has_inset());
}

TEST(PromoteToComputedStyle, TextShadowWithoutColor) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  std::unique_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths;
  lengths.push_back(new LengthValue(100, kPixelsUnit));
  lengths.push_back(new LengthValue(200, kPixelsUnit));

  scoped_refptr<ShadowValue> shadow(new ShadowValue(lengths, NULL, false));
  builder->push_back(shadow);

  computed_style->set_text_shadow(new PropertyListValue(std::move(builder)));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_color(new RGBAColorValue(0x0047abff));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> text_shadow_list =
      dynamic_cast<PropertyListValue*>(computed_style->text_shadow().get());
  DCHECK_EQ(1u, text_shadow_list->value().size());

  scoped_refptr<ShadowValue> text_shadow =
      dynamic_cast<ShadowValue*>(text_shadow_list->value()[0].get());

  EXPECT_EQ(100.0f, text_shadow->offset_x()->value());
  EXPECT_EQ(kPixelsUnit, text_shadow->offset_x()->unit());

  EXPECT_EQ(200.0f, text_shadow->offset_y()->value());
  EXPECT_EQ(kPixelsUnit, text_shadow->offset_y()->unit());

  ASSERT_TRUE(text_shadow->color());
  EXPECT_EQ(0x0047abff, text_shadow->color()->value());

  EXPECT_FALSE(text_shadow->has_inset());
}

TEST(PromoteToComputedStyle, TextShadowWithShadowList) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  std::unique_ptr<PropertyListValue::Builder> builder(
      new PropertyListValue::Builder());

  std::vector<scoped_refptr<LengthValue> > lengths_1;
  lengths_1.push_back(new LengthValue(100, kPixelsUnit));
  lengths_1.push_back(new LengthValue(30, kPixelsUnit));
  lengths_1.push_back(new LengthValue(3, kFontSizesAkaEmUnit));
  scoped_refptr<ShadowValue> shadow_1(new ShadowValue(lengths_1, NULL, false));
  builder->push_back(shadow_1);

  std::vector<scoped_refptr<LengthValue> > lengths_2;
  lengths_2.push_back(new LengthValue(2, kFontSizesAkaEmUnit));
  lengths_2.push_back(new LengthValue(40, kPixelsUnit));
  scoped_refptr<ShadowValue> shadow_2(
      new ShadowValue(lengths_2, RGBAColorValue::GetNavy(), false));

  builder->push_back(shadow_2);

  computed_style->set_text_shadow(new PropertyListValue(std::move(builder)));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_font_size(new LengthValue(50, kPixelsUnit));
  parent_computed_style->set_color(new RGBAColorValue(0x0047ABFF));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> text_shadow_list =
      dynamic_cast<PropertyListValue*>(computed_style->text_shadow().get());
  DCHECK_EQ(2u, text_shadow_list->value().size());

  float expected_length_value[2][3] = {
      {100.0f, 30.0f, 150.0f}, {100.0f, 40.0f, 0.0f},
  };
  float expected_color[2] = {0x0047ABFF, 0x000080FF};

  for (size_t i = 0; i < text_shadow_list->value().size(); ++i) {
    scoped_refptr<ShadowValue> text_shadow =
        dynamic_cast<ShadowValue*>(text_shadow_list->value()[i].get());

    if (i == 0) {
      EXPECT_TRUE(text_shadow->offset_x());
      EXPECT_TRUE(text_shadow->offset_y());
      EXPECT_TRUE(text_shadow->blur_radius());
      EXPECT_FALSE(text_shadow->spread_radius());
    } else {
      EXPECT_TRUE(text_shadow->offset_x());
      EXPECT_TRUE(text_shadow->offset_y());
      EXPECT_FALSE(text_shadow->blur_radius());
      EXPECT_FALSE(text_shadow->spread_radius());
    }

    for (size_t j = 0; j < ShadowValue::kMaxLengths; ++j) {
      scoped_refptr<LengthValue> length = text_shadow->lengths()[j];
      if (length) {
        EXPECT_EQ(expected_length_value[i][j], length->value());
        EXPECT_EQ(kPixelsUnit, length->unit());
      }
    }

    ASSERT_TRUE(text_shadow->color());
    EXPECT_EQ(expected_color[i], text_shadow->color()->value());

    EXPECT_FALSE(text_shadow->has_inset());
  }
}

TEST(PromoteToComputedStyle, TextShadowWithNone) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  computed_style->set_text_shadow(KeywordValue::GetNone());

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_EQ(KeywordValue::GetNone(), computed_style->text_shadow());
}

TEST(PromoteToComputedStyle, HeightPercentageInUnspecifiedHeightBlockIsAuto) {
  // If the height is specified as a percentage and the height of the containing
  // block is not specified explicitly, and this element is not absolutely
  // positioned, the value computes to 'auto'.
  //   https://www.w3.org/TR/CSS2/visudet.html#the-height-property
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_height(new PercentageValue(0.50f));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  EXPECT_EQ(KeywordValue::GetAuto(), parent_computed_style->height());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_EQ(KeywordValue::GetAuto(), computed_style->height());
}

TEST(PromoteToComputedStyle,
     MaxHeightPercentageInUnspecifiedHeightBlockIsNone) {
  // If the max-height is specified as a percentage and the height of the
  // containing block is not specified explicitly, and this element is not
  // absolutely positioned, the percentage value is treated as '0'.
  //   https://www.w3.org/TR/CSS2/visudet.html#propdef-max-height
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_max_height(new PercentageValue(0.50f));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  EXPECT_EQ(KeywordValue::GetAuto(), parent_computed_style->height());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  EXPECT_EQ(KeywordValue::GetNone(), computed_style->max_height());
}

TEST(PromoteToComputedStyle,
     MinHeightPercentageInUnspecifiedHeightBlockIsZero) {
  // If the min-height is specified as a percentage and the height of the
  // containing block is not specified explicitly, and this element is not
  // absolutely positioned, the percentage value is treated as 'none'.
  //   https://www.w3.org/TR/CSS2/visudet.html#propdef-min-height
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_min_height(new PercentageValue(0.50f));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  EXPECT_EQ(KeywordValue::GetAuto(), parent_computed_style->height());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  LengthValue* computed_min_height = base::polymorphic_downcast<LengthValue*>(
      computed_style->min_height().get());
  EXPECT_EQ(0, computed_min_height->value());
  EXPECT_EQ(kPixelsUnit, computed_min_height->unit());
}

TEST(PromoteToComputedStyle, MaxWidthPercentageInNegativeWidthBlockIsZero) {
  // If the max-width is specified as a percentage and the containing block's
  // width is negative, the used value is zero.
  //  https://www.w3.org/TR/CSS2/visudet.html#propdef-max-width
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_max_width(new PercentageValue(0.50f));

  scoped_refptr<CSSComputedStyleData> grandparent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration>
      grandparent_computed_style_declaration(
          CreateComputedStyleDeclaration(grandparent_computed_style));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_width(new LengthValue(-16, kPixelsUnit));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(parent_computed_style,
                         grandparent_computed_style_declaration,
                         grandparent_computed_style, kNullSize, NULL);
  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         grandparent_computed_style, kNullSize, NULL);

  LengthValue* computed_max_width = base::polymorphic_downcast<LengthValue*>(
      computed_style->max_width().get());
  EXPECT_EQ(0, computed_max_width->value());
  EXPECT_EQ(kPixelsUnit, computed_max_width->unit());
}

TEST(PromoteToComputedStyle, MinWidthPercentageInNegativeWidthBlockIsZero) {
  // If the min-width is specified as a percentage and the containing block's
  // width is negative, the used value is zero.
  //  https://www.w3.org/TR/CSS2/visudet.html#propdef-min-width
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_min_width(new PercentageValue(0.50f));

  scoped_refptr<CSSComputedStyleData> grandparent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration>
      grandparent_computed_style_declaration(
          CreateComputedStyleDeclaration(grandparent_computed_style));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_width(new LengthValue(-16, kPixelsUnit));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(parent_computed_style,
                         grandparent_computed_style_declaration,
                         grandparent_computed_style, kNullSize, NULL);
  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         grandparent_computed_style, kNullSize, NULL);

  LengthValue* computed_min_width = base::polymorphic_downcast<LengthValue*>(
      computed_style->min_width().get());
  EXPECT_EQ(0, computed_min_width->value());
  EXPECT_EQ(kPixelsUnit, computed_min_width->unit());
}

TEST(PromoteToComputedStyle, LineHeightPercentageIsRelativeToFontSize) {
  // The computed value of the property is this percentage multiplied by the
  // element's computed font size. Negative values are illegal.
  //   https://www.w3.org/TR/CSS21/visudet.html#line-height
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());
  computed_style->set_font_size(new LengthValue(100, kPixelsUnit));
  computed_style->set_line_height(new PercentageValue(0.75f));

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  LengthValue* computed_line_height = base::polymorphic_downcast<LengthValue*>(
      computed_style->line_height().get());
  EXPECT_EQ(75, computed_line_height->value());
  EXPECT_EQ(kPixelsUnit, computed_line_height->unit());
}

TEST(PromoteToComputedStyle, TransformOriginOneValueWithKeyword) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // transform-origin: bottom;
  std::unique_ptr<PropertyListValue::Builder> transform_origin_builder(
      new PropertyListValue::Builder());
  transform_origin_builder->push_back(KeywordValue::GetBottom());
  scoped_refptr<PropertyListValue> transform_origin(
      new PropertyListValue(std::move(transform_origin_builder)));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> transform_origin_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  CalcValue* first_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[0].get());
  const LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, first_length_value->value());
  EXPECT_EQ(kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, first_value->percentage_value()->value());

  CalcValue* second_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[1].get());
  const LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, second_length_value->value());
  EXPECT_EQ(kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(1.0f, second_value->percentage_value()->value());

  LengthValue* third_value = base::polymorphic_downcast<LengthValue*>(
      transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(0.0f, third_value->value());
  EXPECT_EQ(kPixelsUnit, third_value->unit());
}

TEST(PromoteToComputedStyle, TransformOriginOneValueWithoutKeyword) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // transform-origin: 3em;
  std::unique_ptr<PropertyListValue::Builder> transform_origin_builder(
      new PropertyListValue::Builder());
  transform_origin_builder->push_back(new LengthValue(3, kFontSizesAkaEmUnit));
  scoped_refptr<PropertyListValue> transform_origin(
      new PropertyListValue(std::move(transform_origin_builder)));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> transform_origin_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  CalcValue* first_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[0].get());
  const LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(48.0f, first_length_value->value());
  EXPECT_EQ(kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, first_value->percentage_value()->value());

  CalcValue* second_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[1].get());
  const LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, second_length_value->value());
  EXPECT_EQ(kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, second_value->percentage_value()->value());

  LengthValue* third_value = base::polymorphic_downcast<LengthValue*>(
      transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(0.0f, third_value->value());
  EXPECT_EQ(kPixelsUnit, third_value->unit());
}

TEST(PromoteToComputedStyle, TransformOriginTwoValuesWithoutKeywordValue) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // transform-origin: 3em 40px;
  std::unique_ptr<PropertyListValue::Builder> transform_origin_builder(
      new PropertyListValue::Builder());
  transform_origin_builder->push_back(new LengthValue(3, kFontSizesAkaEmUnit));
  transform_origin_builder->push_back(new LengthValue(40, kPixelsUnit));
  scoped_refptr<PropertyListValue> transform_origin(
      new PropertyListValue(std::move(transform_origin_builder)));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> transform_origin_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  CalcValue* first_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[0].get());
  const LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(48.0f, first_length_value->value());
  EXPECT_EQ(kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, first_value->percentage_value()->value());

  CalcValue* second_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[1].get());
  const LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(40.0f, second_length_value->value());
  EXPECT_EQ(kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, second_value->percentage_value()->value());

  LengthValue* third_value = base::polymorphic_downcast<LengthValue*>(
      transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(0.0f, third_value->value());
  EXPECT_EQ(kPixelsUnit, third_value->unit());
}

TEST(PromoteToComputedStyle, TransformOriginTwoValuesWithOneKeyword) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // transform-origin: right 20%;
  std::unique_ptr<PropertyListValue::Builder> transform_origin_builder(
      new PropertyListValue::Builder());
  transform_origin_builder->push_back(KeywordValue::GetRight());
  transform_origin_builder->push_back(new PercentageValue(0.2f));
  scoped_refptr<PropertyListValue> transform_origin(
      new PropertyListValue(std::move(transform_origin_builder)));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> transform_origin_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  CalcValue* first_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[0].get());
  const LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, first_length_value->value());
  EXPECT_EQ(kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(1.0f, first_value->percentage_value()->value());

  CalcValue* second_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[1].get());
  const LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, second_length_value->value());
  EXPECT_EQ(kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(0.2f, second_value->percentage_value()->value());

  LengthValue* third_value = base::polymorphic_downcast<LengthValue*>(
      transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(0.0f, third_value->value());
  EXPECT_EQ(kPixelsUnit, third_value->unit());
}

TEST(PromoteToComputedStyle, TransformOriginTwoValuesWithoutKeyword) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // transform-origin: 60% 80%;
  std::unique_ptr<PropertyListValue::Builder> transform_origin_builder(
      new PropertyListValue::Builder());
  transform_origin_builder->push_back(new PercentageValue(0.6f));
  transform_origin_builder->push_back(new PercentageValue(0.8f));
  scoped_refptr<PropertyListValue> transform_origin(
      new PropertyListValue(std::move(transform_origin_builder)));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> transform_origin_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  CalcValue* first_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[0].get());
  const LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, first_length_value->value());
  EXPECT_EQ(kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(0.6f, first_value->percentage_value()->value());

  CalcValue* second_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[1].get());
  const LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, second_length_value->value());
  EXPECT_EQ(kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(0.8f, second_value->percentage_value()->value());

  LengthValue* third_value = base::polymorphic_downcast<LengthValue*>(
      transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(0.0f, third_value->value());
  EXPECT_EQ(kPixelsUnit, third_value->unit());
}

TEST(PromoteToComputedStyle, TransformOriginTwoKeywordValues) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // transform-origin: top center;
  std::unique_ptr<PropertyListValue::Builder> transform_origin_builder(
      new PropertyListValue::Builder());
  transform_origin_builder->push_back(KeywordValue::GetTop());
  transform_origin_builder->push_back(KeywordValue::GetCenter());
  scoped_refptr<PropertyListValue> transform_origin(
      new PropertyListValue(std::move(transform_origin_builder)));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> transform_origin_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  CalcValue* first_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[0].get());
  const LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, first_length_value->value());
  EXPECT_EQ(kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(0.5f, first_value->percentage_value()->value());

  CalcValue* second_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[1].get());
  const LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, second_length_value->value());
  EXPECT_EQ(kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, second_value->percentage_value()->value());

  LengthValue* third_value = base::polymorphic_downcast<LengthValue*>(
      transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(0.0f, third_value->value());
  EXPECT_EQ(kPixelsUnit, third_value->unit());
}

TEST(PromoteToComputedStyle, TransformOriginThreeValues) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // transform-origin: 30px top 50px;
  std::unique_ptr<PropertyListValue::Builder> transform_origin_builder(
      new PropertyListValue::Builder());
  transform_origin_builder->push_back(new LengthValue(30.0f, kPixelsUnit));
  transform_origin_builder->push_back(KeywordValue::GetTop());
  transform_origin_builder->push_back(new LengthValue(50.0f, kPixelsUnit));
  scoped_refptr<PropertyListValue> transform_origin(
      new PropertyListValue(std::move(transform_origin_builder)));
  computed_style->set_transform_origin(transform_origin);

  scoped_refptr<CSSComputedStyleData> parent_computed_style(
      new CSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<PropertyListValue> transform_origin_list =
      dynamic_cast<PropertyListValue*>(
          computed_style->transform_origin().get());
  ASSERT_TRUE(transform_origin_list);
  ASSERT_EQ(3, transform_origin_list->value().size());

  CalcValue* first_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[0].get());
  const LengthValue* first_length_value = first_value->length_value();
  EXPECT_FLOAT_EQ(30.0f, first_length_value->value());
  EXPECT_EQ(kPixelsUnit, first_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, first_value->percentage_value()->value());

  CalcValue* second_value = base::polymorphic_downcast<CalcValue*>(
      transform_origin_list->value()[1].get());
  const LengthValue* second_length_value = second_value->length_value();
  EXPECT_FLOAT_EQ(0.0f, second_length_value->value());
  EXPECT_EQ(kPixelsUnit, second_length_value->unit());
  EXPECT_FLOAT_EQ(0.0f, second_value->percentage_value()->value());

  LengthValue* third_value = base::polymorphic_downcast<LengthValue*>(
      transform_origin_list->value()[2].get());
  EXPECT_FLOAT_EQ(50.0f, third_value->value());
  EXPECT_EQ(kPixelsUnit, third_value->unit());
}

TEST(PromoteToComputedStyle, TransformRelativeUnitShouldBeConvertedToAbsolute) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  // transform: translateX(2em);
  TransformFunctionListValue::Builder transform_builder;
  transform_builder.emplace_back(new TranslateFunction(
      TranslateFunction::kXAxis, new LengthValue(2.0f, kFontSizesAkaEmUnit)));
  scoped_refptr<TransformFunctionListValue> transform(
      new TransformFunctionListValue(std::move(transform_builder)));
  computed_style->set_transform(transform);

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_font_size(new LengthValue(100, kPixelsUnit));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  scoped_refptr<TransformFunctionListValue> computed_transform =
      dynamic_cast<TransformFunctionListValue*>(
          computed_style->transform().get());
  ASSERT_TRUE(computed_transform);
  ASSERT_EQ(1, computed_transform->value().size());

  const TranslateFunction* computed_function =
      base::polymorphic_downcast<const TranslateFunction*>(
          computed_transform->value()[0].get());
  ASSERT_TRUE(computed_function);
  EXPECT_FLOAT_EQ(200.0f, computed_function->offset_as_length()->value());
  EXPECT_EQ(kPixelsUnit, computed_function->offset_as_length()->unit());
}

TEST(PromoteToComputedStyle,
     InheritedAnimatablePropertyShouldAlwaysBeDeclared) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_color(RGBAColorValue::GetAqua());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  // Verify that we're testing an inherited and animatable property.
  ASSERT_TRUE(GetPropertyInheritance(kColorProperty) &&
              GetPropertyAnimatable(kColorProperty));

  // For each inherited, animatable property, the property value should be set
  // to inherited if it is not already declared in PromoteToComputedStyle().
  // This causes the value to be  explicitly set within the CSSComputedStyleData
  // and ensures that the original value will be available for transitions
  // (which need to know the before and after state of the property) even when
  // the property is inherited from a parent that has changed.
  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  // Verify that the color is declared in the child even though it was not
  // explicitly declared.
  ASSERT_TRUE(computed_style->IsDeclared(kColorProperty));
}

TEST(PromoteToComputedStyle,
     DeclaredPropertiesInheritedFromParentShouldRemainValidWhenSetToSameValue) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_color(
      new RGBAColorValue(uint8(10), uint8(10), uint8(10), uint8(10)));
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  // Verify that we're testing an inherited and animatable property.
  ASSERT_TRUE(GetPropertyInheritance(kColorProperty) &&
              GetPropertyAnimatable(kColorProperty));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  // Verify that the declared properties inherited from the parent are valid.
  ASSERT_TRUE(computed_style->AreDeclaredPropertiesInheritedFromParentValid());

  parent_computed_style = new MutableCSSComputedStyleData();
  parent_computed_style->set_color(
      new RGBAColorValue(uint8(10), uint8(10), uint8(10), uint8(10)));
  parent_computed_style_declaration->SetData(parent_computed_style);

  // Verify that the declared properties inherited from the parent are still
  // valid.
  ASSERT_TRUE(computed_style->AreDeclaredPropertiesInheritedFromParentValid());
}

TEST(PromoteToComputedStyle,
     InheritedAnimatablePropertyShouldRetainValueAfterParentValueChanges) {
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  parent_computed_style->set_color(RGBAColorValue::GetAqua());
  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));

  // Verify that we're testing an inherited and animatable property.
  ASSERT_TRUE(GetPropertyInheritance(kColorProperty) &&
              GetPropertyAnimatable(kColorProperty));

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         parent_computed_style, kNullSize, NULL);

  // Verify that the child's color is Aqua and matches the parent's color.
  EXPECT_EQ(RGBAColorValue::GetAqua(), computed_style->color());
  EXPECT_EQ(parent_computed_style->color(), computed_style->color());

  // Verify that the declared properties inherited from the parent are valid.
  ASSERT_TRUE(computed_style->AreDeclaredPropertiesInheritedFromParentValid());

  parent_computed_style = new MutableCSSComputedStyleData();
  parent_computed_style->set_color(RGBAColorValue::GetNavy());
  parent_computed_style_declaration->SetData(parent_computed_style);

  // Verify that the color is still Aqua but that it no longer matches the
  // parent's color.
  EXPECT_EQ(RGBAColorValue::GetAqua(), computed_style->color());
  EXPECT_NE(parent_computed_style->color(), computed_style->color());

  // Verify that the declared properties inherited from the parent are no
  // longer valid.
  ASSERT_FALSE(computed_style->AreDeclaredPropertiesInheritedFromParentValid());
}

TEST(UpdateInheritedData,
     UpdateInheritedDataShouldFixInheritedDataAfterItIsAdded) {
  scoped_refptr<MutableCSSComputedStyleData> grandparent_computed_style(
      new MutableCSSComputedStyleData());
  scoped_refptr<CSSComputedStyleDeclaration>
      grandparent_computed_style_declaration(
          CreateComputedStyleDeclaration(grandparent_computed_style));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  // The parent should initially have the default value of normal.
  EXPECT_EQ(FontStyleValue::GetNormal(), parent_computed_style->font_style());

  PromoteToComputedStyle(parent_computed_style,
                         grandparent_computed_style_declaration,
                         grandparent_computed_style, kNullSize, NULL);
  // The parent should still have the default value of normal.
  EXPECT_EQ(FontStyleValue::GetNormal(), parent_computed_style->font_style());

  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         grandparent_computed_style, kNullSize, NULL);

  // The child should have the default value of normal.
  EXPECT_EQ(FontStyleValue::GetNormal(), computed_style->font_style());

  grandparent_computed_style = new MutableCSSComputedStyleData();
  grandparent_computed_style->set_font_style(FontStyleValue::GetOblique());
  grandparent_computed_style_declaration->SetData(grandparent_computed_style);

  // Even though the grandparent has changed, the child should have the original
  // default value of normal until UpdateInheritedData() is called.
  EXPECT_EQ(FontStyleValue::GetNormal(), computed_style->font_style());
  parent_computed_style_declaration->UpdateInheritedData();

  // Now that UpdateInheritedData() has been called, the child should have the
  // new inherited value of oblique.
  EXPECT_EQ(FontStyleValue::GetOblique(), computed_style->font_style());
}

TEST(UpdateInheritedData,
     UpdateInheritedDataShouldFixInheritedDataAfterItIsRemoved) {
  scoped_refptr<MutableCSSComputedStyleData> grandparent_computed_style(
      new MutableCSSComputedStyleData());
  grandparent_computed_style->set_font_style(FontStyleValue::GetItalic());
  scoped_refptr<CSSComputedStyleDeclaration>
      grandparent_computed_style_declaration(
          CreateComputedStyleDeclaration(grandparent_computed_style));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  // The parent should initially have the default value of normal.
  EXPECT_EQ(FontStyleValue::GetNormal(), parent_computed_style->font_style());

  PromoteToComputedStyle(parent_computed_style,
                         grandparent_computed_style_declaration,
                         grandparent_computed_style, kNullSize, NULL);
  // The parent should now have the inherited value of italic.
  EXPECT_EQ(FontStyleValue::GetItalic(), parent_computed_style->font_style());

  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         grandparent_computed_style, kNullSize, NULL);

  // The child should have the inherited value of italic.
  EXPECT_EQ(FontStyleValue::GetItalic(), computed_style->font_style());

  grandparent_computed_style = new MutableCSSComputedStyleData();
  grandparent_computed_style_declaration->SetData(grandparent_computed_style);

  // Even though the grandparent has changed, the child should have the original
  // inherited value of italic until UpdateInheritedData() is called.
  EXPECT_EQ(FontStyleValue::GetItalic(), computed_style->font_style());
  parent_computed_style_declaration->UpdateInheritedData();

  // Now that UpdateInheritedData() has been called, the child should have the
  // default value of normal.
  EXPECT_EQ(FontStyleValue::GetNormal(), computed_style->font_style());
}

TEST(UpdateInheritedData,
     UpdateInheritedDataShouldFixInheritedDataAfterItChanges) {
  scoped_refptr<MutableCSSComputedStyleData> grandparent_computed_style(
      new MutableCSSComputedStyleData());
  grandparent_computed_style->set_font_style(FontStyleValue::GetItalic());
  scoped_refptr<CSSComputedStyleDeclaration>
      grandparent_computed_style_declaration(
          CreateComputedStyleDeclaration(grandparent_computed_style));

  scoped_refptr<MutableCSSComputedStyleData> parent_computed_style(
      new MutableCSSComputedStyleData());
  // The parent should initially have the default value of normal.
  EXPECT_EQ(FontStyleValue::GetNormal(), parent_computed_style->font_style());

  PromoteToComputedStyle(parent_computed_style,
                         grandparent_computed_style_declaration,
                         grandparent_computed_style, kNullSize, NULL);
  // The parent should now have the inherited value of italic.
  EXPECT_EQ(FontStyleValue::GetItalic(), parent_computed_style->font_style());

  scoped_refptr<CSSComputedStyleDeclaration> parent_computed_style_declaration(
      CreateComputedStyleDeclaration(parent_computed_style));
  scoped_refptr<MutableCSSComputedStyleData> computed_style(
      new MutableCSSComputedStyleData());

  PromoteToComputedStyle(computed_style, parent_computed_style_declaration,
                         grandparent_computed_style, kNullSize, NULL);

  // The child should have the inherited value of italic.
  EXPECT_EQ(FontStyleValue::GetItalic(), computed_style->font_style());

  grandparent_computed_style = new MutableCSSComputedStyleData();
  grandparent_computed_style->set_font_style(FontStyleValue::GetOblique());
  grandparent_computed_style_declaration->SetData(grandparent_computed_style);

  // Even though the grandparent has changed, the child should have the original
  // inherited value of italic until UpdateInheritedData() is called.
  EXPECT_EQ(FontStyleValue::GetItalic(), computed_style->font_style());
  parent_computed_style_declaration->UpdateInheritedData();

  // Now that UpdateInheritedData() has been called, the child should have the
  // new inherited value of oblique.
  EXPECT_EQ(FontStyleValue::GetOblique(), computed_style->font_style());
}

}  // namespace
}  // namespace cssom
}  // namespace cobalt

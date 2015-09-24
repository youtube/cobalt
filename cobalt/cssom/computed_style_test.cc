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

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/initial_style.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/property_names.h"
#include "cobalt/cssom/url_value.h"
#include "cobalt/cssom/specified_style.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(PromoteToComputedStyle, FontWeightShouldBeBoldAsSpecified) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->AssignFrom(*cssom::GetInitialStyle());
  computed_style->set_font_weight(cssom::FontWeightValue::GetBoldAka700());

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style =
      cssom::GetInitialStyle();

  PromoteToSpecifiedStyle(computed_style, parent_computed_style);
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  EXPECT_EQ(cssom::FontWeightValue::GetBoldAka700(),
            computed_style->font_weight().get());
}

TEST(PromoteToComputedStyle, FontSizeInEmShouldBeRelativeToInheritedValue) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style =
      new cssom::CSSStyleDeclarationData();
  computed_style->set_font_size(
      new cssom::LengthValue(1.5f, cssom::kFontSizesAkaEmUnit));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style =
      new cssom::CSSStyleDeclarationData();
  parent_computed_style->AssignFrom(*cssom::GetInitialStyle());
  parent_computed_style->set_font_size(
      new cssom::LengthValue(100, cssom::kPixelsUnit));

  PromoteToSpecifiedStyle(computed_style, parent_computed_style);
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  cssom::LengthValue* computed_font_size =
      base::polymorphic_downcast<cssom::LengthValue*>(
          computed_style->font_size().get());
  EXPECT_EQ(150, computed_font_size->value());
  EXPECT_EQ(cssom::kPixelsUnit, computed_font_size->unit());
}

TEST(PromoteToComputedStyle, FontSizeInPixelsShouldBeLeftAsSpecified) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style =
      new cssom::CSSStyleDeclarationData();
  computed_style->set_font_size(new cssom::LengthValue(50, cssom::kPixelsUnit));

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style =
      cssom::GetInitialStyle();

  PromoteToSpecifiedStyle(computed_style, parent_computed_style);
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  cssom::LengthValue* computed_font_size =
      base::polymorphic_downcast<cssom::LengthValue*>(
          computed_style->font_size().get());
  EXPECT_EQ(50, computed_font_size->value());
  EXPECT_EQ(cssom::kPixelsUnit, computed_font_size->unit());
}

TEST(PromoteToComputedStyle, NormalLineHeightShouldBeLeftAsSpecified) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style =
      new cssom::CSSStyleDeclarationData();
  computed_style->set_line_height(cssom::KeywordValue::GetNormal());

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style =
      cssom::GetInitialStyle();

  PromoteToSpecifiedStyle(computed_style, parent_computed_style);
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  EXPECT_EQ(cssom::KeywordValue::GetNormal(), computed_style->line_height());
}

TEST(PromoteToComputedStyle, LineHeightInEmShouldBeComputedAfterFontSize) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style =
      new cssom::CSSStyleDeclarationData();
  computed_style->set_font_size(
      new cssom::LengthValue(2, cssom::kFontSizesAkaEmUnit));
  computed_style->set_line_height(
      new cssom::LengthValue(1.5f, cssom::kFontSizesAkaEmUnit));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style =
      new cssom::CSSStyleDeclarationData();
  parent_computed_style->AssignFrom(*cssom::GetInitialStyle());
  parent_computed_style->set_font_size(
      new cssom::LengthValue(100, cssom::kPixelsUnit));

  PromoteToSpecifiedStyle(computed_style, parent_computed_style);
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  cssom::LengthValue* computed_line_height =
      base::polymorphic_downcast<cssom::LengthValue*>(
          computed_style->line_height().get());
  EXPECT_EQ(300, computed_line_height->value());
  EXPECT_EQ(cssom::kPixelsUnit, computed_line_height->unit());
}

TEST(PromoteToComputedStyle, TextIndentInEmShouldBeComputedAfterFontSize) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style =
      new cssom::CSSStyleDeclarationData();
  computed_style->set_font_size(
      new cssom::LengthValue(2, cssom::kFontSizesAkaEmUnit));
  computed_style->set_text_indent(
      new cssom::LengthValue(1.5f, cssom::kFontSizesAkaEmUnit));

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style =
      new cssom::CSSStyleDeclarationData();
  parent_computed_style->AssignFrom(*cssom::GetInitialStyle());
  parent_computed_style->set_font_size(
      new cssom::LengthValue(100, cssom::kPixelsUnit));

  PromoteToSpecifiedStyle(computed_style, parent_computed_style);
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  cssom::LengthValue* computed_text_indent =
      base::polymorphic_downcast<cssom::LengthValue*>(
          computed_style->text_indent().get());
  EXPECT_EQ(300, computed_text_indent->value());
  EXPECT_EQ(cssom::kPixelsUnit, computed_text_indent->unit());
}

TEST(PromoteToComputedStyle, BackgroundImageAbsoluteURL) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->AssignFrom(*cssom::GetInitialStyle());

  scoped_ptr<cssom::PropertyListValue::Builder> background_image_builder(
      new cssom::PropertyListValue::Builder());
  background_image_builder->push_back(
      new cssom::URLValue("file:///sample.png"));
  scoped_refptr<cssom::PropertyListValue> background_image(
      new cssom::PropertyListValue(background_image_builder.Pass()));
  computed_style->set_background_image(background_image);

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());

  GURLMap property_name_to_base_url_map;
  property_name_to_base_url_map[cssom::kBackgroundImagePropertyName] =
      GURL("file:///computed_style_test/document.html");

  PromoteToComputedStyle(computed_style, parent_computed_style,
                         &property_name_to_base_url_map);

  ASSERT_NE(cssom::KeywordValue::GetNone(), computed_style->background_image());
  scoped_refptr<cssom::PropertyListValue> background_image_list =
      dynamic_cast<cssom::PropertyListValue*>(
          computed_style->background_image().get());
  ASSERT_TRUE(background_image_list);
  ASSERT_EQ(1, background_image_list->value().size());

  GURL value = base::polymorphic_downcast<cssom::AbsoluteURLValue*>(
                   background_image_list->value()[0].get())->value();

  EXPECT_EQ("file:///sample.png", value.spec());
}

TEST(PromoteToComputedStyle, BackgroundImageRelativeURL) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->AssignFrom(*cssom::GetInitialStyle());

  scoped_ptr<cssom::PropertyListValue::Builder> background_image_builder(
      new cssom::PropertyListValue::Builder());
  background_image_builder->push_back(
      new cssom::URLValue("../test/sample.png"));
  scoped_refptr<cssom::PropertyListValue> background_image(
      new cssom::PropertyListValue(background_image_builder.Pass()));
  computed_style->set_background_image(background_image);

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());

  GURLMap property_name_to_base_url_map;
  property_name_to_base_url_map[cssom::kBackgroundImagePropertyName] =
      GURL("file:///computed_style_test/style_sheet.css");

  PromoteToComputedStyle(computed_style, parent_computed_style,
                         &property_name_to_base_url_map);

  ASSERT_NE(cssom::KeywordValue::GetNone(), computed_style->background_image());
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
  computed_style->AssignFrom(*cssom::GetInitialStyle());
  computed_style->set_background_image(cssom::KeywordValue::GetNone());

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());

  GURLMap property_name_to_base_url_map;
  property_name_to_base_url_map[cssom::kBackgroundImagePropertyName] =
      GURL("file:///computed_style_test/document.html");

  PromoteToComputedStyle(computed_style, parent_computed_style,
                         &property_name_to_base_url_map);

  EXPECT_EQ(cssom::KeywordValue::GetNone(),
            computed_style->background_image().get());
}

TEST(PromoteToComputedStyle, BackgroundSizeEmToPixel) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->AssignFrom(*cssom::GetInitialStyle());

  scoped_ptr<cssom::PropertyListValue::Builder> background_size_builder(
      new cssom::PropertyListValue::Builder());
  background_size_builder->push_back(
      new cssom::LengthValue(3, cssom::kFontSizesAkaEmUnit));
  background_size_builder->push_back(
      new cssom::LengthValue(40, cssom::kPixelsUnit));
  scoped_refptr<cssom::PropertyListValue> background_size(
      new cssom::PropertyListValue(background_size_builder.Pass()));
  computed_style->set_background_size(background_size);

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style =
      cssom::GetInitialStyle();
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
  computed_style->AssignFrom(*cssom::GetInitialStyle());
  computed_style->set_background_size(cssom::KeywordValue::GetContain());

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style =
      cssom::GetInitialStyle();
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  EXPECT_EQ(cssom::KeywordValue::GetContain(),
            computed_style->background_size());
}

TEST(PromoteToComputedStyle, HeightPercentageInUnspecifiedHeightBlockIsAuto) {
  // If the height is specified as a percentage and the height of the containing
  // block is not specified explicitly, and this element is not absolutely
  // positioned, the value computes to 'auto'.
  //   http://www.w3.org/TR/CSS2/visudet.html#the-height-property
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->AssignFrom(*cssom::GetInitialStyle());
  computed_style->set_height(new PercentageValue(0.50f));

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style =
      cssom::GetInitialStyle();
  EXPECT_EQ(cssom::KeywordValue::GetAuto(), parent_computed_style->height());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  EXPECT_EQ(cssom::KeywordValue::GetAuto(), computed_style->height());
}

TEST(PromoteToComputedStyle,
     MaxHeightPercentageInUnspecifiedHeightBlockIsNone) {
  // If the max-height is specified as a percentage and the height of the
  // containing block is not specified explicitly, and this element is not
  // absolutely positioned, the percentage value is treated as '0'.
  //   http://www.w3.org/TR/CSS2/visudet.html#propdef-max-height
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->AssignFrom(*cssom::GetInitialStyle());
  computed_style->set_max_height(new PercentageValue(0.50f));

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style =
      cssom::GetInitialStyle();
  EXPECT_EQ(cssom::KeywordValue::GetAuto(), parent_computed_style->height());
  PromoteToComputedStyle(computed_style, parent_computed_style, NULL);

  EXPECT_EQ(cssom::KeywordValue::GetNone(), computed_style->max_height());
}

TEST(PromoteToComputedStyle,
     MinHeightPercentageInUnspecifiedHeightBlockIsZero) {
  // If the min-height is specified as a percentage and the height of the
  // containing block is not specified explicitly, and this element is not
  // absolutely positioned, the percentage value is treated as 'none'.
  //   http://www.w3.org/TR/CSS2/visudet.html#propdef-min-height
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->AssignFrom(*cssom::GetInitialStyle());
  computed_style->set_min_height(new PercentageValue(0.50f));

  scoped_refptr<const cssom::CSSStyleDeclarationData> parent_computed_style =
      cssom::GetInitialStyle();
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
  //  http://www.w3.org/TR/CSS2/visudet.html#propdef-max-width
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->AssignFrom(*cssom::GetInitialStyle());
  computed_style->set_max_width(new PercentageValue(0.50f));

  scoped_refptr<const cssom::CSSStyleDeclarationData>
      grandparent_computed_style = cssom::GetInitialStyle();

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->AssignFrom(*cssom::GetInitialStyle());
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
  //  http://www.w3.org/TR/CSS2/visudet.html#propdef-min-width
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style(
      new cssom::CSSStyleDeclarationData());
  computed_style->AssignFrom(*cssom::GetInitialStyle());
  computed_style->set_min_width(new PercentageValue(0.50f));

  scoped_refptr<const cssom::CSSStyleDeclarationData>
      grandparent_computed_style = cssom::GetInitialStyle();

  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style(
      new cssom::CSSStyleDeclarationData());
  parent_computed_style->AssignFrom(*cssom::GetInitialStyle());
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

}  // namespace cssom
}  // namespace cobalt

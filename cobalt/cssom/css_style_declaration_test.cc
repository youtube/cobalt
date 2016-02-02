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

#include "cobalt/cssom/css_style_declaration.h"

#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/testing/mock_css_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

using ::testing::_;
using ::testing::Return;

class MockMutationObserver : public MutationObserver {
 public:
  MOCK_METHOD0(OnCSSMutation, void());
};

TEST(CSSStyleDeclarationTest, BackgroundSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBackgroundProperty), background, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background(background);
}

TEST(CSSStyleDeclarationTest, BackgroundColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background_color = "#fff";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kBackgroundColorProperty), background_color, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_color(background_color);
}

TEST(CSSStyleDeclarationTest, BackgroundImageSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background_image = "url('images/sample.png')";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kBackgroundImageProperty), background_image, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_image(background_image);
}

TEST(CSSStyleDeclarationTest, BackgroundPositionSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background_position = "50% 50%";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kBackgroundPositionProperty), background_position, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_position(background_position);
}

TEST(CSSStyleDeclarationTest, BackgroundRepeatSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background_repeat = "no-repeat";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kBackgroundRepeatProperty), background_repeat, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_repeat(background_repeat);
}

TEST(CSSStyleDeclarationTest, BackgroundSizeSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background_size = "cover";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBackgroundSizeProperty), background_size, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_size(background_size);
}

TEST(CSSStyleDeclarationTest, BorderSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderProperty), border, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border(border);
}

TEST(CSSStyleDeclarationTest, BorderBottomSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_bottom = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderBottomProperty), border_bottom, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_bottom(border_bottom);
}

TEST(CSSStyleDeclarationTest, BorderBottomColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_bottom_color = "#010203";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kBorderBottomColorProperty), border_bottom_color, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_bottom_color(border_bottom_color);
}

TEST(CSSStyleDeclarationTest, BorderBottomStyleSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_bottom_style = "solid";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kBorderBottomStyleProperty), border_bottom_style, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_bottom_style(border_bottom_style);
}

TEST(CSSStyleDeclarationTest, BorderBottomWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> width =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_bottom_width = "10px";
  MockMutationObserver observer;
  width->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kBorderBottomWidthProperty), border_bottom_width, _,
          const_cast<CSSStyleDeclarationData*>(width->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  width->set_border_bottom_width(border_bottom_width);
}

TEST(CSSStyleDeclarationTest, BorderColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_color = "#010203";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderColorProperty), border_color, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_color(border_color);
}

TEST(CSSStyleDeclarationTest, BorderLeftSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_left = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderLeftProperty), border_left, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_left(border_left);
}

TEST(CSSStyleDeclarationTest, BorderLeftColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_left_color = "#010203";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kBorderLeftColorProperty), border_left_color, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_left_color(border_left_color);
}

TEST(CSSStyleDeclarationTest, BorderLeftStyleSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_left_style = "solid";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kBorderLeftStyleProperty), border_left_style, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_left_style(border_left_style);
}

TEST(CSSStyleDeclarationTest, BorderLeftWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> width =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_left_width = "10px";
  MockMutationObserver observer;
  width->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kBorderLeftWidthProperty), border_left_width, _,
          const_cast<CSSStyleDeclarationData*>(width->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  width->set_border_left_width(border_left_width);
}

TEST(CSSStyleDeclarationTest, BorderRadiusSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_radius = "0.2em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderRadiusProperty), border_radius, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_radius(border_radius);
}

TEST(CSSStyleDeclarationTest, BorderRightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_right = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderRightProperty), border_right, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_right(border_right);
}

TEST(CSSStyleDeclarationTest, BorderRightColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_right_color = "#010203";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kBorderRightColorProperty), border_right_color, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_right_color(border_right_color);
}

TEST(CSSStyleDeclarationTest, BorderRightStyleSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_right_style = "solid";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kBorderRightStyleProperty), border_right_style, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_right_style(border_right_style);
}

TEST(CSSStyleDeclarationTest, BorderRightWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> width =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_right_width = "10px";
  MockMutationObserver observer;
  width->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kBorderRightWidthProperty), border_right_width, _,
          const_cast<CSSStyleDeclarationData*>(width->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  width->set_border_right_width(border_right_width);
}

TEST(CSSStyleDeclarationTest, BorderStyleSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_style = "solid";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderStyleProperty), border_style, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_style(border_style);
}

TEST(CSSStyleDeclarationTest, BorderTopSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_top = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderTopProperty), border_top, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_top(border_top);
}

TEST(CSSStyleDeclarationTest, BorderTopColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_top_color = "#010203";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderTopColorProperty), border_top_color, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_top_color(border_top_color);
}

TEST(CSSStyleDeclarationTest, BorderTopStyleSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_top_style = "solid";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderTopStyleProperty), border_top_style, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_top_style(border_top_style);
}

TEST(CSSStyleDeclarationTest, BorderTopWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> width =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_top_width = "10px";
  MockMutationObserver observer;
  width->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderTopWidthProperty), border_top_width, _,
                  const_cast<CSSStyleDeclarationData*>(width->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  width->set_border_top_width(border_top_width);
}

TEST(CSSStyleDeclarationTest, BottomSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string bottom = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBottomProperty), bottom, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_bottom(bottom);
}

TEST(CSSStyleDeclarationTest, BoxShadowSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string box_shadow = "none";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBoxShadowProperty), box_shadow, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_box_shadow(box_shadow);
}

TEST(CSSStyleDeclarationTest, ColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string color = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kColorProperty), color, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_color(color);
}

TEST(CSSStyleDeclarationTest, ContentSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string content = "url(foo.png)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kContentProperty), content, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_content(content);
}

TEST(CSSStyleDeclarationTest, DisplaySetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string display = "inline-block";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kDisplayProperty), display, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_display(display);
}

TEST(CSSStyleDeclarationTest, FontSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string font = "inherit";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kFontProperty), font, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_font(font);
}

TEST(CSSStyleDeclarationTest, FontFamilySetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string font_family = "Roboto";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kFontFamilyProperty), font_family, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_font_family(font_family);
}

TEST(CSSStyleDeclarationTest, FontSizeSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string font_size = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kFontSizeProperty), font_size, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_font_size(font_size);
}

TEST(CSSStyleDeclarationTest, FontStyleSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string font_style = "italic";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kFontStyleProperty), font_style, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_font_style(font_style);
}

TEST(CSSStyleDeclarationTest, FontWeightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string font_weight = "normal";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kFontWeightProperty), font_weight, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_font_weight(font_weight);
}

TEST(CSSStyleDeclarationTest, HeightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string height = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kHeightProperty), height, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_height(height);
}

TEST(CSSStyleDeclarationTest, LeftSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string left = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kLeftProperty), left, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_left(left);
}

TEST(CSSStyleDeclarationTest, LineHeightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string line_height = "1.5em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kLineHeightProperty), line_height, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_line_height(line_height);
}

TEST(CSSStyleDeclarationTest, MarginSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string margin = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMarginProperty), margin, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_margin(margin);
}

TEST(CSSStyleDeclarationTest, MarginBottomSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string margin_bottom = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMarginBottomProperty), margin_bottom, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_margin_bottom(margin_bottom);
}

TEST(CSSStyleDeclarationTest, MarginLeftSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string margin_left = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMarginLeftProperty), margin_left, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_margin_left(margin_left);
}

TEST(CSSStyleDeclarationTest, MarginRightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string margin_right = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMarginRightProperty), margin_right, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_margin_right(margin_right);
}

TEST(CSSStyleDeclarationTest, MarginTopSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string margin_top = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMarginTopProperty), margin_top, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_margin_top(margin_top);
}

TEST(CSSStyleDeclarationTest, MaxHeightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string max_height = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMaxHeightProperty), max_height, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_max_height(max_height);
}

TEST(CSSStyleDeclarationTest, MaxWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string max_width = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMaxWidthProperty), max_width, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_max_width(max_width);
}

TEST(CSSStyleDeclarationTest, MinHeightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string min_height = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMinHeightProperty), min_height, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_min_height(min_height);
}

TEST(CSSStyleDeclarationTest, MinWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string min_width = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMinWidthProperty), min_width, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_min_width(min_width);
}

TEST(CSSStyleDeclarationTest, OpacitySetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string opacity = "0.5";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kOpacityProperty), opacity, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_opacity(opacity);
}

TEST(CSSStyleDeclarationTest, OverflowSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string overflow = "visible";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kOverflowProperty), overflow, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_overflow(overflow);
}

TEST(CSSStyleDeclarationTest, OverflowWrapSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string overflow_wrap = "break-word";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kOverflowWrapProperty), overflow_wrap, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_overflow_wrap(overflow_wrap);
}

TEST(CSSStyleDeclarationTest, PaddingSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string padding = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kPaddingProperty), padding, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_padding(padding);
}

TEST(CSSStyleDeclarationTest, PaddingBottomSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string padding_bottom = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kPaddingBottomProperty), padding_bottom, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_padding_bottom(padding_bottom);
}

TEST(CSSStyleDeclarationTest, PaddingLeftSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string padding_left = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kPaddingLeftProperty), padding_left, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_padding_left(padding_left);
}

TEST(CSSStyleDeclarationTest, PaddingRightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string padding_right = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kPaddingRightProperty), padding_right, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_padding_right(padding_right);
}

TEST(CSSStyleDeclarationTest, PaddingTopSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string padding_top = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kPaddingTopProperty), padding_top, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_padding_top(padding_top);
}

TEST(CSSStyleDeclarationTest, PositionSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string position = "static";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kPositionProperty), position, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_position(position);
}

TEST(CSSStyleDeclarationTest, RightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string right = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kRightProperty), right, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_right(right);
}

TEST(CSSStyleDeclarationTest, TextAlignSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string text_align = "center";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTextAlignProperty), text_align, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_text_align(text_align);
}

TEST(CSSStyleDeclarationTest, TextIndentSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string text_indent = "4em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTextIndentProperty), text_indent, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_text_indent(text_indent);
}

TEST(CSSStyleDeclarationTest, TextOverflowSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string text_overflow = "ellipsis";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTextOverflowProperty), text_overflow, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_text_overflow(text_overflow);
}

TEST(CSSStyleDeclarationTest, TextShadowSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string text_shadow = "inherit";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTextShadowProperty), text_shadow, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_text_shadow(text_shadow);
}

TEST(CSSStyleDeclarationTest, TextTransformSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string text_transform = "uppercase";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTextTransformProperty), text_transform, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_text_transform(text_transform);
}

TEST(CSSStyleDeclarationTest, TopSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string top = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTopProperty), top, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_top(top);
}

TEST(CSSStyleDeclarationTest, TransformSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string transform = "scale(1.5)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTransformProperty), transform, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transform(transform);
}

TEST(CSSStyleDeclarationTest, TransformOriginSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string transform_origin = "20% 40%";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kTransformOriginProperty), transform_origin, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transform_origin(transform_origin);
}

TEST(CSSStyleDeclarationTest, TransitionSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string transition = "inherit";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTransitionProperty), transition, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transition(transition);
}

TEST(CSSStyleDeclarationTest, TransitionDelaySetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string transition_delay = "2s";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kTransitionDelayProperty), transition_delay, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transition_delay(transition_delay);
}

TEST(CSSStyleDeclarationTest, TransitionDurationSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string transition_duration = "2s";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kTransitionDurationProperty), transition_duration, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transition_duration(transition_duration);
}

TEST(CSSStyleDeclarationTest, TransitionPropertySetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string transition_property = "width";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(
      css_parser,
      ParsePropertyIntoDeclarationData(
          GetPropertyName(kTransitionPropertyProperty), transition_property, _,
          const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transition_property(transition_property);
}

TEST(CSSStyleDeclarationTest, TransitionTimingFunctionSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string transition_timing_function = "linear";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTransitionTimingFunctionProperty),
                  transition_timing_function, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transition_timing_function(transition_timing_function);
}

TEST(CSSStyleDeclarationTest, VerticalAlignSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string vertical_align = "middle";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kVerticalAlignProperty), vertical_align, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_vertical_align(vertical_align);
}

TEST(CSSStyleDeclarationTest, VisibilitySetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string visibility = "hidden";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kVisibilityProperty), visibility, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_visibility(visibility);
}

TEST(CSSStyleDeclarationTest, WhiteSpaceSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string white_space = "pre";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kWhiteSpaceProperty), white_space, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_white_space(white_space);
}

TEST(CSSStyleDeclarationTest, WidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string width = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kWidthProperty), width, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_width(width);
}

TEST(CSSStyleDeclarationTest, WordWrapSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string word_wrap = "break-word";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kWordWrapProperty), word_wrap, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_word_wrap(word_wrap);
}

TEST(CSSStyleDeclarationTest, ZIndexSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string z_index = "-1";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kZIndexProperty), z_index, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_z_index(z_index);
}

TEST(CSSStyleDeclarationTest, CSSTextSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string css_text = "font-size: 100px; color: #0047ab;";

  EXPECT_CALL(css_parser, ParseStyleDeclarationList(css_text, _))
      .WillOnce(Return(scoped_refptr<CSSStyleDeclarationData>()));
  style->set_css_text(css_text);
}

TEST(CSSStyleDeclarationTest, CSSTextSetterEmptyString) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclarationData> initial_style =
      new CSSStyleDeclarationData();
  initial_style->set_display(KeywordValue::GetInline());
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(initial_style, &css_parser);

  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  const std::string css_text = "";

  EXPECT_CALL(css_parser, ParseStyleDeclarationList(css_text, _))
      .WillOnce(Return(scoped_refptr<CSSStyleDeclarationData>()));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_css_text(css_text);
}

TEST(CSSStyleDeclarationTest, CSSTextGetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<PercentageValue> background_size = new PercentageValue(0.50f);
  scoped_refptr<LengthValue> bottom = new LengthValue(16, kPixelsUnit);

  scoped_refptr<CSSStyleDeclarationData> style_data =
      new CSSStyleDeclarationData();
  style_data->set_background_size(background_size);
  style_data->set_bottom(bottom);

  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(style_data, &css_parser);
  EXPECT_EQ(style->css_text(), "background-size: 50%; bottom: 16px;");
}

// TODO(***REMOVED***): Add GetPropertyValue tests, property getter tests and tests
// that checking if the attributes' setter and the getter are consistent when
// fully support converting PropertyValue to std::string.
TEST(CSSStyleDeclarationTest, PropertyValueSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBackgroundProperty), background, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->SetPropertyValue(GetPropertyName(kBackgroundProperty), background);
}

TEST(CSSStyleDeclarationTest, PropertyValueGetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclarationData> initial_style =
      new CSSStyleDeclarationData();
  initial_style->set_text_align(KeywordValue::GetCenter());
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(initial_style, &css_parser);

  EXPECT_EQ(style->GetPropertyValue(GetPropertyName(kTextAlignProperty)),
            "center");
}

TEST(CSSStyleDeclarationTest, LengthAttributeGetterEmpty) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  EXPECT_EQ(style->length(), 0);
}

TEST(CSSStyleDeclarationTest, LengthAttributeGetterNotEmpty) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclarationData> initial_style =
      new CSSStyleDeclarationData();
  initial_style->set_display(KeywordValue::GetInline());
  initial_style->set_text_align(KeywordValue::GetCenter());
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(initial_style, &css_parser);

  EXPECT_EQ(style->length(), 2);
}

TEST(CSSStyleDeclarationTest, ItemGetterEmpty) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  EXPECT_FALSE(style->Item(0));
}

TEST(CSSStyleDeclarationTest, ItemGetterNotEmpty) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclarationData> initial_style =
      new CSSStyleDeclarationData();
  initial_style->set_display(KeywordValue::GetInline());
  initial_style->set_text_align(KeywordValue::GetCenter());
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(initial_style, &css_parser);

  EXPECT_TRUE(style->Item(0));
  EXPECT_TRUE(style->Item(1));
  EXPECT_FALSE(style->Item(2));

  // The order is not important, as long as with properties are represented.
  if (style->Item(0).value() == GetPropertyName(kDisplayProperty)) {
    EXPECT_EQ(style->Item(1).value(), GetPropertyName(kTextAlignProperty));
  } else {
    EXPECT_EQ(style->Item(0).value(), GetPropertyName(kTextAlignProperty));
    EXPECT_EQ(style->Item(1).value(), GetPropertyName(kDisplayProperty));
  }
}

}  // namespace cssom
}  // namespace cobalt

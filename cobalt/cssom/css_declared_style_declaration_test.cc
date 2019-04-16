// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/css_declared_style_declaration.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/testing/mock_css_parser.h"
#include "cobalt/script/exception_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

using ::testing::_;
using ::testing::Return;

namespace {
class MockMutationObserver : public MutationObserver {
 public:
  MOCK_METHOD0(OnCSSMutation, void());
};
}  // namespace

TEST(CSSDeclaredStyleDeclarationTest, BackgroundSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string background = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBackgroundProperty), background, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background(background, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BackgroundColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string background_color = "#fff";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBackgroundColorProperty),
                              background_color, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_color(background_color, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BackgroundImageSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string background_image = "url('images/sample.png')";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBackgroundImageProperty),
                              background_image, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_image(background_image, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BackgroundPositionSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string background_position = "50% 50%";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBackgroundPositionProperty),
                              background_position, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_position(background_position, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BackgroundRepeatSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string background_repeat = "no-repeat";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBackgroundRepeatProperty),
                              background_repeat, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_repeat(background_repeat, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BackgroundSizeSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string background_size = "cover";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBackgroundSizeProperty),
                              background_size, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_size(background_size, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderProperty), border, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border(border, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderBottomSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_bottom = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderBottomProperty), border_bottom, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_bottom(border_bottom, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderBottomColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_bottom_color = "#010203";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderBottomColorProperty),
                              border_bottom_color, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_bottom_color(border_bottom_color, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderBottomLeftRadiusSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_bottom_left_radius = "0.2em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderBottomLeftRadiusProperty),
                              border_bottom_left_radius, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_bottom_left_radius(border_bottom_left_radius, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderBottomRightRadiusSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_bottom_right_radius = "0.2em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderBottomRightRadiusProperty),
                              border_bottom_right_radius, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_bottom_right_radius(border_bottom_right_radius, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderBottomStyleSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_bottom_style = "solid";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderBottomStyleProperty),
                              border_bottom_style, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_bottom_style(border_bottom_style, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderBottomWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> width =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_bottom_width = "10px";
  MockMutationObserver observer;
  width->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderBottomWidthProperty),
                              border_bottom_width, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  width->set_border_bottom_width(border_bottom_width, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_color = "#010203";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderColorProperty), border_color, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_color(border_color, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderLeftSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_left = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderLeftProperty), border_left, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_left(border_left, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderLeftColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_left_color = "#010203";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderLeftColorProperty),
                              border_left_color, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_left_color(border_left_color, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderLeftStyleSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_left_style = "solid";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderLeftStyleProperty),
                              border_left_style, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_left_style(border_left_style, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderLeftWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> width =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_left_width = "10px";
  MockMutationObserver observer;
  width->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderLeftWidthProperty),
                              border_left_width, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  width->set_border_left_width(border_left_width, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderRadiusSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_radius = "0.2em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderRadiusProperty), border_radius, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_radius(border_radius, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderRightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_right = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderRightProperty), border_right, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_right(border_right, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderRightColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_right_color = "#010203";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderRightColorProperty),
                              border_right_color, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_right_color(border_right_color, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderRightStyleSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_right_style = "solid";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderRightStyleProperty),
                              border_right_style, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_right_style(border_right_style, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderRightWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> width =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_right_width = "10px";
  MockMutationObserver observer;
  width->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderRightWidthProperty),
                              border_right_width, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  width->set_border_right_width(border_right_width, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderStyleSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_style = "solid";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderStyleProperty), border_style, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_style(border_style, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderTopLeftRadiusSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_top_left_radius = "0.2em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderTopLeftRadiusProperty),
                              border_top_left_radius, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_top_left_radius(border_top_left_radius, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderTopRightRadiusSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_top_right_radius = "0.2em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderTopRightRadiusProperty),
                              border_top_right_radius, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_top_right_radius(border_top_right_radius, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderTopSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_top = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderTopProperty), border_top, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_top(border_top, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderTopColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_top_color = "#010203";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderTopColorProperty),
                              border_top_color, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_top_color(border_top_color, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderTopStyleSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_top_style = "solid";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderTopStyleProperty),
                              border_top_style, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_border_top_style(border_top_style, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderTopWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> width =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_top_width = "10px";
  MockMutationObserver observer;
  width->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBorderTopWidthProperty),
                              border_top_width, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  width->set_border_top_width(border_top_width, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BorderWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> width =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string border_width = "10px";
  MockMutationObserver observer;
  width->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBorderWidthProperty), border_width, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  width->set_border_width(border_width, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BottomSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string bottom = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kBottomProperty), bottom, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_bottom(bottom, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, BoxShadowSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string box_shadow = "none";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBoxShadowProperty), box_shadow, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_box_shadow(box_shadow, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, ColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string color = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kColorProperty), color, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_color(color, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, ContentSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string content = "url(foo.png)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kContentProperty), content, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_content(content, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, DisplaySetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string display = "inline-block";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kDisplayProperty), display, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_display(display, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, FilterSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string filter =
      "-cobalt-mtm(url(p.msh), 1rad 1rad, "
      "matrix3d(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1))";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kFilterProperty), filter, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_filter(filter, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, FontSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string font = "inherit";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kFontProperty), font, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_font(font, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, FontFamilySetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string font_family = "Roboto";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kFontFamilyProperty), font_family, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_font_family(font_family, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, FontSizeSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string font_size = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kFontSizeProperty), font_size, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_font_size(font_size, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, FontStyleSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string font_style = "italic";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kFontStyleProperty), font_style, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_font_style(font_style, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, FontWeightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string font_weight = "normal";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kFontWeightProperty), font_weight, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_font_weight(font_weight, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, HeightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string height = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kHeightProperty), height, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_height(height, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, LeftSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string left = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kLeftProperty), left, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_left(left, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, LineHeightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string line_height = "1.5em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kLineHeightProperty), line_height, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_line_height(line_height, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, MarginSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string margin = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kMarginProperty), margin, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_margin(margin, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, MarginBottomSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string margin_bottom = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMarginBottomProperty), margin_bottom, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_margin_bottom(margin_bottom, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, MarginLeftSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string margin_left = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMarginLeftProperty), margin_left, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_margin_left(margin_left, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, MarginRightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string margin_right = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMarginRightProperty), margin_right, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_margin_right(margin_right, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, MarginTopSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string margin_top = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMarginTopProperty), margin_top, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_margin_top(margin_top, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, MaxHeightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string max_height = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMaxHeightProperty), max_height, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_max_height(max_height, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, MaxWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string max_width = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMaxWidthProperty), max_width, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_max_width(max_width, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, MinHeightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string min_height = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMinHeightProperty), min_height, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_min_height(min_height, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, MinWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string min_width = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kMinWidthProperty), min_width, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_min_width(min_width, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, OpacitySetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string opacity = "0.5";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kOpacityProperty), opacity, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_opacity(opacity, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, OutlineSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string outline = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kOutlineProperty), outline, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_outline(outline, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, OutlineColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string outline_color = "#010203";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kOutlineColorProperty), outline_color, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_outline_color(outline_color, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, OutlineStyleSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string outline_style = "solid";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kOutlineStyleProperty), outline_style, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_outline_style(outline_style, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, OutlineWidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> width =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string outline_width = "10px";
  MockMutationObserver observer;
  width->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kOutlineWidthProperty), outline_width, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  width->set_outline_width(outline_width, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, OverflowSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string overflow = "visible";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kOverflowProperty), overflow, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_overflow(overflow, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, OverflowWrapSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string overflow_wrap = "break-word";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kOverflowWrapProperty), overflow_wrap, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_overflow_wrap(overflow_wrap, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, PaddingSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string padding = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kPaddingProperty), padding, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_padding(padding, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, PaddingBottomSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string padding_bottom = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kPaddingBottomProperty),
                              padding_bottom, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_padding_bottom(padding_bottom, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, PaddingLeftSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string padding_left = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kPaddingLeftProperty), padding_left, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_padding_left(padding_left, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, PaddingRightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string padding_right = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kPaddingRightProperty), padding_right, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_padding_right(padding_right, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, PaddingTopSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string padding_top = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kPaddingTopProperty), padding_top, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_padding_top(padding_top, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, PositionSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string position = "static";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kPositionProperty), position, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_position(position, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, RightSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string right = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kRightProperty), right, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_right(right, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TextAlignSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string text_align = "center";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTextAlignProperty), text_align, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_text_align(text_align, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TextDecorationColorSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string text_decoration_color = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kTextDecorationColorProperty),
                              text_decoration_color, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_text_decoration_color(text_decoration_color, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TextDecorationLineSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string text_decoration_line = "line-through";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kTextDecorationLineProperty),
                              text_decoration_line, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_text_decoration_line(text_decoration_line, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TextIndentSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string text_indent = "4em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTextIndentProperty), text_indent, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_text_indent(text_indent, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TextOverflowSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string text_overflow = "ellipsis";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTextOverflowProperty), text_overflow, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_text_overflow(text_overflow, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TextShadowSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string text_shadow = "inherit";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTextShadowProperty), text_shadow, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_text_shadow(text_shadow, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TextTransformSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string text_transform = "uppercase";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kTextTransformProperty),
                              text_transform, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_text_transform(text_transform, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TopSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string top = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kTopProperty), top, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_top(top, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TransformSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string transform = "scale(1.5)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTransformProperty), transform, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transform(transform, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TransformOriginSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string transform_origin = "20% 40%";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kTransformOriginProperty),
                              transform_origin, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transform_origin(transform_origin, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TransitionSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string transition = "inherit";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTransitionProperty), transition, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transition(transition, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TransitionDelaySetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string transition_delay = "2s";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kTransitionDelayProperty),
                              transition_delay, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transition_delay(transition_delay, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TransitionDurationSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string transition_duration = "2s";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kTransitionDurationProperty),
                              transition_duration, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transition_duration(transition_duration, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TransitionPropertySetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string transition_property = "width";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kTransitionPropertyProperty),
                              transition_property, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transition_property(transition_property, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, TransitionTimingFunctionSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string transition_timing_function = "linear";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kTransitionTimingFunctionProperty),
                  transition_timing_function, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_transition_timing_function(transition_timing_function, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, VerticalAlignSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string vertical_align = "middle";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kVerticalAlignProperty),
                              vertical_align, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_vertical_align(vertical_align, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, VisibilitySetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string visibility = "hidden";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kVisibilityProperty), visibility, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_visibility(visibility, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, WhiteSpaceSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string white_space = "pre";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kWhiteSpaceProperty), white_space, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_white_space(white_space, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, WidthSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string width = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kWidthProperty), width, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_width(width, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, WordWrapSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string word_wrap = "break-word";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kWordWrapProperty), word_wrap, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_word_wrap(word_wrap, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, ZIndexSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string z_index = "-1";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser, ParsePropertyIntoDeclarationData(
                              GetPropertyName(kZIndexProperty), z_index, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_z_index(z_index, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, CSSTextSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string css_text = "font-size: 100px; color: #0047ab;";

  EXPECT_CALL(css_parser, ParseStyleDeclarationList(css_text, _))
      .WillOnce(Return(scoped_refptr<CSSDeclaredStyleData>()));
  style->set_css_text(css_text, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, CSSTextSetterEmptyString) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleData> initial_style =
      new CSSDeclaredStyleData();
  initial_style->SetPropertyValueAndImportance(
      kDisplayProperty, KeywordValue::GetInline(), false);
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(initial_style, &css_parser);

  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  const std::string css_text = "";

  EXPECT_CALL(css_parser, ParseStyleDeclarationList(css_text, _))
      .WillOnce(Return(scoped_refptr<CSSDeclaredStyleData>()));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_css_text(css_text, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, CSSTextGetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<PercentageValue> background_size = new PercentageValue(0.50f);
  scoped_refptr<LengthValue> bottom = new LengthValue(16, kPixelsUnit);

  scoped_refptr<CSSDeclaredStyleData> style_data = new CSSDeclaredStyleData();
  style_data->SetPropertyValueAndImportance(kBackgroundSizeProperty,
                                            background_size, false);
  style_data->SetPropertyValueAndImportance(kBottomProperty, bottom, true);

  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(style_data, &css_parser);
  EXPECT_EQ(style->css_text(NULL),
            "background-size: 50%; bottom: 16px !important;");
}

// TODO: Add GetPropertyValue tests, property getter tests and tests
// that checking if the attributes' setter and the getter are consistent when
// fully support converting PropertyValue to std::string.
TEST(CSSDeclaredStyleDeclarationTest, PropertyValueSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string background = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBackgroundProperty), background, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->SetPropertyValue(GetPropertyName(kBackgroundProperty), background,
                          NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, PropertyValueSetterTwice) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string background = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBackgroundProperty), background, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->SetPropertyValue(GetPropertyName(kBackgroundProperty), background,
                          NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, PropertySetterWithTwoValues) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string background = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBackgroundProperty), background, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->SetProperty(GetPropertyName(kBackgroundProperty), background, NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, PropertySetterWithThreeValues) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  const std::string background = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kBackgroundProperty), background, _, _));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->SetProperty(GetPropertyName(kBackgroundProperty), background,
                     std::string(), NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, PropertyValueGetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleData> initial_style =
      new CSSDeclaredStyleData();
  initial_style->SetPropertyValueAndImportance(
      kTextAlignProperty, KeywordValue::GetCenter(), false);
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(initial_style, &css_parser);

  EXPECT_EQ(style->GetPropertyValue(GetPropertyName(kTextAlignProperty)),
            "center");
}

TEST(CSSDeclaredStyleDeclarationTest,
     UnknownDeclaredStylePropertyValueShouldBeEmpty) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleData> initial_style =
      new CSSDeclaredStyleData();
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(initial_style, &css_parser);

  EXPECT_EQ(style->GetPropertyValue("cobalt_cobalt_cobalt"), "");
}

TEST(CSSDeclaredStyleDeclarationTest, RemoveProperty) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleData> initial_style =
      new CSSDeclaredStyleData();
  initial_style->SetPropertyValueAndImportance(
      kDisplayProperty, KeywordValue::GetInline(), false);
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(initial_style, &css_parser);

  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  GetPropertyName(kDisplayProperty), std::string(""), _, _));
  style->RemoveProperty(GetPropertyName(kDisplayProperty), NULL);
}

TEST(CSSDeclaredStyleDeclarationTest, LengthAttributeGetterEmpty) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  EXPECT_EQ(style->length(), 0);
}

TEST(CSSDeclaredStyleDeclarationTest, LengthAttributeGetterNotEmpty) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleData> initial_style =
      new CSSDeclaredStyleData();
  initial_style->SetPropertyValueAndImportance(
      kDisplayProperty, KeywordValue::GetInline(), false);
  initial_style->SetPropertyValueAndImportance(
      kTextAlignProperty, KeywordValue::GetCenter(), false);
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(initial_style, &css_parser);

  EXPECT_EQ(style->length(), 2);
}

TEST(CSSDeclaredStyleDeclarationTest, ItemGetterEmpty) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(&css_parser);

  EXPECT_FALSE(style->Item(0));
}

TEST(CSSDeclaredStyleDeclarationTest, ItemGetterNotEmpty) {
  testing::MockCSSParser css_parser;
  scoped_refptr<CSSDeclaredStyleData> initial_style =
      new CSSDeclaredStyleData();
  initial_style->SetPropertyValueAndImportance(
      kDisplayProperty, KeywordValue::GetInline(), false);
  initial_style->SetPropertyValueAndImportance(
      kTextAlignProperty, KeywordValue::GetCenter(), false);
  scoped_refptr<CSSDeclaredStyleDeclaration> style =
      new CSSDeclaredStyleDeclaration(initial_style, &css_parser);

  EXPECT_TRUE(style->Item(0));
  EXPECT_TRUE(style->Item(1));
  EXPECT_FALSE(style->Item(2));

  // The order is not important, as long as declared properties are represented.
  if (style->Item(0).value() == GetPropertyName(kDisplayProperty)) {
    EXPECT_EQ(style->Item(1).value(), GetPropertyName(kTextAlignProperty));
  } else {
    EXPECT_EQ(style->Item(0).value(), GetPropertyName(kTextAlignProperty));
    EXPECT_EQ(style->Item(1).value(), GetPropertyName(kDisplayProperty));
  }
}

}  // namespace cssom
}  // namespace cobalt

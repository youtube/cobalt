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
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

using ::testing::_;

class MockCSSParser : public CSSParser {
 public:
  MOCK_METHOD2(ParseStyleSheet,
               scoped_refptr<CSSStyleSheet>(const std::string&,
                                            const base::SourceLocation&));
  MOCK_METHOD2(ParseStyleRule,
               scoped_refptr<CSSStyleRule>(const std::string&,
                                           const base::SourceLocation&));
  MOCK_METHOD2(ParseDeclarationList,
               scoped_refptr<CSSStyleDeclarationData>(
                   const std::string&, const base::SourceLocation&));
  MOCK_METHOD3(ParsePropertyValue,
               scoped_refptr<PropertyValue>(const std::string&,
                                            const std::string&,
                                            const base::SourceLocation&));
  MOCK_METHOD4(ParsePropertyIntoStyle,
               void(const std::string&, const std::string&,
                    const base::SourceLocation&, CSSStyleDeclarationData*));
};

class MockMutationObserver : public MutationObserver {
 public:
  MOCK_METHOD0(OnLoad, void());
  MOCK_METHOD0(OnMutation, void());
};

// TODO(***REMOVED***): Add GetPropertyValue tests, property getter tests and tests
// that checking if the attributes' setter and the getter are consistent when
// fully support converting PropertyValue to std::string.
TEST(CSSStyleDeclarationTest, PropertyValueSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kBackgroundPropertyName, background, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->SetPropertyValue(kBackgroundPropertyName, background);
}

TEST(CSSStyleDeclarationTest, BackgroundSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kBackgroundPropertyName, background, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_background(background);
}

TEST(CSSStyleDeclarationTest, BackgroundColorSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background_color = "#fff";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kBackgroundColorPropertyName, background_color, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_background_color(background_color);
}

TEST(CSSStyleDeclarationTest, BackgroundImageSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background_image = "url('images/sample.png')";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kBackgroundImagePropertyName, background_image, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_background_image(background_image);
}

TEST(CSSStyleDeclarationTest, BorderRadiusdSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_radius = "0.2em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kBorderRadiusPropertyName, border_radius, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_border_radius(border_radius);
}

TEST(CSSStyleDeclarationTest, ColorSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string color = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kColorPropertyName, color, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_color(color);
}

TEST(CSSStyleDeclarationTest, DisplaySetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string display = "inline-block";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kDisplayPropertyName, display, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_display(display);
}

TEST(CSSStyleDeclarationTest, FontFamilySetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string font_family = "Droid Sans";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kFontFamilyPropertyName, font_family, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_font_family(font_family);
}

TEST(CSSStyleDeclarationTest, FontSizeSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string font_size = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kFontSizePropertyName, font_size, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_font_size(font_size);
}

TEST(CSSStyleDeclarationTest, FontWeightSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string font_weight = "normal";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kFontWeightPropertyName, font_weight, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_font_weight(font_weight);
}

TEST(CSSStyleDeclarationTest, HeightSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string height = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kHeightPropertyName, height, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_height(height);
}

TEST(CSSStyleDeclarationTest, LineHeightSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string line_height = "1.5em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kLineHeightPropertyName, line_height, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_line_height(line_height);
}

TEST(CSSStyleDeclarationTest, OpacitySetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string opacity = "0.5";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kOpacityPropertyName, opacity, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_opacity(opacity);
}

TEST(CSSStyleDeclarationTest, OverflowSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string overflow = "visible";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kOverflowPropertyName, overflow, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_overflow(overflow);
}

TEST(CSSStyleDeclarationTest, PositionSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string position = "static";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kPositionPropertyName, position, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_position(position);
}

TEST(CSSStyleDeclarationTest, TransformSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string transform = "scale(1.5)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kTransformPropertyName, transform, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_transform(transform);
}

TEST(CSSStyleDeclarationTest, VerticalAlignSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string vertical_align = "middle";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kVerticalAlignPropertyName, vertical_align, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_vertical_align(vertical_align);
}

TEST(CSSStyleDeclarationTest, WidthSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string width = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoStyle(
                  kWidthPropertyName, width, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnMutation()).Times(1);

  style->set_width(width);
}

TEST(CSSStyleDeclarationTest, CSSTextSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string css_text = "font-size: 100px; color: #0047ab;";
  EXPECT_CALL(css_parser, ParseDeclarationList(css_text, _))
      .WillOnce(testing::Return(scoped_refptr<CSSStyleDeclarationData>()));
  style->set_css_text(css_text);
}

}  // namespace cssom
}  // namespace cobalt

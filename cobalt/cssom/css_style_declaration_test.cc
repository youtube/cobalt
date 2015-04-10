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
  MOCK_METHOD2(ParseDeclarationList,
               scoped_refptr<CSSStyleDeclarationData>(
                   const std::string&, const base::SourceLocation&));
  MOCK_METHOD3(ParsePropertyValue,
               scoped_refptr<PropertyValue>(const std::string&,
                                            const std::string&,
                                            const base::SourceLocation&));
};

// TODO(***REMOVED***): Add PropertyValue getter tests and property setter tests when
// fully support converting PropertyValue to std::string.
TEST(CSSStyleDeclarationTest, PropertyValueSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background = "rgba(0, 0, 0, .8)";
  EXPECT_CALL(css_parser,
              ParsePropertyValue(kBackgroundPropertyName, background, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->SetPropertyValue(kBackgroundPropertyName, background);
}

TEST(CSSStyleDeclarationTest, BackgroundSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background = "rgba(0, 0, 0, .8)";
  EXPECT_CALL(css_parser,
              ParsePropertyValue(kBackgroundPropertyName, background, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->set_background(background);
}

TEST(CSSStyleDeclarationTest, BackgroundColorSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background_color = "#fff";
  EXPECT_CALL(css_parser, ParsePropertyValue(kBackgroundColorPropertyName,
                                             background_color, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->set_background_color(background_color);
}

TEST(CSSStyleDeclarationTest, BackgroundImageSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background_image = "url('images/sample.png')";
  EXPECT_CALL(css_parser, ParsePropertyValue(kBackgroundImagePropertyName,
                                             background_image, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->set_background_image(background_image);
}

TEST(CSSStyleDeclarationTest, BorderRadiusdSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_radius = "0.2em";
  EXPECT_CALL(css_parser,
              ParsePropertyValue(kBorderRadiusPropertyName, border_radius, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->set_border_radius(border_radius);
}

TEST(CSSStyleDeclarationTest, ColorSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string color = "rgba(0, 0, 0, .8)";
  EXPECT_CALL(css_parser, ParsePropertyValue(kColorPropertyName, color, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->set_color(color);
}

TEST(CSSStyleDeclarationTest, DisplaySetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string display = "inline-block";
  EXPECT_CALL(css_parser, ParsePropertyValue(kDisplayPropertyName, display, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->set_display(display);
}

TEST(CSSStyleDeclarationTest, FontFamilySetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string font_family = "Droid Sans";
  EXPECT_CALL(css_parser,
              ParsePropertyValue(kFontFamilyPropertyName, font_family, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->set_font_family(font_family);
}

TEST(CSSStyleDeclarationTest, FontSizeSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string font_size = "100px";
  EXPECT_CALL(css_parser,
              ParsePropertyValue(kFontSizePropertyName, font_size, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->set_font_size(font_size);
}

TEST(CSSStyleDeclarationTest, FontWeightSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string font_weight = "normal";
  EXPECT_CALL(css_parser,
              ParsePropertyValue(kFontWeightPropertyName, font_weight, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->set_font_weight(font_weight);
}

TEST(CSSStyleDeclarationTest, HeightSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string height = "100px";
  EXPECT_CALL(css_parser, ParsePropertyValue(kHeightPropertyName, height, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->set_height(height);
}

TEST(CSSStyleDeclarationTest, OpacitySetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string opacity = "0.5";
  EXPECT_CALL(css_parser, ParsePropertyValue(kOpacityPropertyName, opacity, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->set_opacity(opacity);
}

TEST(CSSStyleDeclarationTest, OverflowSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string overflow = "visible";
  EXPECT_CALL(css_parser,
              ParsePropertyValue(kOverflowPropertyName, overflow, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->set_overflow(overflow);
}

TEST(CSSStyleDeclarationTest, TransformSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string transform = "scale(1.5)";
  EXPECT_CALL(css_parser,
              ParsePropertyValue(kTransformPropertyName, transform, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
  style->set_transform(transform);
}

TEST(CSSStyleDeclarationTest, WidthSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string width = "100px";
  EXPECT_CALL(css_parser, ParsePropertyValue(kWidthPropertyName, width, _))
      .WillOnce(testing::Return(scoped_refptr<PropertyValue>()));
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

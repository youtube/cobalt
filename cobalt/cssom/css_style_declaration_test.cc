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

#include "cobalt/cssom/css_font_face_declaration_data.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/media_query.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_names.h"
#include "cobalt/cssom/property_value.h"
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
  MOCK_METHOD2(ParseStyleDeclarationList,
               scoped_refptr<CSSStyleDeclarationData>(
                   const std::string&, const base::SourceLocation&));
  MOCK_METHOD2(ParseFontFaceDeclarationList,
               scoped_refptr<CSSFontFaceDeclarationData>(
                   const std::string&, const base::SourceLocation&));
  MOCK_METHOD3(ParsePropertyValue,
               scoped_refptr<PropertyValue>(const std::string&,
                                            const std::string&,
                                            const base::SourceLocation&));
  MOCK_METHOD4(ParsePropertyIntoDeclarationData,
               void(const std::string&, const std::string&,
                    const base::SourceLocation&, CSSDeclarationData*));
  MOCK_METHOD2(ParseMediaQuery,
               scoped_refptr<MediaQuery>(const std::string&,
                                         const base::SourceLocation&));
};

class MockMutationObserver : public MutationObserver {
 public:
  MOCK_METHOD0(OnCSSMutation, void());
};

TEST(CSSStyleDeclarationTest, BackgroundSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kBackgroundPropertyName, background, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
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
              ParsePropertyIntoDeclarationData(
                  kBackgroundColorPropertyName, background_color, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
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
              ParsePropertyIntoDeclarationData(
                  kBackgroundImagePropertyName, background_image, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_image(background_image);
}

TEST(CSSStyleDeclarationTest, BackgroundPositionSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background_position = "50% 50%";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kBackgroundPositionPropertyName, background_position, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_position(background_position);
}

TEST(CSSStyleDeclarationTest, BackgroundRepeatSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background_repeat = "no-repeat";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kBackgroundRepeatPropertyName, background_repeat, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_repeat(background_repeat);
}

TEST(CSSStyleDeclarationTest, BackgroundSizeSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background_size = "cover";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kBackgroundSizePropertyName, background_size, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_background_size(background_size);
}

TEST(CSSStyleDeclarationTest, BorderRadiusSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string border_radius = "0.2em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kBorderRadiusPropertyName, border_radius, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
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
              ParsePropertyIntoDeclarationData(
                  kColorPropertyName, color, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_color(color);
}

TEST(CSSStyleDeclarationTest, ContentSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string content = "url(foo.png)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kContentPropertyName, content, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_content(content);
}

TEST(CSSStyleDeclarationTest, DisplaySetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string display = "inline-block";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kDisplayPropertyName, display, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
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
              ParsePropertyIntoDeclarationData(
                  kFontFamilyPropertyName, font_family, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
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
              ParsePropertyIntoDeclarationData(
                  kFontSizePropertyName, font_size, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
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
              ParsePropertyIntoDeclarationData(
                  kFontWeightPropertyName, font_weight, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
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
              ParsePropertyIntoDeclarationData(
                  kHeightPropertyName, height, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
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
              ParsePropertyIntoDeclarationData(
                  kLineHeightPropertyName, line_height, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
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
              ParsePropertyIntoDeclarationData(
                  kOpacityPropertyName, opacity, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
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
              ParsePropertyIntoDeclarationData(
                  kOverflowPropertyName, overflow, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_overflow(overflow);
}

TEST(CSSStyleDeclarationTest, OverflowWrapSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string overflow_wrap = "break-word";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kOverflowWrapPropertyName, overflow_wrap, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_overflow_wrap(overflow_wrap);
}

TEST(CSSStyleDeclarationTest, PositionSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string position = "static";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kPositionPropertyName, position, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_position(position);
}

TEST(CSSStyleDeclarationTest, TabSizeSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string tab_size = "4";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kTabSizePropertyName, tab_size, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_tab_size(tab_size);
}

TEST(CSSStyleDeclarationTest, TextAlignSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string text_align = "center";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kTextAlignPropertyName, text_align, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_text_align(text_align);
}

TEST(CSSStyleDeclarationTest, TextIndentSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string text_indent = "4em";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kTextIndentPropertyName, text_indent, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_text_indent(text_indent);
}

TEST(CSSStyleDeclarationTest, TextOverflowSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string text_overflow = "ellipsis";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kTextOverflowPropertyName, text_overflow, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_text_overflow(text_overflow);
}

TEST(CSSStyleDeclarationTest, TextTransformSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string text_transform = "uppercase";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kTextTransformPropertyName, text_transform, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_text_transform(text_transform);
}

TEST(CSSStyleDeclarationTest, TransformSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string transform = "scale(1.5)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kTransformPropertyName, transform, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
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
              ParsePropertyIntoDeclarationData(
                  kVerticalAlignPropertyName, vertical_align, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_vertical_align(vertical_align);
}

TEST(CSSStyleDeclarationTest, VisibilitySetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string visibility = "hidden";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kVisibilityPropertyName, visibility, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_visibility(visibility);
}

TEST(CSSStyleDeclarationTest, WhiteSpaceSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string white_space = "pre";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kWhiteSpacePropertyName, white_space, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_white_space(white_space);
}

TEST(CSSStyleDeclarationTest, WidthSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string width = "100px";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kWidthPropertyName, width, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_width(width);
}

TEST(CSSStyleDeclarationTest, WordWrapSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string word_wrap = "break-word";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kWordWrapPropertyName, word_wrap, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);

  style->set_word_wrap(word_wrap);
}

TEST(CSSStyleDeclarationTest, CSSTextSetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string css_text = "font-size: 100px; color: #0047ab;";

  EXPECT_CALL(css_parser, ParseStyleDeclarationList(css_text, _))
      .WillOnce(testing::Return(scoped_refptr<CSSStyleDeclarationData>()));
  style->set_css_text(css_text);
}

TEST(CSSStyleDeclarationTest, CSSTextSetterEmptyString) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclarationData> initial_style =
      new CSSStyleDeclarationData();
  initial_style->set_display(KeywordValue::GetInline());
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(initial_style, &css_parser);

  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  const std::string css_text = "";

  EXPECT_CALL(css_parser, ParseStyleDeclarationList(css_text, _))
      .WillOnce(testing::Return(scoped_refptr<CSSStyleDeclarationData>()));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->set_css_text(css_text);
}

TEST(CSSStyleDeclarationTest, CSSTextGetter) {
  MockCSSParser css_parser;
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
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  const std::string background = "rgba(0, 0, 0, .8)";
  MockMutationObserver observer;
  style->set_mutation_observer(&observer);

  EXPECT_CALL(css_parser,
              ParsePropertyIntoDeclarationData(
                  kBackgroundPropertyName, background, _,
                  const_cast<CSSStyleDeclarationData*>(style->data().get())));
  EXPECT_CALL(observer, OnCSSMutation()).Times(1);
  style->SetPropertyValue(kBackgroundPropertyName, background);
}

TEST(CSSStyleDeclarationTest, PropertyValueGetter) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclarationData> initial_style =
      new CSSStyleDeclarationData();
  initial_style->set_text_align(KeywordValue::GetCenter());
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(initial_style, &css_parser);

  EXPECT_EQ(style->GetPropertyValue(kTextAlignPropertyName), "center");
}

TEST(CSSStyleDeclarationTest, LengthAttributeGetterEmpty) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  EXPECT_EQ(style->length(), 0);
}

TEST(CSSStyleDeclarationTest, LengthAttributeGetterNotEmpty) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclarationData> initial_style =
      new CSSStyleDeclarationData();
  initial_style->set_display(KeywordValue::GetInline());
  initial_style->set_text_align(KeywordValue::GetCenter());
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(initial_style, &css_parser);

  EXPECT_EQ(style->length(), 2);
}

TEST(CSSStyleDeclarationTest, ItemGetterEmpty) {
  MockCSSParser css_parser;
  scoped_refptr<CSSStyleDeclaration> style =
      new CSSStyleDeclaration(&css_parser);

  EXPECT_FALSE(style->Item(0));
}

TEST(CSSStyleDeclarationTest, ItemGetterNotEmpty) {
  MockCSSParser css_parser;
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
  if (style->Item(0).value() == kDisplayPropertyName) {
    EXPECT_EQ(style->Item(1).value(), kTextAlignPropertyName);
  } else {
    EXPECT_EQ(style->Item(0).value(), kTextAlignPropertyName);
    EXPECT_EQ(style->Item(1).value(), kDisplayPropertyName);
  }
}

}  // namespace cssom
}  // namespace cobalt

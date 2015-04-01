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

#include "cobalt/css_parser/parser.h"

#include "base/bind.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/scale_function.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/transform_list_value.h"
#include "cobalt/cssom/translate_function.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace css_parser {

using ::testing::_;

class MockParserObserver {
 public:
  MOCK_METHOD1(OnWarning, void(const std::string& message));
  MOCK_METHOD1(OnError, void(const std::string& message));
};

class ParserTest : public ::testing::Test {
 public:
  ParserTest();
  ~ParserTest() OVERRIDE {}

 protected:
  ::testing::StrictMock<MockParserObserver> parser_observer_;
  Parser parser_;
};

ParserTest::ParserTest()
    : parser_(base::Bind(&MockParserObserver::OnWarning,
                         base::Unretained(&parser_observer_)),
              base::Bind(&MockParserObserver::OnError,
                         base::Unretained(&parser_observer_))) {}

namespace {

scoped_refptr<cssom::CSSStyleDeclaration> GetStyleOfOnlyRuleInStyleSheet(
    const scoped_refptr<cssom::CSSStyleSheet>& style_sheet) {
  CHECK_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  CHECK_EQ(1u, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule = css_rules->Item(0);
  CHECK_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  CHECK_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  return style;
}

}  // namespace

// TODO(***REMOVED***): Test every reduction that has semantic action.

TEST_F(ParserTest, ParsesEmptyInput) {
  scoped_refptr<cssom::CSSStyleSheet> style_sheet = parser_.ParseStyleSheet(
      "", base::SourceLocation("[object ParserTest]", 1, 1));
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(0, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, HandlesUnrecoverableSyntaxError) {
  EXPECT_CALL(
      parser_observer_,
      OnError("[object ParserTest]:1:1: error: unrecoverable syntax error"));

  scoped_refptr<cssom::CSSStyleSheet> style_sheet = parser_.ParseStyleSheet(
      "@casino-royale", base::SourceLocation("[object ParserTest]", 1, 1));
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(0, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, ComputesErrorLocationOnFirstLine) {
  EXPECT_CALL(
      parser_observer_,
      OnError("[object ParserTest]:10:18: error: unrecoverable syntax error"));

  scoped_refptr<cssom::CSSStyleSheet> style_sheet = parser_.ParseStyleSheet(
      "cucumber", base::SourceLocation("[object ParserTest]", 10, 10));
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(0, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, ComputesErrorLocationOnSecondLine) {
  EXPECT_CALL(
      parser_observer_,
      OnError("[object ParserTest]:11:9: error: unrecoverable syntax error"));

  scoped_refptr<cssom::CSSStyleSheet> style_sheet = parser_.ParseStyleSheet(
      "\n"
      "cucumber",
      base::SourceLocation("[object ParserTest]", 10, 10));
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(0, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, IgnoresSgmlCommentDelimiters) {
  scoped_refptr<cssom::CSSStyleSheet> style_sheet = parser_.ParseStyleSheet(
      "<!-- body {} -->", base::SourceLocation("[object ParserTest]", 1, 1));
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(1, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, RecoversFromInvalidAtToken) {
  EXPECT_CALL(parser_observer_,
              OnWarning("[object ParserTest]:1:9: warning: invalid rule"));

  scoped_refptr<cssom::CSSStyleSheet> style_sheet = parser_.ParseStyleSheet(
      "body {} @cobalt-magic; div {}",
      base::SourceLocation("[object ParserTest]", 1, 1));
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(2, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, RecoversFromInvalidRuleWhichEndsWithSemicolon) {
  EXPECT_CALL(parser_observer_,
              OnWarning("[object ParserTest]:1:9: warning: invalid rule"));

  scoped_refptr<cssom::CSSStyleSheet> style_sheet = parser_.ParseStyleSheet(
      "body {} @charset 'utf-8'; div {}",
      base::SourceLocation("[object ParserTest]", 1, 1));
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(2, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, RecoversFromInvalidRuleWhichEndsWithBlock) {
  EXPECT_CALL(parser_observer_,
              OnWarning("[object ParserTest]:1:9: warning: invalid rule"));

  scoped_refptr<cssom::CSSStyleSheet> style_sheet = parser_.ParseStyleSheet(
      "body {} !important {} div {}",
      base::SourceLocation("[object ParserTest]", 1, 1));
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(2, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, SilentlyIgnoresNonWebKitProperties) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".double-size {\n"
          "  -moz-transform: scale(2);\n"
          "  -ms-transform: scale(2);\n"
          "  -o-transform: scale(2);\n"
          "  transform: scale(2);\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_NE(scoped_refptr<cssom::PropertyValue>(), style->transform());
}

TEST_F(ParserTest, WarnsAboutInvalidStandardAndWebKitProperties) {
  EXPECT_CALL(parser_observer_, OnWarning(
                                    "[object ParserTest]:2:3: warning: "
                                    "unsupported property -webkit-pony"));
  EXPECT_CALL(
      parser_observer_,
      OnWarning("[object ParserTest]:3:3: warning: unsupported property pony"));

  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".friendship {\n"
          "  -webkit-pony: rainbowdash;\n"
          "  pony: rainbowdash;\n"
          "  color: #9edbf9;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_NE(scoped_refptr<cssom::PropertyValue>(), style->color());
}

TEST_F(ParserTest, WarnsAboutInvalidPropertyValues) {
  EXPECT_CALL(
      parser_observer_,
      OnWarning("[object ParserTest]:2:21: warning: unsupported value"));

  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".champs-elysees {\n"
          "  background-color: morning haze over Paris;\n"
          "  color: #fff;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_EQ(scoped_refptr<cssom::PropertyValue>(), style->background_color());
  EXPECT_NE(scoped_refptr<cssom::PropertyValue>(), style->color());
}

TEST_F(ParserTest, RecoversFromInvalidPropertyDeclaration) {
  EXPECT_CALL(
      parser_observer_,
      OnWarning("[object ParserTest]:2:3: warning: invalid declaration"));

  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".friday-night-submit {\n"
          "  1px;\n"
          "  color: #fff;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_NE(scoped_refptr<cssom::PropertyValue>(), style->color());
}

// TODO(***REMOVED***): Test selectors.

TEST_F(ParserTest, ParsesInherit) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".like-father-like-son {\n"
          "  background-color: inherit;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_EQ(cssom::KeywordValue::GetInherit(), style->background_color());
}

TEST_F(ParserTest, ParsesInitial) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".reset {\n"
          "  background-color: initial;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_EQ(cssom::KeywordValue::GetInitial(), style->background_color());
}

TEST_F(ParserTest, ParsesBackground) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".playlist {\n"
          "  background: rgba(0, 0, 0, .8);\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::RGBAColorValue> background =
      dynamic_cast<cssom::RGBAColorValue*>(style->background().get());
  ASSERT_NE(scoped_refptr<cssom::RGBAColorValue>(), background);
  EXPECT_EQ(0x000000cc, background->value());
}

TEST_F(ParserTest, ParsesBackgroundColor) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".clean {\n"
          "  background-color: #fff;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::RGBAColorValue> background_color =
      dynamic_cast<cssom::RGBAColorValue*>(style->background_color().get());
  ASSERT_NE(scoped_refptr<cssom::RGBAColorValue>(), background_color);
  EXPECT_EQ(0xffffffff, background_color->value());
}

TEST_F(ParserTest, ParsesBorderRadius) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".rounded {\n"
          "  border-radius: 0.2em;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::LengthValue> border_radius =
      dynamic_cast<cssom::LengthValue*>(style->border_radius().get());
  ASSERT_NE(scoped_refptr<cssom::LengthValue>(), border_radius);
  EXPECT_FLOAT_EQ(0.2f, border_radius->value());
  EXPECT_EQ(cssom::kFontSizesAkaEmUnit, border_radius->unit());
}

TEST_F(ParserTest, Parses3DigitColor) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".dark {\n"
          "  color: #123;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::RGBAColorValue> color =
      dynamic_cast<cssom::RGBAColorValue*>(style->color().get());
  ASSERT_NE(scoped_refptr<cssom::RGBAColorValue>(), color);
  EXPECT_EQ(0x112233ff, color->value());
}

TEST_F(ParserTest, Parses6DigitColor) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".logo {\n"
          "  color: #0047ab;  /* Cobalt blue */\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::RGBAColorValue> color =
      dynamic_cast<cssom::RGBAColorValue*>(style->color().get());
  ASSERT_NE(scoped_refptr<cssom::RGBAColorValue>(), color);
  EXPECT_EQ(0x0047abff, color->value());
}

TEST_F(ParserTest, ParsesRGBColorWithOutOfRangeIntegers) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".sparta {\n"
          "  color: rgb(300, 0, -300);\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::RGBAColorValue> color =
      dynamic_cast<cssom::RGBAColorValue*>(style->color().get());
  ASSERT_NE(scoped_refptr<cssom::RGBAColorValue>(), color);
  EXPECT_EQ(0xff0000ff, color->value());
}

TEST_F(ParserTest, ParsesRGBAColorWithIntegers) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".dimmed {\n"
          "  color: rgba(255, 128, 1, 0.5);\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::RGBAColorValue> color =
      dynamic_cast<cssom::RGBAColorValue*>(style->color().get());
  ASSERT_NE(scoped_refptr<cssom::RGBAColorValue>(), color);
  EXPECT_EQ(0xff80017f, color->value());
}

TEST_F(ParserTest, ParsesBlockDisplay) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          "div {\n"
          "  display: block;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_EQ(cssom::KeywordValue::GetBlock(), style->display());
}

TEST_F(ParserTest, ParsesInlineDisplay) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          "span {\n"
          "  display: inline;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_EQ(cssom::KeywordValue::GetInline(), style->display());
}

TEST_F(ParserTest, ParsesInlineBlockDisplay) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          "marquee {\n"
          "  display: inline-block;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_EQ(cssom::KeywordValue::GetInlineBlock(), style->display());
}

TEST_F(ParserTest, ParsesNoneDisplay) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          "head {\n"
          "  display: none;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_EQ(cssom::KeywordValue::GetNone(), style->display());
}

TEST_F(ParserTest, ParsesFontFamily) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".nexus {\n"
          "  font-family: \"Droid Sans\";\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::StringValue> font_family =
      dynamic_cast<cssom::StringValue*>(style->font_family().get());
  ASSERT_NE(scoped_refptr<cssom::StringValue>(), font_family);
  EXPECT_EQ("Droid Sans", font_family->value());
}

// TODO(***REMOVED***): Test other length units, including negative ones.

TEST_F(ParserTest, ParsesFontSize) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".large {\n"
          "  font-size: 100px;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::LengthValue> font_size =
      dynamic_cast<cssom::LengthValue*>(style->font_size().get());
  ASSERT_NE(scoped_refptr<cssom::LengthValue>(), font_size);
  EXPECT_FLOAT_EQ(100, font_size->value());
  EXPECT_EQ(cssom::kPixelsUnit, font_size->unit());
}

TEST_F(ParserTest, ParsesNormalFontWeight) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".normal {\n"
          "  font-weight: normal;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_EQ(cssom::FontWeightValue::GetNormalAka400(), style->font_weight());
}

TEST_F(ParserTest, ParsesBoldFontWeight) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".bold {\n"
          "  font-weight: bold;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_EQ(cssom::FontWeightValue::GetBoldAka700(), style->font_weight());
}

TEST_F(ParserTest, ParsesHeight) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".fixed-height {\n"
          "  height: 100px;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::LengthValue> height =
      dynamic_cast<cssom::LengthValue*>(style->height().get());
  ASSERT_NE(scoped_refptr<cssom::LengthValue>(), height);
  EXPECT_FLOAT_EQ(100, height->value());
  EXPECT_EQ(cssom::kPixelsUnit, height->unit());
}

TEST_F(ParserTest, ParsesOpacity) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".translucent {\n"
          "  opacity: 0.5;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::NumberValue> translucent =
      dynamic_cast<cssom::NumberValue*>(style->opacity().get());
  ASSERT_NE(scoped_refptr<cssom::NumberValue>(), translucent);
  EXPECT_FLOAT_EQ(0.5f, translucent->value());
}

TEST_F(ParserTest, ClampsOpacityToZero) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".transparent {\n"
          "  opacity: -3.14;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::NumberValue> transparent =
      dynamic_cast<cssom::NumberValue*>(style->opacity().get());
  ASSERT_NE(scoped_refptr<cssom::NumberValue>(), transparent);
  EXPECT_FLOAT_EQ(0, transparent->value());
}

TEST_F(ParserTest, ClampsOpacityToOne) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".opaque {\n"
          "  opacity: 2.72;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::NumberValue> opaque =
      dynamic_cast<cssom::NumberValue*>(style->opacity().get());
  ASSERT_NE(scoped_refptr<cssom::NumberValue>(), opaque);
  EXPECT_FLOAT_EQ(1, opaque->value());
}

TEST_F(ParserTest, ParsesHiddenOverflow) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".hidden {\n"
          "  overflow: hidden;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_EQ(cssom::KeywordValue::GetHidden(), style->overflow());
}

TEST_F(ParserTest, ParsesVisibleOverflow) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".visible {\n"
          "  overflow: visible;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_EQ(cssom::KeywordValue::GetVisible(), style->overflow());
}

TEST_F(ParserTest, ParsesNoneTransform) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".normal-state {\n"
          "  transform: none;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_EQ(cssom::KeywordValue::GetNone(), style->transform());
}

TEST_F(ParserTest, ParsesIsotropicScaleTransform) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".isotropic-scale {\n"
          "  transform: scale(1.5);\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::TransformListValue> transform_list =
      dynamic_cast<cssom::TransformListValue*>(style->transform().get());
  ASSERT_NE(scoped_refptr<cssom::TransformListValue>(), transform_list);
  ASSERT_EQ(1, transform_list->value().size());

  const cssom::ScaleFunction* scale_function =
      dynamic_cast<const cssom::ScaleFunction*>(transform_list->value()[0]);
  ASSERT_NE(static_cast<cssom::ScaleFunction*>(NULL), scale_function);
  EXPECT_FLOAT_EQ(1.5, scale_function->x_factor());
  EXPECT_FLOAT_EQ(1.5, scale_function->y_factor());
}

// TODO(***REMOVED***): Test integers, including negative ones.
// TODO(***REMOVED***): Test reals, including negative ones.
// TODO(***REMOVED***): Test non-zero lengths without units.

TEST_F(ParserTest, ParsesAnisotropicScaleTransform) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".anisotropic-scale {\n"
          "  transform: scale(16, 9);\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::TransformListValue> transform_list =
      dynamic_cast<cssom::TransformListValue*>(style->transform().get());
  ASSERT_NE(scoped_refptr<cssom::TransformListValue>(), transform_list);
  ASSERT_EQ(1, transform_list->value().size());

  const cssom::ScaleFunction* scale_function =
      dynamic_cast<const cssom::ScaleFunction*>(transform_list->value()[0]);
  ASSERT_NE(static_cast<cssom::ScaleFunction*>(NULL), scale_function);
  EXPECT_FLOAT_EQ(16, scale_function->x_factor());
  EXPECT_FLOAT_EQ(9, scale_function->y_factor());
}

TEST_F(ParserTest, ParsesTranslateXTransform) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".translate-x {\n"
          "  transform: translateX(0);\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::TransformListValue> transform_list =
      dynamic_cast<cssom::TransformListValue*>(style->transform().get());
  ASSERT_NE(scoped_refptr<cssom::TransformListValue>(), transform_list);
  ASSERT_EQ(1, transform_list->value().size());

  const cssom::TranslateXFunction* translate_x_function =
      dynamic_cast<const cssom::TranslateXFunction*>(
          transform_list->value()[0]);
  ASSERT_NE(static_cast<cssom::TranslateXFunction*>(NULL),
            translate_x_function);

  scoped_refptr<cssom::LengthValue> offset = translate_x_function->offset();
  EXPECT_FLOAT_EQ(0, offset->value());
  EXPECT_EQ(cssom::kPixelsUnit, offset->unit());
}

TEST_F(ParserTest, ParsesTranslateYTransform) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".translate-y {\n"
          "  transform: translateY(30em);\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::TransformListValue> transform_list =
      dynamic_cast<cssom::TransformListValue*>(style->transform().get());
  ASSERT_NE(scoped_refptr<cssom::TransformListValue>(), transform_list);
  ASSERT_EQ(1, transform_list->value().size());

  const cssom::TranslateYFunction* translate_y_function =
      dynamic_cast<const cssom::TranslateYFunction*>(
          transform_list->value()[0]);
  ASSERT_NE(static_cast<cssom::TranslateYFunction*>(NULL),
            translate_y_function);

  scoped_refptr<cssom::LengthValue> offset = translate_y_function->offset();
  EXPECT_FLOAT_EQ(30, offset->value());
  EXPECT_EQ(cssom::kFontSizesAkaEmUnit, offset->unit());
}

TEST_F(ParserTest, ParsesTranslateZTransform) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".translate-z {\n"
          "  transform: translateZ(-22px);\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::TransformListValue> transform_list =
      dynamic_cast<cssom::TransformListValue*>(style->transform().get());
  ASSERT_NE(scoped_refptr<cssom::TransformListValue>(), transform_list);
  ASSERT_EQ(1, transform_list->value().size());

  const cssom::TranslateZFunction* translate_z_function =
      dynamic_cast<const cssom::TranslateZFunction*>(
          transform_list->value()[0]);
  ASSERT_NE(static_cast<cssom::TranslateZFunction*>(NULL),
            translate_z_function);

  scoped_refptr<cssom::LengthValue> offset = translate_z_function->offset();
  EXPECT_FLOAT_EQ(-22, offset->value());
  EXPECT_EQ(cssom::kPixelsUnit, offset->unit());
}

TEST_F(ParserTest, ParsesMultipleTransforms) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".multiple-transforms {\n"
          "  transform: scale(2) translateZ(10px);\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::TransformListValue> transform_list =
      dynamic_cast<cssom::TransformListValue*>(style->transform().get());
  ASSERT_NE(scoped_refptr<cssom::TransformListValue>(), transform_list);
  ASSERT_EQ(2, transform_list->value().size());
  EXPECT_NE(static_cast<cssom::TransformFunction*>(NULL),
            transform_list->value()[0]);
  EXPECT_NE(static_cast<cssom::TransformFunction*>(NULL),
            transform_list->value()[1]);
}

TEST_F(ParserTest, RecoversFromInvalidTransformList) {
  EXPECT_CALL(
      parser_observer_,
      OnWarning(
          "[object ParserTest]:2:27: warning: invalid transform function"));

  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".black-hole {\n"
          "  transform: scale(0.001) warp(spacetime);\n"
          "  color: #000;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  EXPECT_NE(scoped_refptr<cssom::PropertyValue>(), style->color());
}

TEST_F(ParserTest, ParsesWidth) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      GetStyleOfOnlyRuleInStyleSheet(parser_.ParseStyleSheet(
          ".fixed-width {\n"
          "  width: 100px;\n"
          "}\n",
          base::SourceLocation("[object ParserTest]", 1, 1)));

  scoped_refptr<cssom::LengthValue> width =
      dynamic_cast<cssom::LengthValue*>(style->width().get());
  ASSERT_NE(scoped_refptr<cssom::LengthValue>(), width);
  EXPECT_FLOAT_EQ(100, width->value());
  EXPECT_EQ(cssom::kPixelsUnit, width->unit());
}

TEST_F(ParserTest, DISABLED_ParseDeclarationList) {
  scoped_refptr<cssom::CSSStyleDeclaration> style =
      parser_.ParseDeclarationList(
          "color: rgba(255, 128, 1, 0.5);"
          "border-radius: 0.2em;"
          "font-family: \"Droid Sans\";"
          "font-size: 100px;",
          base::SourceLocation("[object ParserTest]", 1, 1));

  scoped_refptr<cssom::RGBAColorValue> color =
      dynamic_cast<cssom::RGBAColorValue*>(style->color().get());
  ASSERT_NE(scoped_refptr<cssom::RGBAColorValue>(), color);
  EXPECT_EQ(0xff80017f, color->value());

  scoped_refptr<cssom::LengthValue> border_radius =
      dynamic_cast<cssom::LengthValue*>(style->border_radius().get());
  ASSERT_NE(scoped_refptr<cssom::LengthValue>(), border_radius);
  EXPECT_FLOAT_EQ(0.2f, border_radius->value());
  EXPECT_EQ(cssom::kFontSizesAkaEmUnit, border_radius->unit());

  scoped_refptr<cssom::StringValue> font_family =
      dynamic_cast<cssom::StringValue*>(style->font_family().get());
  ASSERT_NE(scoped_refptr<cssom::StringValue>(), font_family);
  EXPECT_EQ("Droid Sans", font_family->value());

  scoped_refptr<cssom::LengthValue> font_size =
      dynamic_cast<cssom::LengthValue*>(style->font_size().get());
  ASSERT_NE(scoped_refptr<cssom::LengthValue>(), font_size);
  EXPECT_FLOAT_EQ(100, font_size->value());
  EXPECT_EQ(cssom::kPixelsUnit, font_size->unit());

  EXPECT_EQ(scoped_refptr<cssom::PropertyValue>(), style->background());
  EXPECT_EQ(scoped_refptr<cssom::PropertyValue>(), style->background_color());
  EXPECT_EQ(scoped_refptr<cssom::PropertyValue>(), style->background_image());
  EXPECT_EQ(scoped_refptr<cssom::PropertyValue>(), style->display());
  EXPECT_EQ(scoped_refptr<cssom::PropertyValue>(), style->font_weight());
  EXPECT_EQ(scoped_refptr<cssom::PropertyValue>(), style->height());
  EXPECT_EQ(scoped_refptr<cssom::PropertyValue>(), style->opacity());
  EXPECT_EQ(scoped_refptr<cssom::PropertyValue>(), style->overflow());
  EXPECT_EQ(scoped_refptr<cssom::PropertyValue>(), style->transform());
  EXPECT_EQ(scoped_refptr<cssom::PropertyValue>(), style->width());
}

TEST_F(ParserTest, ParsePropertyValue_ParsesValidPropertyValue) {
  scoped_refptr<cssom::RGBAColorValue> color =
      dynamic_cast<cssom::RGBAColorValue*>(
          parser_.ParsePropertyValue(
                      "color", "#c0ffee",
                      base::SourceLocation("[object ParserTest]", 1, 1)).get());
  ASSERT_NE(scoped_refptr<cssom::RGBAColorValue>(), color);
}

TEST_F(ParserTest, ParsePropertyValue_WarnsAboutImportant) {
  EXPECT_CALL(parser_observer_,
              OnError(
                  "[object ParserTest]:1:1: error: !important is not allowed "
                  "in property value"));

  scoped_refptr<cssom::PropertyValue> null_value = parser_.ParsePropertyValue(
      "color", "#f0000d !important",
      base::SourceLocation("[object ParserTest]", 1, 1));
  ASSERT_EQ(scoped_refptr<cssom::PropertyValue>(), null_value);
}

TEST_F(ParserTest, ParsePropertyValue_WarnsAboutTrailingSemicolon) {
  EXPECT_CALL(
      parser_observer_,
      OnError("[object ParserTest]:1:8: error: unrecoverable syntax error"));

  scoped_refptr<cssom::PropertyValue> null_value = parser_.ParsePropertyValue(
      "color", "#baaaad;", base::SourceLocation("[object ParserTest]", 1, 1));
  ASSERT_EQ(scoped_refptr<cssom::PropertyValue>(), null_value);
}

TEST_F(ParserTest, ParsePropertyValue_WarnsAboutInvalidPropertyValue) {
  EXPECT_CALL(parser_observer_,
              OnWarning("[object ParserTest]:1:1: warning: unsupported value"));

  scoped_refptr<cssom::PropertyValue> null_value = parser_.ParsePropertyValue(
      "color", "spring grass",
      base::SourceLocation("[object ParserTest]", 1, 1));
  ASSERT_EQ(scoped_refptr<cssom::PropertyValue>(), null_value);
}

}  // namespace css_parser
}  // namespace cobalt

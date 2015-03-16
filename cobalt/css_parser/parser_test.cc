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
  const scoped_ptr<MockParserObserver> parser_observer_;
  const scoped_ptr<Parser> parser_;
};

ParserTest::ParserTest()
    : parser_observer_(new MockParserObserver()),
      parser_(
          new Parser(base::Bind(&MockParserObserver::OnWarning,
                                base::Unretained(parser_observer_.get())),
                     base::Bind(&MockParserObserver::OnError,
                                base::Unretained(parser_observer_.get())))) {}

// TODO(***REMOVED***): Test every reduction that has semantic action.

TEST_F(ParserTest, ParsesEmptyInput) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css", "");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(0, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, HandlesUnrecoverableSyntaxError) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(
      *parser_observer_,
      OnError("parser_test.css:1:1: error: unrecoverable syntax error"));

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css", "@casino-royale");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(0, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, IgnoresSgmlCommentDelimiters) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css", "<!-- body {} -->");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(1, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, RecoversFromInvalidAtToken) {
  EXPECT_CALL(*parser_observer_,
              OnWarning("parser_test.css:1:9: warning: invalid rule"));
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet = parser_->ParseStyleSheet(
      "parser_test.css", "body {} @cobalt-magic; div {}");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(2, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, RecoversFromInvalidRuleWhichEndsWithSemicolon) {
  EXPECT_CALL(*parser_observer_,
              OnWarning("parser_test.css:1:9: warning: invalid rule"));
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet = parser_->ParseStyleSheet(
      "parser_test.css", "body {} @charset 'utf-8'; div {}");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(2, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, RecoversFromInvalidRuleWhichEndsWithBlock) {
  EXPECT_CALL(*parser_observer_,
              OnWarning("parser_test.css:1:9: warning: invalid rule"));
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet = parser_->ParseStyleSheet(
      "parser_test.css", "body {} !important {} div {}");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);
  ASSERT_EQ(2, style_sheet->css_rules()->length());
}

TEST_F(ParserTest, SilentlyIgnoresNonWebKitProperties) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".double-size {\n"
                               "  -moz-transform: scale(2);\n"
                               "  -ms-transform: scale(2);\n"
                               "  -o-transform: scale(2);\n"
                               "  transform: scale(2);\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  EXPECT_NE(scoped_refptr<cssom::PropertyValue>(), style->transform());
}

TEST_F(ParserTest, WarnsAboutInvalidStandardAndWebKitProperties) {
  EXPECT_CALL(
      *parser_observer_,
      OnWarning(
          "parser_test.css:2:3: warning: unsupported property -webkit-pony"));
  EXPECT_CALL(
      *parser_observer_,
      OnWarning("parser_test.css:3:3: warning: unsupported property pony"));
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".friendship {\n"
                               "  -webkit-pony: rainbowdash;\n"
                               "  pony: rainbowdash;\n"
                               "  color: #9edbf9;\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  EXPECT_NE(scoped_refptr<cssom::PropertyValue>(), style->color());
}

TEST_F(ParserTest, WarnsAboutInvalidPropertyValues) {
  EXPECT_CALL(*parser_observer_,
              OnWarning("parser_test.css:2:21: warning: unsupported value"));
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);


  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".champs-elysees {\n"
                               "  background-color: morning haze over Paris;\n"
                               "  color: #fff;\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  EXPECT_NE(scoped_refptr<cssom::PropertyValue>(), style->color());
}

// TODO(***REMOVED***): Test selectors.

TEST_F(ParserTest, ParsesInherit) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".like-father-like-son {\n"
                               "  background-color: inherit;\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  EXPECT_EQ(cssom::KeywordValue::GetInherit(), style->background_color());
}

TEST_F(ParserTest, ParsesInitial) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".reset {\n"
                               "  background-color: initial;\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  EXPECT_EQ(cssom::KeywordValue::GetInitial(), style->background_color());
}

TEST_F(ParserTest, ParsesBackgroundColor) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".clean {\n"
                               "  background-color: #fff;\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  scoped_refptr<cssom::RGBAColorValue> background_color =
      dynamic_cast<cssom::RGBAColorValue*>(style->background_color().get());
  ASSERT_NE(scoped_refptr<cssom::RGBAColorValue>(), background_color);
  EXPECT_EQ(0xffffffff, background_color->value());
}

TEST_F(ParserTest, Parses3DigitColor) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".dark {\n"
                               "  color: #123;\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  scoped_refptr<cssom::RGBAColorValue> color =
      dynamic_cast<cssom::RGBAColorValue*>(style->color().get());
  ASSERT_NE(scoped_refptr<cssom::RGBAColorValue>(), color);
  EXPECT_EQ(0x112233ff, color->value());
}

TEST_F(ParserTest, Parses6DigitColor) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".logo {\n"
                               "  color: #0047ab;  /* Cobalt blue */\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  scoped_refptr<cssom::RGBAColorValue> color =
      dynamic_cast<cssom::RGBAColorValue*>(style->color().get());
  ASSERT_NE(scoped_refptr<cssom::RGBAColorValue>(), color);
  EXPECT_EQ(0x0047abff, color->value());
}

TEST_F(ParserTest, ParsesFontFamily) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".nexus {\n"
                               "  font-family: \"Droid Sans\";\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  scoped_refptr<cssom::StringValue> font_family =
      dynamic_cast<cssom::StringValue*>(style->font_family().get());
  ASSERT_NE(scoped_refptr<cssom::StringValue>(), font_family);
  EXPECT_EQ("Droid Sans", font_family->value());
}

// TODO(***REMOVED***): Test other length units, including negative ones.

TEST_F(ParserTest, ParsesFontSize) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".large {\n"
                               "  font-size: 100px;\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  scoped_refptr<cssom::LengthValue> font_size =
      dynamic_cast<cssom::LengthValue*>(style->font_size().get());
  ASSERT_NE(scoped_refptr<cssom::LengthValue>(), font_size);
  EXPECT_FLOAT_EQ(100, font_size->value());
  EXPECT_EQ(cssom::kPixelsUnit, font_size->unit());
}

TEST_F(ParserTest, ParsesOpacity) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".transparent {\n"
                               "  opacity: -3.14;\n"
                               "}\n"
                               ".translucent {\n"
                               "  opacity: 0.5;\n"
                               "}\n"
                               ".opaque {\n"
                               "  opacity: 2.72;\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(3, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> transparent_style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), transparent_style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> transparent_style =
      transparent_style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), transparent_style);

  scoped_refptr<cssom::NumberValue> transparent =
      dynamic_cast<cssom::NumberValue*>(transparent_style->opacity().get());
  ASSERT_NE(scoped_refptr<cssom::NumberValue>(), transparent);
  EXPECT_FLOAT_EQ(-3.14f, transparent->value());

  scoped_refptr<cssom::CSSStyleRule> translucent_style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(1).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), translucent_style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> translucent_style =
      translucent_style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), translucent_style);

  scoped_refptr<cssom::NumberValue> translucent =
      dynamic_cast<cssom::NumberValue*>(translucent_style->opacity().get());
  ASSERT_NE(scoped_refptr<cssom::NumberValue>(), translucent);
  EXPECT_FLOAT_EQ(0.5f, translucent->value());

  scoped_refptr<cssom::CSSStyleRule> opaque_style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(2).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), opaque_style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> opaque_style =
      opaque_style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), opaque_style);

  scoped_refptr<cssom::NumberValue> opaque =
      dynamic_cast<cssom::NumberValue*>(opaque_style->opacity().get());
  ASSERT_NE(scoped_refptr<cssom::NumberValue>(), opaque);
  EXPECT_FLOAT_EQ(2.72f, opaque->value());
}

TEST_F(ParserTest, ParsesOverflow) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".hidden {\n"
                               "  overflow: hidden;\n"
                               "}\n"
                               ".visible {\n"
                               "  overflow: visible;\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(2, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> hidden_style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), hidden_style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> hidden_style =
      hidden_style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), hidden_style);

  EXPECT_EQ(cssom::KeywordValue::GetHidden(), hidden_style->overflow());

  scoped_refptr<cssom::CSSStyleRule> visible_style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(1).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), visible_style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> visible_style =
      visible_style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), visible_style);

  EXPECT_EQ(cssom::KeywordValue::GetVisible(), visible_style->overflow());
}

TEST_F(ParserTest, ParsesNoneTransform) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".normal-state {\n"
                               "  transform: none;\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  EXPECT_EQ(cssom::KeywordValue::GetNone(), style->transform());
}

TEST_F(ParserTest, ParsesIsotropicScaleTransform) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".isotropic-scale {\n"
                               "  transform: scale(1.5);\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  scoped_refptr<cssom::TransformListValue> transform_list =
      dynamic_cast<cssom::TransformListValue*>(style->transform().get());
  ASSERT_NE(scoped_refptr<cssom::TransformListValue>(), transform_list);
  ASSERT_EQ(1, transform_list->value().size());

  const cssom::ScaleFunction* scale_function =
      dynamic_cast<const cssom::ScaleFunction*>(transform_list->value()[0]);
  ASSERT_NE(static_cast<cssom::ScaleFunction*>(NULL), scale_function);

  scoped_refptr<cssom::NumberValue> x_factor =
      dynamic_cast<cssom::NumberValue*>(scale_function->x_factor().get());
  ASSERT_NE(scoped_refptr<cssom::NumberValue>(), x_factor);
  EXPECT_FLOAT_EQ(1.5, x_factor->value());

  scoped_refptr<cssom::NumberValue> y_factor =
      dynamic_cast<cssom::NumberValue*>(scale_function->y_factor().get());
  ASSERT_NE(scoped_refptr<cssom::NumberValue>(), y_factor);
  EXPECT_FLOAT_EQ(1.5, y_factor->value());
}

// TODO(***REMOVED***): Test integers, including negative ones.
// TODO(***REMOVED***): Test reals, including negative ones.
// TODO(***REMOVED***): Test non-zero lengths without units.

TEST_F(ParserTest, ParsesAnisotropicScaleTransform) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".anisotropic-scale {\n"
                               "  transform: scale(16, 9);\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  scoped_refptr<cssom::TransformListValue> transform_list =
      dynamic_cast<cssom::TransformListValue*>(style->transform().get());
  ASSERT_NE(scoped_refptr<cssom::TransformListValue>(), transform_list);
  ASSERT_EQ(1, transform_list->value().size());

  const cssom::ScaleFunction* scale_function =
      dynamic_cast<const cssom::ScaleFunction*>(transform_list->value()[0]);
  ASSERT_NE(static_cast<cssom::ScaleFunction*>(NULL), scale_function);

  scoped_refptr<cssom::NumberValue> x_factor =
      dynamic_cast<cssom::NumberValue*>(scale_function->x_factor().get());
  ASSERT_NE(scoped_refptr<cssom::NumberValue>(), x_factor);
  EXPECT_FLOAT_EQ(16, x_factor->value());

  scoped_refptr<cssom::NumberValue> y_factor =
      dynamic_cast<cssom::NumberValue*>(scale_function->y_factor().get());
  ASSERT_NE(scoped_refptr<cssom::NumberValue>(), y_factor);
  EXPECT_FLOAT_EQ(9, y_factor->value());
}

TEST_F(ParserTest, ParsesTranslateXTransform) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".translate-x {\n"
                               "  transform: translateX(0);\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  scoped_refptr<cssom::TransformListValue> transform_list =
      dynamic_cast<cssom::TransformListValue*>(style->transform().get());
  ASSERT_NE(scoped_refptr<cssom::TransformListValue>(), transform_list);
  ASSERT_EQ(1, transform_list->value().size());

  const cssom::TranslateXFunction* translate_x_function =
      dynamic_cast<const cssom::TranslateXFunction*>(
          transform_list->value()[0]);
  ASSERT_NE(static_cast<cssom::TranslateXFunction*>(NULL),
            translate_x_function);

  scoped_refptr<cssom::LengthValue> offset =
      dynamic_cast<cssom::LengthValue*>(translate_x_function->offset().get());
  ASSERT_NE(scoped_refptr<cssom::LengthValue>(), offset);
  EXPECT_FLOAT_EQ(0, offset->value());
  EXPECT_EQ(cssom::kPixelsUnit, offset->unit());
}

TEST_F(ParserTest, ParsesTranslateYTransform) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".translate-y {\n"
                               "  transform: translateY(30em);\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  scoped_refptr<cssom::TransformListValue> transform_list =
      dynamic_cast<cssom::TransformListValue*>(style->transform().get());
  ASSERT_NE(scoped_refptr<cssom::TransformListValue>(), transform_list);
  ASSERT_EQ(1, transform_list->value().size());

  const cssom::TranslateYFunction* translate_y_function =
      dynamic_cast<const cssom::TranslateYFunction*>(
          transform_list->value()[0]);
  ASSERT_NE(static_cast<cssom::TranslateYFunction*>(NULL),
            translate_y_function);

  scoped_refptr<cssom::LengthValue> offset =
      dynamic_cast<cssom::LengthValue*>(translate_y_function->offset().get());
  ASSERT_NE(scoped_refptr<cssom::LengthValue>(), offset);
  EXPECT_FLOAT_EQ(30, offset->value());
  EXPECT_EQ(cssom::kFontSizesAkaEmUnit, offset->unit());
}

TEST_F(ParserTest, ParsesTranslateZTransform) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".translate-z {\n"
                               "  transform: translateZ(-22px);\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  scoped_refptr<cssom::TransformListValue> transform_list =
      dynamic_cast<cssom::TransformListValue*>(style->transform().get());
  ASSERT_NE(scoped_refptr<cssom::TransformListValue>(), transform_list);
  ASSERT_EQ(1, transform_list->value().size());

  const cssom::TranslateZFunction* translate_z_function =
      dynamic_cast<const cssom::TranslateZFunction*>(
          transform_list->value()[0]);
  ASSERT_NE(static_cast<cssom::TranslateZFunction*>(NULL),
            translate_z_function);

  scoped_refptr<cssom::LengthValue> offset =
      dynamic_cast<cssom::LengthValue*>(translate_z_function->offset().get());
  ASSERT_NE(scoped_refptr<cssom::LengthValue>(), offset);
  EXPECT_FLOAT_EQ(-22, offset->value());
  EXPECT_EQ(cssom::kPixelsUnit, offset->unit());
}

TEST_F(ParserTest, ParsesMultipleTransforms) {
  EXPECT_CALL(*parser_observer_, OnWarning(_)).Times(0);
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".multiple-transforms {\n"
                               "  transform: scale(2) translateZ(10px);\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

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
      *parser_observer_,
      OnWarning("parser_test.css:2:27: warning: invalid transform function"));
  EXPECT_CALL(*parser_observer_, OnError(_)).Times(0);

  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      parser_->ParseStyleSheet("parser_test.css",
                               ".black-hole {\n"
                               "  transform: scale(0.001) warp(spacetime);\n"
                               "  color: #000;\n"
                               "}\n");
  ASSERT_NE(scoped_refptr<cssom::CSSStyleSheet>(), style_sheet);

  scoped_refptr<cssom::CSSRuleList> css_rules = style_sheet->css_rules();
  ASSERT_EQ(1, css_rules->length());

  scoped_refptr<cssom::CSSStyleRule> style_rule =
      dynamic_cast<cssom::CSSStyleRule*>(css_rules->Item(0).get());
  ASSERT_NE(scoped_refptr<cssom::CSSStyleRule>(), style_rule);

  scoped_refptr<cssom::CSSStyleDeclaration> style = style_rule->style();
  ASSERT_NE(scoped_refptr<cssom::CSSStyleDeclaration>(), style);

  EXPECT_NE(scoped_refptr<cssom::PropertyValue>(), style->color());
}

}  // namespace css_parser
}  // namespace cobalt

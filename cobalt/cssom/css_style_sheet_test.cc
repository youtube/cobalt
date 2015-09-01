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

#include "cobalt/cssom/css_rule_list.h"

#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/media_query.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/selector.h"
#include "cobalt/cssom/style_sheet_list.h"
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
                    const base::SourceLocation&,
                    CSSStyleDeclarationData* style_declaration));
  MOCK_METHOD2(ParseMediaQuery,
               scoped_refptr<MediaQuery>(const std::string&,
                                         const base::SourceLocation&));
};

class CSSStyleSheetTest : public ::testing::Test {
 protected:
  CSSStyleSheetTest()
      : style_sheet_list_(new StyleSheetList(NULL)),
        css_style_sheet_(new CSSStyleSheet(&css_parser_)) {
    css_style_sheet_->AttachToStyleSheetList(style_sheet_list_);
  }
  ~CSSStyleSheetTest() OVERRIDE {}

  const scoped_refptr<StyleSheetList> style_sheet_list_;
  const scoped_refptr<CSSStyleSheet> css_style_sheet_;
  MockCSSParser css_parser_;
};

TEST_F(CSSStyleSheetTest, InsertRule) {
  const std::string css_text = "div { font-size: 100px; color: #0047ab; }";
  EXPECT_CALL(css_parser_, ParseStyleRule(css_text, _))
      .WillOnce(testing::Return(scoped_refptr<CSSStyleRule>()));
  css_style_sheet_->InsertRule(css_text, 0);
}

TEST_F(CSSStyleSheetTest, CSSRuleListIsCached) {
  scoped_refptr<CSSRuleList> rule_list_1 = css_style_sheet_->css_rules();
  scoped_refptr<CSSRuleList> rule_list_2 = css_style_sheet_->css_rules();
  ASSERT_EQ(rule_list_1, rule_list_2);
}

TEST_F(CSSStyleSheetTest, CSSRuleListIsLive) {
  scoped_refptr<CSSRuleList> rule_list = css_style_sheet_->css_rules();
  ASSERT_EQ(0, rule_list->length());
  ASSERT_FALSE(rule_list->Item(0).get());

  scoped_refptr<CSSStyleRule> rule =
      new CSSStyleRule(Selectors(), new CSSStyleDeclaration(NULL));
  rule_list->AppendCSSStyleRule(rule);
  css_style_sheet_->set_css_rules(rule_list);
  ASSERT_EQ(1, rule_list->length());
  ASSERT_EQ(rule, rule_list->Item(0));
  ASSERT_FALSE(rule_list->Item(1).get());
  ASSERT_EQ(rule_list, css_style_sheet_->css_rules());
}

}  // namespace cssom
}  // namespace cobalt

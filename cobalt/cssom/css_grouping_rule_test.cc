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

#include "cobalt/cssom/css_grouping_rule.h"

#include <string>

#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/media_query.h"
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

class FakeCSSGroupingRule : public CSSGroupingRule {
 public:
  // From CSSRule.
  Type type() const OVERRIDE { return kMediaRule; }
  std::string css_text() const OVERRIDE { return ""; }
  void set_css_text(const std::string& /* css_text */) OVERRIDE {}
};

class CSSGroupingRuleTest : public ::testing::Test {
 protected:
  CSSGroupingRuleTest()
      : style_sheet_list_(new StyleSheetList(NULL)),
        css_style_sheet_(new CSSStyleSheet(&css_parser_)),
        css_grouping_rule_(new FakeCSSGroupingRule()) {
    css_style_sheet_->AttachToStyleSheetList(style_sheet_list_);
    css_grouping_rule_->AttachToCSSStyleSheet(css_style_sheet_);
  }
  ~CSSGroupingRuleTest() OVERRIDE {}

  const scoped_refptr<StyleSheetList> style_sheet_list_;
  const scoped_refptr<CSSStyleSheet> css_style_sheet_;
  const scoped_refptr<FakeCSSGroupingRule> css_grouping_rule_;
  MockCSSParser css_parser_;
};


TEST_F(CSSGroupingRuleTest, InsertRule) {
  const std::string css_text = "div { font-size: 100px; color: #0047ab; }";

  EXPECT_CALL(css_parser_, ParseStyleRule(css_text, _))
      .WillOnce(testing::Return(scoped_refptr<CSSStyleRule>()));
  css_grouping_rule_->InsertRule(css_text, 0);
}

TEST_F(CSSGroupingRuleTest, CSSRulesGetter) {
  scoped_refptr<CSSRuleList> rule_list = css_grouping_rule_->css_rules();

  ASSERT_TRUE(rule_list);
  ASSERT_EQ(0, rule_list->length());
  ASSERT_FALSE(rule_list->Item(0).get());
}

TEST_F(CSSGroupingRuleTest, CSSRulesSetter) {
  scoped_refptr<CSSRuleList> rule_list = new CSSRuleList();
  css_grouping_rule_->set_css_rules(rule_list);

  ASSERT_EQ(rule_list, css_grouping_rule_->css_rules());
}

}  // namespace cssom
}  // namespace cobalt

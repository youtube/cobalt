// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/cssom/css_grouping_rule.h"

#include <string>

#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/style_sheet_list.h"
#include "cobalt/cssom/testing/mock_css_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

using ::testing::_;
using ::testing::Return;

class FakeCSSGroupingRule : public CSSGroupingRule {
 public:
  // From CSSRule.
  Type type() const override { return kMediaRule; }
  std::string css_text(
      script::ExceptionState* /*exception_state*/) const override {
    return "";
  }
  void set_css_text(const std::string& /* css_text */,
                    script::ExceptionState* /*exception_state*/) override {}
  void AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) override {
    set_parent_style_sheet(style_sheet);
    css_rules()->AttachToCSSStyleSheet(style_sheet);
  }
};

class CSSGroupingRuleTest : public ::testing::Test {
 protected:
  CSSGroupingRuleTest()
      : style_sheet_list_(new StyleSheetList()),
        css_style_sheet_(new CSSStyleSheet(&css_parser_)),
        css_grouping_rule_(new FakeCSSGroupingRule()) {
    css_style_sheet_->AttachToStyleSheetList(style_sheet_list_);
    css_grouping_rule_->AttachToCSSStyleSheet(css_style_sheet_);
  }
  ~CSSGroupingRuleTest() override {}

  const scoped_refptr<StyleSheetList> style_sheet_list_;
  const scoped_refptr<CSSStyleSheet> css_style_sheet_;
  const scoped_refptr<FakeCSSGroupingRule> css_grouping_rule_;
  testing::MockCSSParser css_parser_;
};

TEST_F(CSSGroupingRuleTest, InsertRule) {
  const std::string css_text = "div { font-size: 100px; color: #0047ab; }";

  EXPECT_CALL(css_parser_, ParseRule(css_text, _))
      .WillOnce(Return(scoped_refptr<CSSRule>()));
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

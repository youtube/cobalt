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

#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/selector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

class CSSStyleSheetTest : public ::testing::Test {
 protected:
  CSSStyleSheetTest() : css_style_sheet_(new CSSStyleSheet()) {}
  ~CSSStyleSheetTest() OVERRIDE {}

  const scoped_refptr<CSSStyleSheet> css_style_sheet_;
};

TEST_F(CSSStyleSheetTest, CSSRuleListIsCached) {
  scoped_refptr<CSSRuleList> rule_list_1 = css_style_sheet_->css_rules();
  scoped_refptr<CSSRuleList> rule_list_2 = css_style_sheet_->css_rules();
  ASSERT_EQ(rule_list_1, rule_list_2);
}

TEST_F(CSSStyleSheetTest, CSSRuleListIsLive) {
  scoped_refptr<CSSRuleList> rule_list = css_style_sheet_->css_rules();
  ASSERT_EQ(0, rule_list->length());
  ASSERT_EQ(NULL, rule_list->Item(0).get());

  scoped_refptr<CSSStyleRule> rule =
      new CSSStyleRule(Selectors(), new CSSStyleDeclaration());
  css_style_sheet_->AppendRule(rule);
  ASSERT_EQ(1, rule_list->length());
  ASSERT_EQ(rule, rule_list->Item(0));
  ASSERT_EQ(NULL, rule_list->Item(1).get());
}

}  // namespace cssom
}  // namespace cobalt

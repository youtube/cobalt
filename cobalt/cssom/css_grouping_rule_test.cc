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

#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/selector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(CSSGroupingRuleTest, InsertRule) {
  scoped_refptr<CSSGroupingRule> grouping_rule = new CSSGroupingRule();
  scoped_refptr<CSSRuleList> rule_list = new CSSRuleList();
  grouping_rule->set_css_rules(rule_list);

  scoped_refptr<CSSStyleRule> rule =
      new CSSStyleRule(Selectors(), new CSSStyleDeclaration(NULL));
  grouping_rule->InsertRule(rule, 0);
  ASSERT_EQ(1, rule_list->length());
  ASSERT_EQ(rule, rule_list->Item(0));
  ASSERT_TRUE(rule_list->Item(1) == NULL);
}

TEST(CSSGroupingRuleTest, SetCSSRules) {
  scoped_refptr<CSSGroupingRule> grouping_rule = new CSSGroupingRule();

  scoped_refptr<CSSRuleList> rule_list = new CSSRuleList();
  scoped_refptr<CSSStyleRule> rule =
      new CSSStyleRule(Selectors(), new CSSStyleDeclaration(NULL));
  rule_list->AppendCSSStyleRule(rule);

  grouping_rule->set_css_rules(rule_list);
  ASSERT_EQ(rule_list, grouping_rule->css_rules());
  ASSERT_EQ(1, grouping_rule->css_rules()->length());
  ASSERT_EQ(rule, grouping_rule->css_rules()->Item(0));
  ASSERT_EQ(NULL, grouping_rule->css_rules()->Item(1).get());
}

}  // namespace cssom
}  // namespace cobalt

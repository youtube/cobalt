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

#include "cobalt/cssom/css_style_rule.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(CSSRuleListTest, ItemAccess) {
  scoped_refptr<CSSRuleList> rule_list = new CSSRuleList();
  ASSERT_EQ(0, rule_list->length());
  ASSERT_TRUE(rule_list->Item(0) == NULL);

  scoped_refptr<CSSRule> rule = new CSSStyleRule();
  rule_list->Append(rule);
  ASSERT_EQ(1, rule_list->length());
  ASSERT_EQ(rule, rule_list->Item(0));
  ASSERT_TRUE(rule_list->Item(1) == NULL);
}

}  // namespace cssom
}  // namespace cobalt

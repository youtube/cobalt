// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/css_rule_list.h"

#include "cobalt/cssom/css_font_face_rule.h"
#include "cobalt/cssom/css_media_rule.h"
#include "cobalt/cssom/css_rule_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/media_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(CSSRuleListTest, ItemAccess) {
  scoped_refptr<CSSRuleList> rule_list = new CSSRuleList();
  ASSERT_EQ(0, rule_list->length());
  ASSERT_FALSE(rule_list->Item(0));

  scoped_refptr<CSSStyleRule> rule =
      new CSSStyleRule(Selectors(), new CSSRuleStyleDeclaration(NULL));
  rule_list->AppendCSSRule(rule);
  ASSERT_EQ(1, rule_list->length());
  ASSERT_EQ(rule, rule_list->Item(0));
  ASSERT_FALSE(rule_list->Item(1));
}

TEST(CSSRuleListTest, AppendCSSRuleShouldTakeCSSStyleRule) {
  scoped_refptr<CSSRuleList> rule_list = new CSSRuleList();
  scoped_refptr<CSSStyleRule> rule = new CSSStyleRule();
  rule_list->AppendCSSRule(rule);

  ASSERT_EQ(1, rule_list->length());
  ASSERT_EQ(CSSRule::kStyleRule, rule_list->Item(0)->type());
  ASSERT_EQ(rule, rule_list->Item(0));
  ASSERT_FALSE(rule_list->Item(1).get());
}

TEST(CSSRuleListTest, AppendCSSRuleShouldTakeCSSFontFaceRule) {
  scoped_refptr<CSSRuleList> rule_list = new CSSRuleList();
  scoped_refptr<CSSFontFaceRule> rule = new CSSFontFaceRule();
  rule_list->AppendCSSRule(rule);

  ASSERT_EQ(1, rule_list->length());
  ASSERT_EQ(CSSRule::kFontFaceRule, rule_list->Item(0)->type());
  ASSERT_EQ(rule, rule_list->Item(0));
  ASSERT_FALSE(rule_list->Item(1).get());
}

TEST(CSSRuleListTest, AppendCSSRuleShouldTakeCSSMediaRule) {
  scoped_refptr<CSSRuleList> rule_list = new CSSRuleList();
  scoped_refptr<CSSMediaRule> rule =
      new CSSMediaRule(new MediaList(), new CSSRuleList());
  rule_list->AppendCSSRule(rule);

  ASSERT_EQ(1, rule_list->length());
  ASSERT_EQ(CSSRule::kMediaRule, rule_list->Item(0)->type());
  ASSERT_EQ(rule, rule_list->Item(0));
  ASSERT_FALSE(rule_list->Item(1).get());
}

}  // namespace cssom
}  // namespace cobalt

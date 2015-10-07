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

#include "cobalt/cssom/cascaded_style.h"

#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/cascade_priority.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/cssom/css_style_rule.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(CascadedStyleTest, PromoteToCascadedStyle) {
  scoped_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSStyleDeclarationData> style = new CSSStyleDeclarationData();
  RulesWithCascadePriority rules_with_cascade_priority;
  cssom::GURLMap property_name_to_base_url_map;

  scoped_refptr<CSSStyleRule> css_style_rule_1 =
      css_parser->ParseRule(
                    "div {"
                    "  left: 100px !important;"
                    "  right: 100px;"
                    "  width: 100px;"
                    "  height: 100px;"
                    "}",
                    base::SourceLocation("[object CascadedStyleTest]", 1, 1))
          ->AsCSSStyleRule();
  CascadePriority cascade_priority_1(kNormalUserAgent);  // Lowest priority.
  rules_with_cascade_priority.push_back(
      std::make_pair(css_style_rule_1, cascade_priority_1));

  scoped_refptr<CSSStyleRule> css_style_rule_2 =
      css_parser->ParseRule(
                    "div {"
                    "  left: 200px;"
                    "  right: 200px !important;"
                    "  width: 200px;"
                    "  height: 200px;"
                    "}",
                    base::SourceLocation("[object CascadedStyleTest]", 1, 1))
          ->AsCSSStyleRule();
  CascadePriority cascade_priority_2(kNormalOverride);  // Highest priority.
  rules_with_cascade_priority.push_back(
      std::make_pair(css_style_rule_2, cascade_priority_2));

  scoped_refptr<CSSStyleRule> css_style_rule_3 =
      css_parser->ParseRule(
                    "div {"
                    "  left: 300px;"
                    "  right: 300px;"
                    "  width: 300px !important;"
                    "  height: 300px;"
                    "}",
                    base::SourceLocation("[object CascadedStyleTest]", 1, 1))
          ->AsCSSStyleRule();
  CascadePriority cascade_priority_3(kNormalAuthor);  // Middle priority.
  rules_with_cascade_priority.push_back(
      std::make_pair(css_style_rule_3, cascade_priority_3));

  PromoteToCascadedStyle(style, &rules_with_cascade_priority,
                         &property_name_to_base_url_map);

  EXPECT_EQ(style->left(), css_style_rule_1->style()->data()->left());
  EXPECT_EQ(style->right(), css_style_rule_2->style()->data()->right());
  EXPECT_EQ(style->width(), css_style_rule_3->style()->data()->width());
  EXPECT_EQ(style->height(), css_style_rule_2->style()->data()->height());
}

}  // namespace cssom
}  // namespace cobalt

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
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/css_declaration_data.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/css_rule_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/keyword_value.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(CascadedStyleTest, PromoteToCascadedStyle) {
  scoped_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();
  RulesWithCascadePriority rules_with_cascade_priority;
  cssom::GURLMap property_key_to_base_url_map;

  style->SetPropertyValueAndImportance(kVerticalAlignProperty,
                                       KeywordValue::GetBottom(), true);
  style->SetPropertyValueAndImportance(kTextAlignProperty,
                                       KeywordValue::GetLeft(), false);

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
                    "  vertical-align: top !important;"
                    "  text-align: center !important;"
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

  scoped_refptr<cssom::CSSComputedStyleData> computed_style =
      PromoteToCascadedStyle(style, &rules_with_cascade_priority,
                             &property_key_to_base_url_map);

  EXPECT_EQ(computed_style->left(),
            css_style_rule_1->declared_style_data()->GetPropertyValue(
                cssom::kLeftProperty));
  EXPECT_EQ(computed_style->right(),
            css_style_rule_2->declared_style_data()->GetPropertyValue(
                cssom::kRightProperty));
  EXPECT_EQ(computed_style->width(),
            css_style_rule_3->declared_style_data()->GetPropertyValue(
                cssom::kWidthProperty));
  EXPECT_EQ(computed_style->height(),
            css_style_rule_2->declared_style_data()->GetPropertyValue(
                cssom::kHeightProperty));
  EXPECT_EQ(computed_style->vertical_align(),
            style->GetPropertyValue(cssom::kVerticalAlignProperty));
  EXPECT_EQ(computed_style->text_align(),
            css_style_rule_2->declared_style_data()->GetPropertyValue(
                cssom::kTextAlignProperty));
}

TEST(CascadedStyleTest, PromoteToCascadedStyleWithBackgroundImage) {
  scoped_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();
  RulesWithCascadePriority rules_with_cascade_priority;
  cssom::GURLMap property_key_to_base_url_map;

  scoped_refptr<CSSStyleRule> css_style_rule_1 =
      css_parser->ParseRule(
                    "div {"
                    "  left: 100px !important;"
                    "  right: 100px;"
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
                    "  background-image: url(foo.png);"
                    "}",
                    base::SourceLocation("[object CascadedStyleTest]", 1, 1))
          ->AsCSSStyleRule();
  CascadePriority cascade_priority_2(kNormalOverride);  // Highest priority.
  rules_with_cascade_priority.push_back(
      std::make_pair(css_style_rule_2, cascade_priority_2));

  scoped_refptr<CSSStyleSheet> parent_style_sheet(new CSSStyleSheet());
  parent_style_sheet->SetLocationUrl(GURL("https:///www.youtube.com/tv/img"));
  css_style_rule_2->set_parent_style_sheet(parent_style_sheet.get());

  scoped_refptr<cssom::CSSComputedStyleData> computed_style =
      PromoteToCascadedStyle(style, &rules_with_cascade_priority,
                             &property_key_to_base_url_map);

  EXPECT_EQ(computed_style->left(),
            css_style_rule_1->declared_style_data()->GetPropertyValue(
                cssom::kLeftProperty));
  EXPECT_EQ(computed_style->right(),
            css_style_rule_2->declared_style_data()->GetPropertyValue(
                cssom::kRightProperty));
  EXPECT_EQ(computed_style->background_image(),
            css_style_rule_2->declared_style_data()->GetPropertyValue(
                cssom::kBackgroundImageProperty));
  ASSERT_FALSE(property_key_to_base_url_map.empty());
  EXPECT_EQ(property_key_to_base_url_map[kBackgroundImageProperty].spec(),
            "https://www.youtube.com/tv/img");
}

}  // namespace cssom
}  // namespace cobalt

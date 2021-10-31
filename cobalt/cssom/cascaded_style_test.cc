// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/cssom/cascaded_style.h"

#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/cascade_precedence.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/css_declaration_data.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/css_rule_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/keyword_value.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace cssom {

TEST(CascadedStyleTest, PromoteToCascadedStyle) {
  std::unique_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();
  RulesWithCascadePrecedence rules_with_cascade_precedence;
  GURLMap property_key_to_base_url_map;

  style->SetPropertyValueAndImportance(kVerticalAlignProperty,
                                       KeywordValue::GetBottom(), true);
  style->SetPropertyValueAndImportance(kTextAlignProperty,
                                       KeywordValue::GetLeft(), false);

  // The order of cascade precedence of the following rules:
  // rule 2 > rule 3 > rule 1.

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
  CascadePrecedence cascade_precedence_1(kNormalUserAgent);
  rules_with_cascade_precedence.push_back(
      std::make_pair(css_style_rule_1, cascade_precedence_1));

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
  CascadePrecedence cascade_precedence_2(kNormalOverride);
  rules_with_cascade_precedence.push_back(
      std::make_pair(css_style_rule_2, cascade_precedence_2));

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
  CascadePrecedence cascade_precedence_3(kNormalAuthor);
  rules_with_cascade_precedence.push_back(
      std::make_pair(css_style_rule_3, cascade_precedence_3));

  scoped_refptr<CSSComputedStyleData> computed_style = PromoteToCascadedStyle(
      style, &rules_with_cascade_precedence, &property_key_to_base_url_map);

  EXPECT_EQ(
      computed_style->left(),
      css_style_rule_1->declared_style_data()->GetPropertyValue(kLeftProperty));
  EXPECT_EQ(computed_style->right(),
            css_style_rule_2->declared_style_data()->GetPropertyValue(
                kRightProperty));
  EXPECT_EQ(computed_style->width(),
            css_style_rule_3->declared_style_data()->GetPropertyValue(
                kWidthProperty));
  EXPECT_EQ(computed_style->height(),
            css_style_rule_2->declared_style_data()->GetPropertyValue(
                kHeightProperty));
  EXPECT_EQ(computed_style->vertical_align(),
            style->GetPropertyValue(kVerticalAlignProperty));
  EXPECT_EQ(computed_style->text_align(),
            css_style_rule_2->declared_style_data()->GetPropertyValue(
                kTextAlignProperty));
}

TEST(CascadedStyleTest, PromoteToCascadedStyleWithBackgroundImage) {
  std::unique_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();
  RulesWithCascadePrecedence rules_with_cascade_precedence;
  GURLMap property_key_to_base_url_map;

  // The order of cascade precedence of the following rules:
  // rule 2 > rule 1.

  scoped_refptr<CSSStyleRule> css_style_rule_1 =
      css_parser->ParseRule(
                    "div {"
                    "  left: 100px !important;"
                    "  right: 100px;"
                    "}",
                    base::SourceLocation("[object CascadedStyleTest]", 1, 1))
          ->AsCSSStyleRule();
  CascadePrecedence cascade_precedence_1(kNormalUserAgent);
  rules_with_cascade_precedence.push_back(
      std::make_pair(css_style_rule_1, cascade_precedence_1));

  scoped_refptr<CSSStyleRule> css_style_rule_2 =
      css_parser->ParseRule(
                    "div {"
                    "  left: 200px;"
                    "  right: 200px !important;"
                    "  background-image: url(foo.png);"
                    "}",
                    base::SourceLocation("[object CascadedStyleTest]", 1, 1))
          ->AsCSSStyleRule();
  CascadePrecedence cascade_precedence_2(kNormalOverride);
  rules_with_cascade_precedence.push_back(
      std::make_pair(css_style_rule_2, cascade_precedence_2));

  scoped_refptr<CSSStyleSheet> parent_style_sheet(new CSSStyleSheet());
  parent_style_sheet->SetOriginClean(true);
  parent_style_sheet->SetLocationUrl(GURL("https:///www.youtube.com/tv/img"));
  css_style_rule_2->set_parent_style_sheet(parent_style_sheet.get());

  scoped_refptr<CSSComputedStyleData> computed_style = PromoteToCascadedStyle(
      style, &rules_with_cascade_precedence, &property_key_to_base_url_map);

  EXPECT_EQ(
      computed_style->left(),
      css_style_rule_1->declared_style_data()->GetPropertyValue(kLeftProperty));
  EXPECT_EQ(computed_style->right(),
            css_style_rule_2->declared_style_data()->GetPropertyValue(
                kRightProperty));
  EXPECT_EQ(computed_style->background_image(),
            css_style_rule_2->declared_style_data()->GetPropertyValue(
                kBackgroundImageProperty));
  ASSERT_FALSE(property_key_to_base_url_map.empty());
  EXPECT_EQ(property_key_to_base_url_map[kBackgroundImageProperty].spec(),
            "https://www.youtube.com/tv/img");
}

TEST(CascadedStyleTest,
     PromoteToCascadedStyleWithParentStyleSheetLocationUnset) {
  std::unique_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();
  RulesWithCascadePrecedence rules_with_cascade_precedence;
  GURLMap property_key_to_base_url_map;
  property_key_to_base_url_map[kBackgroundImageProperty] =
      GURL("https://www.youtube.com/tv/img");

  scoped_refptr<CSSStyleRule> css_style_rule =
      css_parser->ParseRule(
                    "div {"
                    "  left: 200px;"
                    "  right: 200px !important;"
                    "  background-image: url(foo.png);"
                    "}",
                    base::SourceLocation("[object CascadedStyleTest]", 1, 1))
          ->AsCSSStyleRule();
  CascadePrecedence cascade_precedence(kNormalOverride);
  rules_with_cascade_precedence.push_back(
      std::make_pair(css_style_rule, cascade_precedence));

  scoped_refptr<CSSStyleSheet> parent_style_sheet(new CSSStyleSheet());
  parent_style_sheet->SetOriginClean(true);
  css_style_rule->set_parent_style_sheet(parent_style_sheet.get());

  scoped_refptr<CSSComputedStyleData> computed_style = PromoteToCascadedStyle(
      style, &rules_with_cascade_precedence, &property_key_to_base_url_map);

  EXPECT_EQ(
      computed_style->left(),
      css_style_rule->declared_style_data()->GetPropertyValue(kLeftProperty));
  EXPECT_EQ(
      computed_style->right(),
      css_style_rule->declared_style_data()->GetPropertyValue(kRightProperty));
  EXPECT_EQ(computed_style->background_image(),
            css_style_rule->declared_style_data()->GetPropertyValue(
                kBackgroundImageProperty));
  ASSERT_FALSE(property_key_to_base_url_map.empty());
  EXPECT_EQ(property_key_to_base_url_map[kBackgroundImageProperty].spec(),
            "https://www.youtube.com/tv/img");
}

TEST(CascadedStyleTest,
     PromoteToCascadedStyleWithHigherBackgroundImagePriorityInFirstRule) {
  std::unique_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSDeclaredStyleData> style = new CSSDeclaredStyleData();
  RulesWithCascadePrecedence rules_with_cascade_precedence;
  GURLMap property_key_to_base_url_map;

  // The order of cascade precedence of the following rules:
  // rule 2 > rule 1.

  scoped_refptr<CSSStyleRule> css_style_rule_1 =
      css_parser->ParseRule(
                    "div {"
                    "  right: 100px;"
                    "  background-image: url(bar.png) !important;"
                    "}",
                    base::SourceLocation("[object CascadedStyleTest]", 1, 1))
          ->AsCSSStyleRule();
  CascadePrecedence cascade_precedence_1(kNormalUserAgent);
  rules_with_cascade_precedence.push_back(
      std::make_pair(css_style_rule_1, cascade_precedence_1));

  scoped_refptr<CSSStyleSheet> parent_style_sheet_1(new CSSStyleSheet());
  parent_style_sheet_1->SetLocationUrl(
      GURL("https:///www.youtube.com/tv/img1"));
  css_style_rule_1->set_parent_style_sheet(parent_style_sheet_1.get());

  scoped_refptr<CSSStyleRule> css_style_rule_2 =
      css_parser->ParseRule(
                    "div {"
                    "  right: 200px !important;"
                    "  background-image: url(foo.png);"
                    "}",
                    base::SourceLocation("[object CascadedStyleTest]", 1, 1))
          ->AsCSSStyleRule();
  CascadePrecedence cascade_precedence_2(kNormalOverride);
  rules_with_cascade_precedence.push_back(
      std::make_pair(css_style_rule_2, cascade_precedence_2));

  scoped_refptr<CSSStyleSheet> parent_style_sheet_2(new CSSStyleSheet());
  parent_style_sheet_2->SetLocationUrl(
      GURL("https:///www.youtube.com/tv/img2"));
  css_style_rule_2->set_parent_style_sheet(parent_style_sheet_2.get());

  scoped_refptr<CSSComputedStyleData> computed_style = PromoteToCascadedStyle(
      style, &rules_with_cascade_precedence, &property_key_to_base_url_map);

  EXPECT_EQ(computed_style->right(),
            css_style_rule_2->declared_style_data()->GetPropertyValue(
                kRightProperty));
  EXPECT_EQ(computed_style->background_image(),
            css_style_rule_1->declared_style_data()->GetPropertyValue(
                kBackgroundImageProperty));
  ASSERT_FALSE(property_key_to_base_url_map.empty());
  EXPECT_EQ(property_key_to_base_url_map[kBackgroundImageProperty].spec(),
            "https://www.youtube.com/tv/img1");
}

}  // namespace cssom
}  // namespace cobalt

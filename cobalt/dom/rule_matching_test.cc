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

#include "cobalt/dom/rule_matching.h"

#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom_parser/parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class RuleMatchingTest : public ::testing::Test {
 protected:
  RuleMatchingTest()
      : css_parser_(css_parser::Parser::Create()),
        dom_parser_(new dom_parser::Parser()),
        html_element_context_(NULL, css_parser_.get(), dom_parser_.get(), NULL,
                              NULL, NULL),
        document_(new Document(&html_element_context_, Document::Options())),
        root_(new Element(document_)) {}

  ~RuleMatchingTest() OVERRIDE {}

  void MatchRules(Element *element) {
    HTMLElement* html_element = element->AsHTMLElement();

    html_element->ClearMatchingRules();

    style_sheet_->MaybeUpdateRuleIndexes();
    GetMatchingRulesFromStyleSheet(style_sheet_, html_element,
                                   cssom::kNormalAuthor);
    matching_rules_ = html_element->matching_rules();
    for (int i = 0; i < kMaxPseudoElementType; ++i) {
      PseudoElementType pseudo_element_type = static_cast<PseudoElementType>(i);
      PseudoElement* pseudo_element =
          html_element->pseudo_element(pseudo_element_type);
      if (pseudo_element) {
        pseudo_element_matching_rules_[pseudo_element_type] =
            pseudo_element->matching_rules();
      } else {
        pseudo_element_matching_rules_[pseudo_element_type] = NULL;
      }
    }
  }

  scoped_ptr<css_parser::Parser> css_parser_;
  scoped_ptr<dom_parser::Parser> dom_parser_;
  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
  scoped_refptr<Element> root_;

  scoped_refptr<cssom::CSSStyleSheet> style_sheet_;
  cssom::RulesWithCascadePriority *matching_rules_;
  cssom::RulesWithCascadePriority *
      pseudo_element_matching_rules_[kMaxPseudoElementType];
};

// *:after should match <div/> and <span/>.
TEST_F(RuleMatchingTest, DISABLED_AfterPseudoElementMatchGlobal) {
  // Generate rules in the style sheet from the following CSS code.
  style_sheet_ = css_parser_->ParseStyleSheet(
      "*:after {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));

  // Generate DOM elements from the following HTML code.
  root_->set_inner_html("<div class=\"a\"/><span class=\"a\"/>");

  // Get the matching rules and check the result.
  MatchRules(root_->first_element_child());
  cssom::RulesWithCascadePriority *after_matching_rule =
      pseudo_element_matching_rules_[kAfterPseudoElementType];
  ASSERT_TRUE(after_matching_rule);
  ASSERT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kBeforePseudoElementType]);
  ASSERT_EQ(1, after_matching_rule->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0),
            (*after_matching_rule)[0].first);

  MatchRules(root_->last_element_child());
  after_matching_rule = pseudo_element_matching_rules_[kAfterPseudoElementType];
  ASSERT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kBeforePseudoElementType]);
  ASSERT_TRUE(after_matching_rule);
  ASSERT_EQ(1, after_matching_rule->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0),
            (*after_matching_rule)[0].first);
}

// div:after should match <div/>.
TEST_F(RuleMatchingTest, AfterPseudoElementSelectorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div:after {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/>");

  MatchRules(root_->first_element_child());
  cssom::RulesWithCascadePriority *after_matching_rule =
      pseudo_element_matching_rules_[kAfterPseudoElementType];
  ASSERT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kBeforePseudoElementType]);
  ASSERT_TRUE(after_matching_rule);
  ASSERT_EQ(1, after_matching_rule->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0),
            (*after_matching_rule)[0].first);
}

// div:after shouldn't match <span/>.
TEST_F(RuleMatchingTest, AfterPseudoElementSelectorNoMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div:after {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<span/>");

  MatchRules(root_->first_element_child());
  EXPECT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kAfterPseudoElementType]);
  ASSERT_FALSE(pseudo_element_matching_rules_[kBeforePseudoElementType]);
}

// :before should match <div/> and <span/>.
TEST_F(RuleMatchingTest, DISABLED_BeforePseudoElementMatchGlobal) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      ":before {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/><span/>");

  MatchRules(root_->first_element_child());
  cssom::RulesWithCascadePriority *before_matching_rule =
      pseudo_element_matching_rules_[kBeforePseudoElementType];
  ASSERT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kAfterPseudoElementType]);
  ASSERT_TRUE(before_matching_rule);
  ASSERT_EQ(1, before_matching_rule->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0),
            (*before_matching_rule)[0].first);

  MatchRules(root_->last_element_child());
  before_matching_rule =
      pseudo_element_matching_rules_[kBeforePseudoElementType];
  ASSERT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kAfterPseudoElementType]);
  ASSERT_TRUE(before_matching_rule);
  ASSERT_EQ(1, before_matching_rule->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0),
            (*before_matching_rule)[0].first);
}

// div:before should match <div/>.
TEST_F(RuleMatchingTest, BeforePseudoElementSelectorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div:before {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/>");

  MatchRules(root_->first_element_child());
  cssom::RulesWithCascadePriority *before_matching_rule =
      pseudo_element_matching_rules_[kBeforePseudoElementType];
  ASSERT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kAfterPseudoElementType]);
  ASSERT_TRUE(before_matching_rule);
  ASSERT_EQ(1, before_matching_rule->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0),
            (*before_matching_rule)[0].first);
}

// div:before shouldn't match <span/>.
TEST_F(RuleMatchingTest, BeforePseudoElementSelectorNoMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div:before {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<span/>");

  MatchRules(root_->first_element_child());
  EXPECT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kAfterPseudoElementType]);
  ASSERT_FALSE(pseudo_element_matching_rules_[kBeforePseudoElementType]);
}

// .my-class should match <div class="my-class"/>.
TEST_F(RuleMatchingTest, ClassSelectorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      ".my-class {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div class=\"my-class\"/>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), (*matching_rules_)[0].first);
}

// :empty should match <div></div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      ":empty {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div></div>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), (*matching_rules_)[0].first);
}

// :empty should match <div><!--comment--></div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassMatchCommentOnly) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      ":empty {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div><!--comment--></div>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), (*matching_rules_)[0].first);
}

// :empty shouldn't match <div> </div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassMatchTextOnly) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      ":empty {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div> </div>");

  MatchRules(root_->first_element_child());
  EXPECT_EQ(0, matching_rules_->size());
}

// :empty shouldn't match (the outer) <div><div></div></div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassNotMatchElement) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      ":empty {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div><div></div></div>");

  MatchRules(root_->first_element_child());
  EXPECT_EQ(0, matching_rules_->size());
}

// #div1 should match <div id="div1"/>.
TEST_F(RuleMatchingTest, IdSelectorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "#div1 {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div id=\"div1\"/>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), (*matching_rules_)[0].first);
}

// div should match <div/>.
TEST_F(RuleMatchingTest, TypeSelectorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), (*matching_rules_)[0].first);
}

// div.my-class should match <div class="my-class"/>.
TEST_F(RuleMatchingTest, CompoundSelectorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div.my-class {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div class=\"my-class\"/>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), (*matching_rules_)[0].first);
}

// div.my-class shouldn't match <div/> or <span class="my-class"/>.
TEST_F(RuleMatchingTest, CompoundSelectorNoMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div.my-class {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/><span class=\"my-class\"/>");

  MatchRules(root_->first_element_child());
  EXPECT_EQ(0, matching_rules_->size());
  MatchRules(root_->last_element_child());
  EXPECT_EQ(0, matching_rules_->size());
}

// "div span" should match inner span in <div><span><span></span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorDescendantCombinatorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div><span><span></span></span></div>");

  MatchRules(root_->first_element_child()
                 ->first_element_child()
                 ->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), (*matching_rules_)[0].first);
}

// "span span" shouldn't match span in <div><span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorDescendantCombinatorNoMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "span span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div><span></span></div>");

  MatchRules(root_->first_element_child()->first_element_child());
  ASSERT_EQ(0, matching_rules_->size());
}

// "div > span" should match span in <div><span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorChildCombinatorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div > span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div><span></span></div>");

  MatchRules(root_->first_element_child()->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), (*matching_rules_)[0].first);
}

// "div > span" shouldn't match inner span in
// <div><span><span></span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorChildCombinatorNoMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div > span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div><span><span></span></span></div>");

  MatchRules(root_->first_element_child()
                 ->first_element_child()
                 ->first_element_child());
  ASSERT_EQ(0, matching_rules_->size());
}

// "span + span" should match second span in <span/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorNextSiblingCombinatorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "span + span {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<span/><span/>");

  MatchRules(root_->last_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), (*matching_rules_)[0].first);
}

// "span + span" shouldm't match first span in <span/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorNextSiblingCombinatorNoMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "span + span {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<span/><span/>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(0, matching_rules_->size());
}

// "div ~ span" should match second span in <div/><span/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorFollowingSiblingCombinatorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div ~ span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/><span/><span/>");

  MatchRules(root_->last_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), (*matching_rules_)[0].first);
}

// "span ~ span" shouldn't match span in <div/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorFollowingSiblingCombinatorNoMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "span ~ span {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/><span/>");

  MatchRules(root_->last_element_child());
  ASSERT_EQ(0, matching_rules_->size());
}

TEST_F(RuleMatchingTest, SelectorListMatchUseHighestSpecificity) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      ".first-class, #my-id, .first-class.second-class {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html(
      "<div class=\"first-class second-class\" id=\"my-id\"/>");
  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), (*matching_rules_)[0].first);
  // Specificity of the second selector (1, 0, 0) should be used since it's the
  // highest of all matching selectors.
  EXPECT_EQ(cssom::CascadePriority(
                cssom::kNormalAuthor, cssom::Specificity(1, 0, 0),
                cssom::Appearance(cssom::Appearance::kUnattached, 0)),
            (*matching_rules_)[0].second);
}

// A complex example using several combinators.
TEST_F(RuleMatchingTest, ComplexSelectorCombinedMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div ~ span + div ~ div + div > div + div {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html(
      "<div/><span/><span/><div/><span/><div/>"
      "<div><div/><div/></div>");

  MatchRules(root_->last_element_child()->last_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), (*matching_rules_)[0].first);
}

}  // namespace dom
}  // namespace cobalt

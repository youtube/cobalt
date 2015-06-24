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
#include "cobalt/cssom/adjacent_selector.h"
#include "cobalt/cssom/child_combinator.h"
#include "cobalt/cssom/class_selector.h"
#include "cobalt/cssom/combinator.h"
#include "cobalt/cssom/complex_selector.h"
#include "cobalt/cssom/compound_selector.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/descendant_combinator.h"
#include "cobalt/cssom/empty_pseudo_class.h"
#include "cobalt/cssom/following_sibling_combinator.h"
#include "cobalt/cssom/id_selector.h"
#include "cobalt/cssom/next_sibling_combinator.h"
#include "cobalt/cssom/selector_visitor.h"
#include "cobalt/cssom/type_selector.h"
#include "cobalt/dom/comment.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/html_div_element.h"
#include "cobalt/dom/html_element_factory.h"
#include "cobalt/dom/html_span_element.h"
#include "cobalt/dom/html_unknown_element.h"
#include "cobalt/dom/text.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class RuleMatchingTest : public ::testing::Test {
 protected:
  RuleMatchingTest()
      : css_parser_(css_parser::Parser::Create()),
        html_element_factory(NULL, NULL, NULL, NULL),
        root(new Element(&html_element_factory)) {}
  ~RuleMatchingTest() OVERRIDE {}

  HTMLElementFactory html_element_factory;
  scoped_refptr<Element> root;
  scoped_ptr<css_parser::Parser> css_parser_;
  scoped_refptr<cssom::CSSStyleSheet> style_sheet_;
  cssom::RulesWithCascadePriority matching_rules_;
};

// .my-class should match <div class="my-class"/>
TEST_F(RuleMatchingTest, ClassSelectorMatch) {
  // Generate rules in the style sheet from the following CSS code.
  style_sheet_ = css_parser_->ParseStyleSheet(
      ".my-class {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));

  // Generate DOM elements from the following HTML code.
  root->set_inner_html("<div class=\"my-class\"/>");

  // Get the matching rules and check the result.
  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->first_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(1, matching_rules_.size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), matching_rules_[0].first);
}

// :empty should match <div></div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      ":empty {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div></div>");

  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->first_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(1, matching_rules_.size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), matching_rules_[0].first);
}

// :empty should match <div><!--comment--></div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassMatchCommentOnly) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      ":empty {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div><!--comment--></div>");

  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->first_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(1, matching_rules_.size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), matching_rules_[0].first);
}

// :empty shouldn't match <div> </div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassMatchTextOnly) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      ":empty {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div> </div>");

  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->first_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  EXPECT_EQ(0, matching_rules_.size());
}

// :empty shouldn't match (the outer) <div><div></div></div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassNotMatchElement) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      ":empty {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div><div></div></div>");

  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->first_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  EXPECT_EQ(0, matching_rules_.size());
}

// #div1 should match <div id="div1"/>.
TEST_F(RuleMatchingTest, IdSelectorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "#div1 {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div id=\"div1\"/>");

  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->first_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(1, matching_rules_.size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), matching_rules_[0].first);
}

// div should match <div/>.
TEST_F(RuleMatchingTest, TypeSelectorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div/>");

  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->first_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(1, matching_rules_.size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), matching_rules_[0].first);
}

// div.my-class should match <div class="my-class"/>.
TEST_F(RuleMatchingTest, CompoundSelectorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div.my-class {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div class=\"my-class\"/>");

  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->first_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(1, matching_rules_.size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), matching_rules_[0].first);
}

// div.my-class shouldn't match <div/> or <span class="my-class"/>.
TEST_F(RuleMatchingTest, CompoundSelectorNoMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div.my-class {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div/><span class=\"my-class\"/>");

  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->first_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  EXPECT_EQ(0, matching_rules_.size());
  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->last_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  EXPECT_EQ(0, matching_rules_.size());
}

// "div span" should match inner span in <div><span><span></span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorDescendantCombinatorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div><span><span></span></span></div>");

  GetMatchingRulesFromStyleSheet(style_sheet_, root->first_element_child()
                                                   ->first_element_child()
                                                   ->first_element_child()
                                                   ->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(1, matching_rules_.size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), matching_rules_[0].first);
}

// "span span" shouldn't match span in <div><span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorDescendantCombinatorNoMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "span span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div><span></span></div>");

  GetMatchingRulesFromStyleSheet(
      style_sheet_,
      root->first_element_child()->first_element_child()->AsHTMLElement(),
      &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(0, matching_rules_.size());
}

// "div > span" should match span in <div><span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorChildCombinatorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div > span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div><span></span></div>");

  GetMatchingRulesFromStyleSheet(
      style_sheet_,
      root->first_element_child()->first_element_child()->AsHTMLElement(),
      &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(1, matching_rules_.size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), matching_rules_[0].first);
}

// "div > span" shouldn't match inner span in
// <div><span><span></span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorChildCombinatorNoMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div > span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div><span><span></span></span></div>");

  GetMatchingRulesFromStyleSheet(style_sheet_, root->first_element_child()
                                                   ->first_element_child()
                                                   ->first_element_child()
                                                   ->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(0, matching_rules_.size());
}

// "span + span" should match second span in <span/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorNextSiblingCombinatorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "span + span {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<span/><span/>");

  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->last_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(1, matching_rules_.size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), matching_rules_[0].first);
}

// "span + span" shouldm't match first span in <span/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorNextSiblingCombinatorNoMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "span + span {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<span/><span/>");

  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->first_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(0, matching_rules_.size());
}

// "div ~ span" should match second span in <div/><span/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorFollowingSiblingCombinatorMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div ~ span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div/><span/><span/>");

  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->last_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(1, matching_rules_.size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), matching_rules_[0].first);
}

// "span ~ span" shouldn't match span in <div/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorFollowingSiblingCombinatorNoMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "span ~ span {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html("<div/><span/>");

  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->last_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(0, matching_rules_.size());
}

TEST_F(RuleMatchingTest, SelectorListMatchUseHighestSpecificity) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      ".first-class, #my-id, .first-class.second-class {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));

  root->set_inner_html(
      "<div class=\"first-class second-class\" id=\"my-id\"/>");
  GetMatchingRulesFromStyleSheet(style_sheet_,
                                 root->first_element_child()->AsHTMLElement(),
                                 &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(1, matching_rules_.size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), matching_rules_[0].first);
  // Specificity of the second selector (1, 0, 0) should be used since it's the
  // highest of all matching selectors.
  EXPECT_EQ(cssom::CascadePriority(
                cssom::kNormalAuthor, cssom::Specificity(1, 0, 0),
                cssom::Appearance(cssom::Appearance::kUnattached, 0)),
            matching_rules_[0].second);
}

// A complex example using several combinators.
TEST_F(RuleMatchingTest, ComplexSelectorCombinedMatch) {
  style_sheet_ = css_parser_->ParseStyleSheet(
      "div ~ span + div ~ div + div > div + div {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root->set_inner_html(
      "<div/><span/><span/><div/><span/><div/>"
      "<div><div/><div/></div>");

  GetMatchingRulesFromStyleSheet(
      style_sheet_,
      root->last_element_child()->last_element_child()->AsHTMLElement(),
      &matching_rules_, cssom::kNormalAuthor);
  ASSERT_EQ(1, matching_rules_.size());
  EXPECT_EQ(style_sheet_->css_rules()->Item(0), matching_rules_[0].first);
}

}  // namespace dom
}  // namespace cobalt

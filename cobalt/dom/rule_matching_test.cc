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

#include <algorithm>
#include <vector>

#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/node.h"
#include "cobalt/dom/node_list.h"
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
                              NULL, NULL, NULL, NULL, NULL, ""),
        document_(new Document(&html_element_context_)),
        root_(document_->CreateElement("html")->AsHTMLElement()) {
    document_->AppendChild(root_);
  }

  ~RuleMatchingTest() OVERRIDE {}

  void MatchRules(Element* element);

  scoped_ptr<css_parser::Parser> css_parser_;
  scoped_ptr<dom_parser::Parser> dom_parser_;
  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
  scoped_refptr<HTMLElement> root_;

  scoped_refptr<cssom::CSSStyleSheet> css_style_sheet_;
  cssom::RulesWithCascadePriority* matching_rules_;
  cssom::RulesWithCascadePriority*
      pseudo_element_matching_rules_[kMaxPseudoElementType];
};

void RuleMatchingTest::MatchRules(Element* element) {
  HTMLElement* html_element = element->AsHTMLElement();
  DCHECK(html_element);

  document_->style_sheets()->Append(css_style_sheet_);

  document_->UpdateMatchingRules();

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

// div should match <div/>.
TEST_F(RuleMatchingTest, TypeSelectorMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "div {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
}

// .my-class should match <div class="my-class"/>.
TEST_F(RuleMatchingTest, ClassSelectorMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      ".my-class {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div class=\"my-class\"/>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
}

// #div1 should match <div id="div1"/>.
TEST_F(RuleMatchingTest, IdSelectorMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "#div1 {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div id=\"div1\"/>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
}

// :empty should match <div></div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      ":empty {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div></div>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
}

// :empty should match <div><!--comment--></div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassShouldMatchCommentOnly) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      ":empty {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div><!--comment--></div>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
}

// :empty shouldn't match <div> </div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassShouldMatchTextOnly) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      ":empty {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div> </div>");

  MatchRules(root_->first_element_child());
  EXPECT_EQ(0, matching_rules_->size());
}

// div:focus should match focused div.
TEST_F(RuleMatchingTest, FocusPseudoClassMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      ":focus {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div tabIndex=-1/>");
  root_->first_element_child()->AsHTMLElement()->Focus();

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
}

// div:focus shouldn't match unfocused div.
TEST_F(RuleMatchingTest, FocusPseudoClassNoMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      ":focus {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div tabIndex=-1/>");

  MatchRules(root_->first_element_child());
  EXPECT_EQ(0, matching_rules_->size());
}

// :not(.my-class) should match <div/>.
TEST_F(RuleMatchingTest, NotPseudoClassMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      ":not(.my-class) {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
}

// :not(.my-class) shouldn't match <div class="my-class"/>.
TEST_F(RuleMatchingTest, NotPseudoClassNoMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      ":not(.my-class) {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div class=\"my-class\"/>");

  MatchRules(root_->first_element_child());
  EXPECT_EQ(0, matching_rules_->size());
}

// *:after should match <div/> and <span/>.
TEST_F(RuleMatchingTest, DISABLED_AfterPseudoElementMatchGlobal) {
  // Generate rules in the style sheet from the following CSS code.
  css_style_sheet_ = css_parser_->ParseStyleSheet(
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
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*after_matching_rule)[0].first);

  MatchRules(root_->last_element_child());
  after_matching_rule = pseudo_element_matching_rules_[kAfterPseudoElementType];
  ASSERT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kBeforePseudoElementType]);
  ASSERT_TRUE(after_matching_rule);
  ASSERT_EQ(1, after_matching_rule->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*after_matching_rule)[0].first);
}

// div:after should match <div/>.
TEST_F(RuleMatchingTest, AfterPseudoElementSelectorMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "div:after {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/>");

  MatchRules(root_->first_element_child());
  cssom::RulesWithCascadePriority *after_matching_rule =
      pseudo_element_matching_rules_[kAfterPseudoElementType];
  ASSERT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kBeforePseudoElementType]);
  ASSERT_TRUE(after_matching_rule);
  ASSERT_EQ(1, after_matching_rule->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*after_matching_rule)[0].first);
}

// div:after shouldn't match <span/>.
TEST_F(RuleMatchingTest, AfterPseudoElementSelectorNoMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "div:after {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<span/>");

  MatchRules(root_->first_element_child());
  EXPECT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kAfterPseudoElementType]);
  ASSERT_FALSE(pseudo_element_matching_rules_[kBeforePseudoElementType]);
}

// :before should match <div/> and <span/>.
TEST_F(RuleMatchingTest, DISABLED_BeforePseudoElementMatchGlobal) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      ":before {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/><span/>");

  MatchRules(root_->first_element_child());
  cssom::RulesWithCascadePriority *before_matching_rule =
      pseudo_element_matching_rules_[kBeforePseudoElementType];
  ASSERT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kAfterPseudoElementType]);
  ASSERT_TRUE(before_matching_rule);
  ASSERT_EQ(1, before_matching_rule->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*before_matching_rule)[0].first);

  MatchRules(root_->last_element_child());
  before_matching_rule =
      pseudo_element_matching_rules_[kBeforePseudoElementType];
  ASSERT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kAfterPseudoElementType]);
  ASSERT_TRUE(before_matching_rule);
  ASSERT_EQ(1, before_matching_rule->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*before_matching_rule)[0].first);
}

// div:before should match <div/>.
TEST_F(RuleMatchingTest, BeforePseudoElementSelectorMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "div:before {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/>");

  MatchRules(root_->first_element_child());
  cssom::RulesWithCascadePriority *before_matching_rule =
      pseudo_element_matching_rules_[kBeforePseudoElementType];
  ASSERT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kAfterPseudoElementType]);
  ASSERT_TRUE(before_matching_rule);
  ASSERT_EQ(1, before_matching_rule->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*before_matching_rule)[0].first);
}

// div:before shouldn't match <span/>.
TEST_F(RuleMatchingTest, BeforePseudoElementSelectorNoMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "div:before {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<span/>");

  MatchRules(root_->first_element_child());
  EXPECT_EQ(0, matching_rules_->size());
  ASSERT_FALSE(pseudo_element_matching_rules_[kAfterPseudoElementType]);
  ASSERT_FALSE(pseudo_element_matching_rules_[kBeforePseudoElementType]);
}

// :empty shouldn't match (the outer) <div><div></div></div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassNotMatchElement) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      ":empty {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div><div></div></div>");

  MatchRules(root_->first_element_child());
  EXPECT_EQ(0, matching_rules_->size());
}

// div.my-class should match <div class="my-class"/>.
TEST_F(RuleMatchingTest, CompoundSelectorMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "div.my-class {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div class=\"my-class\"/>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
}

// div.my-class shouldn't match <div/> or <span class="my-class"/>.
TEST_F(RuleMatchingTest, CompoundSelectorNoMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
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
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "div span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div><span><span></span></span></div>");

  MatchRules(root_->first_element_child()
                 ->first_element_child()
                 ->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
}

// "span span" shouldn't match span in <div><span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorDescendantCombinatorNoMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "span span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div><span></span></div>");

  MatchRules(root_->first_element_child()->first_element_child());
  ASSERT_EQ(0, matching_rules_->size());
}

// "div > span" should match span in <div><span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorChildCombinatorMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "div > span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div><span></span></div>");

  MatchRules(root_->first_element_child()->first_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
}

// "div > span" shouldn't match inner span in
// <div><span><span></span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorChildCombinatorNoMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "div > span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div><span><span></span></span></div>");

  MatchRules(root_->first_element_child()
                 ->first_element_child()
                 ->first_element_child());
  ASSERT_EQ(0, matching_rules_->size());
}

// "span + span" should match second span in <span/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorNextSiblingCombinatorMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "span + span {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<span/><span/>");
  MatchRules(root_->last_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
}

// "span + span" shouldm't match first span in <span/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorNextSiblingCombinatorNoMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "span + span {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<span/><span/>");

  MatchRules(root_->first_element_child());
  ASSERT_EQ(0, matching_rules_->size());
}

// "div ~ span" should match second span in <div/><span/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorFollowingSiblingCombinatorMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "div ~ span {}", base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/><span/><span/>");

  MatchRules(root_->last_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
}

// "span ~ span" shouldn't match span in <div/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorFollowingSiblingCombinatorNoMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "span ~ span {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html("<div/><span/>");

  MatchRules(root_->last_element_child());
  ASSERT_EQ(0, matching_rules_->size());
}

TEST_F(RuleMatchingTest, SelectorListMatchShouldContainAllMatches) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      ".first-class, #my-id, .first-class.second-class {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html(
      "<div class=\"first-class second-class\" id=\"my-id\"/>");
  MatchRules(root_->first_element_child());
  ASSERT_EQ(3, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[1].first);
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[2].first);

  std::vector<cssom::Specificity> vs;
  vs.push_back((*matching_rules_)[0].second.specificity());
  vs.push_back((*matching_rules_)[1].second.specificity());
  vs.push_back((*matching_rules_)[2].second.specificity());
  sort(vs.begin(), vs.end());
  EXPECT_EQ(vs[0], cssom::Specificity(0, 1, 0));
  EXPECT_EQ(vs[1], cssom::Specificity(0, 2, 0));
  EXPECT_EQ(vs[2], cssom::Specificity(1, 0, 0));
}

// A complex example using several combinators.
TEST_F(RuleMatchingTest, ComplexSelectorCombinedMatch) {
  css_style_sheet_ = css_parser_->ParseStyleSheet(
      "div ~ span + div ~ div + div > div + div {}",
      base::SourceLocation("[object RuleMatchingTest]", 1, 1));
  root_->set_inner_html(
      "<div/><span/><span/><div/><span/><div/>"
      "<div><div/><div/></div>");

  MatchRules(root_->last_element_child()->last_element_child());
  ASSERT_EQ(1, matching_rules_->size());
  EXPECT_EQ(css_style_sheet_->css_rules()->Item(0),
            (*matching_rules_)[0].first);
}

TEST_F(RuleMatchingTest, QuerySelectorShouldReturnFirstMatch) {
  root_->set_inner_html(
      "<div id='div1'>"
      "  <div id='div2'/>"
      "  <div id='div3'/>"
      "</div>");

  scoped_refptr<Element> div1 =
      QuerySelector(document_, "div", css_parser_.get());
  ASSERT_TRUE(div1);
  EXPECT_EQ("div1", div1->id());

  EXPECT_FALSE(QuerySelector(document_, "span", css_parser_.get()));
}

TEST_F(RuleMatchingTest, QuerySelectorShouldLimitResultInSubtree) {
  root_->set_inner_html(
      "<div id='div1'>"
      "  <div id='div2'/>"
      "  <div id='div3'/>"
      "</div>");

  scoped_refptr<Element> div1 =
      QuerySelector(document_, "div", css_parser_.get());
  ASSERT_TRUE(div1);
  EXPECT_EQ("div1", div1->id());

  scoped_refptr<Element> div2 = QuerySelector(div1, "div", css_parser_.get());
  ASSERT_TRUE(div2);
  EXPECT_EQ("div2", div2->id());
}

TEST_F(RuleMatchingTest, QuerySelectorShouldMatchCombinatorOutsideSubtree) {
  root_->set_inner_html(
      "<div class='out'>"
      "  <div id='div1'>"
      "    <span/>"
      "  </div>"
      "</div>");

  scoped_refptr<Element> div1 =
      QuerySelector(document_, "#div1", css_parser_.get());
  ASSERT_TRUE(div1);
  EXPECT_EQ("div1", div1->id());
  scoped_refptr<Element> span =
      QuerySelector(div1, ".out span", css_parser_.get());
  EXPECT_TRUE(span);
}

TEST_F(RuleMatchingTest, QuerySelectorShouldMatchCombinatorsRecursively) {
  root_->set_inner_html(
      "<div/>"
      "<div/>"
      "<p/>"
      "<div/>"
      "<span/>");

  scoped_refptr<Element> span =
      QuerySelector(document_, "div + div ~ span", css_parser_.get());
  EXPECT_TRUE(span);
}

TEST_F(RuleMatchingTest, QuerySelectorShouldMatchCombinatorsCombined) {
  root_->set_inner_html(
      "<div/>"
      "<span/>"
      "<span/>"
      "<div/>"
      "<span/>"
      "<div/>"
      "<div>"
      "  <div/>"
      "  <div/>"
      "</div>");

  scoped_refptr<Element> div = QuerySelector(
      document_, "div ~ span + div ~ div + div > div + div", css_parser_.get());
  EXPECT_TRUE(div);
}

TEST_F(RuleMatchingTest, QuerySelectorAllShouldReturnAllMatches) {
  root_->set_inner_html(
      "<div id='div1'>"
      "  <div id='div2'/>"
      "  <div id='div3'/>"
      "</div>");

  scoped_refptr<NodeList> node_list;
  node_list = QuerySelectorAll(document_, "div", css_parser_.get());
  ASSERT_EQ(3, node_list->length());
  EXPECT_EQ("div1", node_list->Item(0)->AsElement()->id());
  EXPECT_EQ("div2", node_list->Item(1)->AsElement()->id());
  EXPECT_EQ("div3", node_list->Item(2)->AsElement()->id());

  node_list = QuerySelectorAll(document_, "span", css_parser_.get());
  EXPECT_EQ(0, node_list->length());
}

}  // namespace dom
}  // namespace cobalt

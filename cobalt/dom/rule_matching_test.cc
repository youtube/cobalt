// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/rule_matching.h"

#include <algorithm>
#include <vector>

#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/node.h"
#include "cobalt/dom/node_descendants_iterator.h"
#include "cobalt/dom/node_list.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/dom_parser/parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class RuleMatchingTest : public ::testing::Test {
 protected:
  RuleMatchingTest()
      : css_parser_(css_parser::Parser::Create()),
        dom_parser_(new dom_parser::Parser()),
        dom_stat_tracker_(new DomStatTracker("RuleMatchingTest")),
        html_element_context_(NULL, css_parser_.get(), dom_parser_.get(), NULL,
                              NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                              NULL, NULL, dom_stat_tracker_.get(), "",
                              base::kApplicationStateStarted, NULL),
        document_(new Document(&html_element_context_)),
        root_(document_->CreateElement("html")->AsHTMLElement()),
        head_(document_->CreateElement("head")->AsHTMLElement()),
        body_(document_->CreateElement("body")->AsHTMLElement()) {
    root_->AppendChild(head_);
    root_->AppendChild(body_);
    document_->AppendChild(root_);
  }

  ~RuleMatchingTest() override {}

  void UpdateAllMatchingRules();

  scoped_refptr<cssom::CSSStyleSheet> GetDocumentStyleSheet(
      unsigned int index) {
    return document_->style_sheets()->Item(index)->AsCSSStyleSheet();
  }

  scoped_ptr<css_parser::Parser> css_parser_;
  scoped_ptr<dom_parser::Parser> dom_parser_;
  scoped_ptr<DomStatTracker> dom_stat_tracker_;
  HTMLElementContext html_element_context_;

  scoped_refptr<Document> document_;
  scoped_refptr<HTMLElement> root_;
  scoped_refptr<HTMLElement> head_;
  scoped_refptr<HTMLElement> body_;
};

void RuleMatchingTest::UpdateAllMatchingRules() {
  document_->UpdateSelectorTree();
  NodeDescendantsIterator iterator(document_);
  Node* child = iterator.First();
  while (child) {
    if (child->AsElement()) {
      DCHECK(child->AsElement()->AsHTMLElement());
      UpdateMatchingRules(child->AsElement()->AsHTMLElement());
    }
    child = iterator.Next();
  }
}

// For each test, we go through the following process:
// 1. Generate rules in the style sheet from the CSS code.
// 2. Generate DOM elements from the HTML code.
// 3. Get the matching rules and check the result.

// * should match <div/>.
TEST_F(RuleMatchingTest, UniversalSelectorMatch) {
  head_->set_inner_html("<style>* {}</style>");
  body_->set_inner_html("<div/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// div should match <div/>.
TEST_F(RuleMatchingTest, TypeSelectorMatch) {
  head_->set_inner_html("<style>div {}</style>");
  body_->set_inner_html("<div/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// [attr] should match <div attr/>.
TEST_F(RuleMatchingTest, AttributeSelectorMatchNoValue) {
  head_->set_inner_html("<style>[attr] {}</style>");
  body_->set_inner_html("<div attr/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// [attr=value] should match <div attr="value"/>.
TEST_F(RuleMatchingTest, AttributeSelectorMatchEquals) {
  head_->set_inner_html("<style>[attr=value] {}</style>");
  body_->set_inner_html("<div attr=\"value\"/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// [attr="value"] should match <div attr="value"/>.
TEST_F(RuleMatchingTest, AttributeSelectorMatchEqualsWithQuote) {
  head_->set_inner_html("<style>[attr=\"value\"] {}</style>");
  body_->set_inner_html("<div attr=\"value\"/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// [attr=value] should not match <div attr/>.
TEST_F(RuleMatchingTest, AttributeSelectorNoMatchEquals) {
  head_->set_inner_html("<style>[attr=value] {}</style>");
  body_->set_inner_html("<div attr/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(0, matching_rules->size());
}

// .my-class should match <div class="my-class"/>.
TEST_F(RuleMatchingTest, ClassSelectorMatch) {
  head_->set_inner_html("<style>.my-class {}</style>");
  body_->set_inner_html("<div class=\"my-class\"/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// #div1 should match <div id="div1"/>.
TEST_F(RuleMatchingTest, IdSelectorMatch) {
  head_->set_inner_html("<style>#div1 {}</style>");
  body_->set_inner_html("<div id=\"div1\"/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// :empty should match <div></div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassMatch) {
  head_->set_inner_html("<style>:empty {}</style>");
  body_->set_inner_html("<div></div>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// :empty should match <div><!--comment--></div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassShouldMatchCommentOnly) {
  head_->set_inner_html("<style>:empty {}</style>");
  body_->set_inner_html("<div><!--comment--></div>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// :empty shouldn't match <div> </div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassShouldMatchTextOnly) {
  head_->set_inner_html("<style>:empty {}</style>");
  body_->set_inner_html("<div> </div>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
}

// div:focus should match focused div.
TEST_F(RuleMatchingTest, FocusPseudoClassMatch) {
  // Give the document browsing context which is needed for focus to work.
  testing::StubWindow window;
  document_->set_window(window.window());
  // Give the document initial computed style.
  document_->SetViewport(math::Size(320, 240));

  head_->set_inner_html("<style>:focus {}</style>");
  body_->set_inner_html("<div tabIndex=\"-1\"/>");
  body_->first_element_child()->AsHTMLElement()->Focus();
  // This is required because RunFocusingSteps() earlies out as a result of the
  // document not having a browsing context.
  root_->InvalidateMatchingRulesRecursively();
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// div:focus shouldn't match unfocused div.
TEST_F(RuleMatchingTest, FocusPseudoClassNoMatch) {
  // Give the document browsing context which is needed for focus to work.
  testing::StubWindow window;
  document_->set_window(window.window());
  // Give the document initial computed style.
  document_->SetViewport(math::Size(320, 240));

  head_->set_inner_html("<style>:focus {}</style>");
  body_->set_inner_html("<div tabIndex=\"-1\"/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
}

// :not(.my-class) should match <div/>.
TEST_F(RuleMatchingTest, NotPseudoClassMatch) {
  head_->set_inner_html("<style>:not(.my-class) {}</style>");
  body_->set_inner_html("<div/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// :not(.my-class) shouldn't match <div class="my-class"/>.
TEST_F(RuleMatchingTest, NotPseudoClassNoMatch) {
  head_->set_inner_html("<style>:not(.my-class) {}</style>");
  body_->set_inner_html("<div class=\"my-class\"/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
}

// Neither :not(.my-class) and div:not(.my-class) should match
// <div class=\"my-class\"/>.
TEST_F(RuleMatchingTest, TwoNotPseudoClassForSameElementNeitherMatch) {
  head_->set_inner_html(
      "<style>:not(.my-class) {} div:not(.my-class) {}</style>");
  body_->set_inner_html("<div class=\"my-class\"/>");
  UpdateAllMatchingRules();

  ASSERT_EQ(2, GetDocumentStyleSheet(0)->css_rules_same_origin()->length());
  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(0, matching_rules->size());
}

// div:not(.other-class) should match <div class=\"my-class\"/>.
TEST_F(RuleMatchingTest, TwoNotPseudoClassForSameElementFirstMatch) {
  head_->set_inner_html(
      "<style>div:not(.other-class) {} div:not(.my-class) {}</style>");
  body_->set_inner_html("<div class=\"my-class\"/>");
  UpdateAllMatchingRules();

  ASSERT_EQ(2, GetDocumentStyleSheet(0)->css_rules_same_origin()->length());
  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// div:not(.other-class) should match <div class=\"my-class\"/>.
TEST_F(RuleMatchingTest, TwoNotPseudoClassForSameElementSecondMatch) {
  head_->set_inner_html(
      "<style>div:not(.my-class) {} div:not(.other-class) {}</style>");
  body_->set_inner_html("<div class=\"my-class\"/>");
  UpdateAllMatchingRules();

  ASSERT_EQ(2, GetDocumentStyleSheet(0)->css_rules_same_origin()->length());
  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(1),
            (*matching_rules)[0].first);
}

// div:not(.my-class) and div:not(.other-class) should both match
// <div class=\"neither-class\"/>.
TEST_F(RuleMatchingTest, TwoNotPseudoClassForSameElementBothMatch) {
  head_->set_inner_html(
      "<style>div:not(.my-class) {} div:not(.other-class) {}</style>");
  body_->set_inner_html("<div class=\"neither-class\"/>");
  UpdateAllMatchingRules();

  ASSERT_EQ(2, GetDocumentStyleSheet(0)->css_rules_same_origin()->length());
  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(2, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(1),
            (*matching_rules)[1].first);
}

// *:after should create and match the after pseudo element of all elements.
TEST_F(RuleMatchingTest, AfterPseudoElementMatchGlobal) {
  head_->set_inner_html("<style>*:after {}</style>");
  body_->set_inner_html("<div class=\"a\"/><span class=\"a\"/>");
  UpdateAllMatchingRules();

  HTMLElement* html_element = body_->first_element_child()->AsHTMLElement();
  cssom::RulesWithCascadePrecedence* matching_rules =
      html_element->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
  ASSERT_TRUE(html_element->pseudo_element(kAfterPseudoElementType));
  matching_rules =
      html_element->pseudo_element(kAfterPseudoElementType)->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);

  html_element = body_->last_element_child()->AsHTMLElement();
  matching_rules = html_element->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
  ASSERT_TRUE(html_element->pseudo_element(kAfterPseudoElementType));
  matching_rules =
      html_element->pseudo_element(kAfterPseudoElementType)->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// div:after should create and match the after pseudo element of <div/>.
TEST_F(RuleMatchingTest, AfterPseudoElementSelectorMatch) {
  head_->set_inner_html("<style>div:after {}</style>");
  body_->set_inner_html("<div/>");
  UpdateAllMatchingRules();

  HTMLElement* html_element = body_->first_element_child()->AsHTMLElement();
  cssom::RulesWithCascadePrecedence* matching_rules =
      html_element->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
  ASSERT_TRUE(html_element->pseudo_element(kAfterPseudoElementType));
  matching_rules =
      html_element->pseudo_element(kAfterPseudoElementType)->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// div:after shouldn't create the after pseudo element of <span/>.
TEST_F(RuleMatchingTest, AfterPseudoElementSelectorNoMatch) {
  head_->set_inner_html("<style>div:after {}</style>");
  body_->set_inner_html("<span/>");
  UpdateAllMatchingRules();

  HTMLElement* html_element = body_->first_element_child()->AsHTMLElement();
  cssom::RulesWithCascadePrecedence* matching_rules =
      html_element->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
  EXPECT_FALSE(html_element->pseudo_element(kAfterPseudoElementType));
}

// *:before should create and match the before pseudo element of all elements.
TEST_F(RuleMatchingTest, BeforePseudoElementMatchGlobal) {
  head_->set_inner_html("<style>*:before {}</style>");
  body_->set_inner_html("<div class=\"a\"/><span class=\"a\"/>");
  UpdateAllMatchingRules();

  HTMLElement* html_element = body_->first_element_child()->AsHTMLElement();
  cssom::RulesWithCascadePrecedence* matching_rules =
      html_element->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
  ASSERT_TRUE(html_element->pseudo_element(kBeforePseudoElementType));
  matching_rules =
      html_element->pseudo_element(kBeforePseudoElementType)->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);

  html_element = body_->last_element_child()->AsHTMLElement();
  matching_rules = html_element->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
  ASSERT_TRUE(html_element->pseudo_element(kBeforePseudoElementType));
  matching_rules =
      html_element->pseudo_element(kBeforePseudoElementType)->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// div:before should create and match the before pseudo element of <div/>.
TEST_F(RuleMatchingTest, BeforePseudoElementSelectorMatch) {
  head_->set_inner_html("<style>div:before {}</style>");
  body_->set_inner_html("<div/>");
  UpdateAllMatchingRules();

  HTMLElement* html_element = body_->first_element_child()->AsHTMLElement();
  cssom::RulesWithCascadePrecedence* matching_rules =
      html_element->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
  ASSERT_TRUE(html_element->pseudo_element(kBeforePseudoElementType));
  matching_rules =
      html_element->pseudo_element(kBeforePseudoElementType)->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// div:before shouldn't create the before pseudo element of <span/>.
TEST_F(RuleMatchingTest, BeforePseudoElementSelectorNoMatch) {
  head_->set_inner_html("<style>div:before {}</style>");
  body_->set_inner_html("<span/>");
  UpdateAllMatchingRules();

  HTMLElement* html_element = body_->first_element_child()->AsHTMLElement();
  cssom::RulesWithCascadePrecedence* matching_rules =
      html_element->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
  EXPECT_FALSE(html_element->pseudo_element(kBeforePseudoElementType));
}

// :empty shouldn't match (the outer) <div><div></div></div>.
TEST_F(RuleMatchingTest, EmptyPseudoClassNotMatchElement) {
  head_->set_inner_html("<style>:empty {}</style>");
  body_->set_inner_html("<div><div></div></div>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
}

// div.my-class should match <div class="my-class"/>.
TEST_F(RuleMatchingTest, CompoundSelectorMatch) {
  head_->set_inner_html("<style>div.my-class {}</style>");
  body_->set_inner_html("<div class=\"my-class\"/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// div.my-class shouldn't match <div/> or <span class="my-class"/>.
TEST_F(RuleMatchingTest, CompoundSelectorNoMatch) {
  head_->set_inner_html("<style>div.my-class {}</style>");
  body_->set_inner_html("<div/><span class=\"my-class\"/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
  matching_rules =
      body_->last_element_child()->AsHTMLElement()->matching_rules();
  EXPECT_EQ(0, matching_rules->size());
}

// "div span" should match inner span in <div><span><span></span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorDescendantCombinatorMatch) {
  head_->set_inner_html("<style>div span {}</style>");
  body_->set_inner_html("<div><span><span></span></span></div>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()
          ->first_element_child()
          ->first_element_child()
          ->AsHTMLElement()
          ->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// "span span" shouldn't match span in <div><span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorDescendantCombinatorNoMatch) {
  head_->set_inner_html("<style>span span {}</style>");
  body_->set_inner_html("<div><span></span></div>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()
          ->first_element_child()
          ->AsHTMLElement()
          ->matching_rules();
  ASSERT_EQ(0, matching_rules->size());
}

// "div > span" should match span in <div><span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorChildCombinatorMatch) {
  head_->set_inner_html("<style>div > span {}</style>");
  body_->set_inner_html("<div><span></span></div>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()
          ->first_element_child()
          ->AsHTMLElement()
          ->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// "div > span" shouldn't match inner span in
// <div><span><span></span></span></div>.
TEST_F(RuleMatchingTest, ComplexSelectorChildCombinatorNoMatch) {
  head_->set_inner_html("<style>div > span {}</style>");
  body_->set_inner_html("<div><span><span></span></span></div>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()
          ->first_element_child()
          ->first_element_child()
          ->AsHTMLElement()
          ->matching_rules();
  ASSERT_EQ(0, matching_rules->size());
}

// "span + span" should match second span in <span/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorNextSiblingCombinatorMatch) {
  head_->set_inner_html("<style>span + span {}</style>");
  body_->set_inner_html("<span/><span/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->last_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// "span + span" shouldm't match first span in <span/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorNextSiblingCombinatorNoMatch) {
  head_->set_inner_html("<style>span + span {}</style>");
  body_->set_inner_html("<span/><span/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(0, matching_rules->size());
}

// "div ~ span" should match second span in <div/><span/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorFollowingSiblingCombinatorMatch) {
  head_->set_inner_html("<style>div ~ span {}</style>");
  body_->set_inner_html("<div/><span/><span/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->last_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

// "span ~ span" shouldn't match span in <div/><span/>.
TEST_F(RuleMatchingTest, ComplexSelectorFollowingSiblingCombinatorNoMatch) {
  head_->set_inner_html("<style>span ~ span {}</style>");
  body_->set_inner_html("<div/><span/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->last_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(0, matching_rules->size());
}

TEST_F(RuleMatchingTest, SelectorListMatchShouldContainAllMatches) {
  head_->set_inner_html(
      "<style>.first-class, #my-id, .first-class.second-class {}</style>");
  body_->set_inner_html(
      "<div class=\"first-class second-class\" id=\"my-id\"/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(3, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[1].first);
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[2].first);

  std::vector<cssom::Specificity> vs;
  vs.push_back((*matching_rules)[0].second.specificity());
  vs.push_back((*matching_rules)[1].second.specificity());
  vs.push_back((*matching_rules)[2].second.specificity());
  sort(vs.begin(), vs.end());
  EXPECT_EQ(vs[0], cssom::Specificity(0, 1, 0));
  EXPECT_EQ(vs[1], cssom::Specificity(0, 2, 0));
  EXPECT_EQ(vs[2], cssom::Specificity(1, 0, 0));
}

// A complex example using several combinators.
TEST_F(RuleMatchingTest, ComplexSelectorCombinedMatch) {
  head_->set_inner_html(
      "<style>div ~ span + div ~ div + div > div + div {}</style>");
  body_->set_inner_html(
      "<div/><span/><span/><div/><span/><div/>"
      "<div><div/><div/></div>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->last_element_child()
          ->last_element_child()
          ->AsHTMLElement()
          ->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

TEST_F(RuleMatchingTest, QuerySelectorShouldReturnFirstMatch) {
  body_->set_inner_html(
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
  body_->set_inner_html(
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
  body_->set_inner_html(
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
  body_->set_inner_html(
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
  body_->set_inner_html(
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
  body_->set_inner_html(
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

TEST_F(RuleMatchingTest, StyleElementRemoval) {
  head_->set_inner_html("<style>* {}</style>");
  body_->set_inner_html("<div/>");
  UpdateAllMatchingRules();
  head_->set_inner_html("<style/>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(0, matching_rules->size());
}

TEST_F(RuleMatchingTest, StyleElementAddition) {
  head_->set_inner_html("<style/>");
  body_->set_inner_html("<div/>");
  UpdateAllMatchingRules();
  head_->set_inner_html("<style>* {}</style>");
  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      body_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
}

TEST_F(RuleMatchingTest, StyleElementReorderingOneMatching) {
  scoped_refptr<HTMLElement> div1 =
      document_->CreateElement("div")->AsHTMLElement();
  div1->set_inner_html("<style/>");

  scoped_refptr<HTMLElement> div2 =
      document_->CreateElement("div")->AsHTMLElement();
  div2->set_inner_html("<style>* {}</style>");

  body_->set_inner_html("<div/>");
  head_->AppendChild(div1);
  head_->AppendChild(div2);

  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      head_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_NE(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
  EXPECT_EQ(GetDocumentStyleSheet(1)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);

  head_->RemoveChild(div2);
  head_->InsertBefore(div2, div1);

  UpdateAllMatchingRules();

  matching_rules =
      head_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(1, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
  EXPECT_NE(GetDocumentStyleSheet(1)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

TEST_F(RuleMatchingTest, StyleElementReorderingTwoMatching) {
  scoped_refptr<HTMLElement> div1 =
      document_->CreateElement("div")->AsHTMLElement();
  div1->set_inner_html("<style>* {}</style>");

  scoped_refptr<HTMLElement> div2 =
      document_->CreateElement("div")->AsHTMLElement();
  div2->set_inner_html("<style>* {}</style>");

  body_->set_inner_html("<div/>");
  head_->AppendChild(div1);
  head_->AppendChild(div2);

  UpdateAllMatchingRules();

  cssom::RulesWithCascadePrecedence* matching_rules =
      head_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(2, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
  EXPECT_NE(GetDocumentStyleSheet(1)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);

  head_->RemoveChild(div2);
  head_->InsertBefore(div2, div1);

  UpdateAllMatchingRules();

  matching_rules =
      head_->first_element_child()->AsHTMLElement()->matching_rules();
  ASSERT_EQ(2, matching_rules->size());
  EXPECT_EQ(GetDocumentStyleSheet(0)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
  EXPECT_NE(GetDocumentStyleSheet(1)->css_rules_same_origin()->Item(0),
            (*matching_rules)[0].first);
}

}  // namespace dom
}  // namespace cobalt

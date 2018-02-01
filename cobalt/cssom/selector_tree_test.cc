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

#include "cobalt/cssom/selector_tree.h"

#include "cobalt/base/version_compatibility.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/specificity.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(SelectorTreeTest, RootShouldHaveNoChildrenAfterInitialization) {
  SelectorTree selector_tree;
  EXPECT_TRUE(
      selector_tree.children(selector_tree.root(), kChildCombinator).empty());
  EXPECT_TRUE(
      selector_tree.children(selector_tree.root(), kDescendantCombinator)
          .empty());
  EXPECT_TRUE(
      selector_tree.children(selector_tree.root(), kNextSiblingCombinator)
          .empty());
  EXPECT_TRUE(
      selector_tree.children(selector_tree.root(), kFollowingSiblingCombinator)
          .empty());
}

TEST(SelectorTreeTest, AppendRuleShouldTakeOneRule) {
  SelectorTree selector_tree;

  // Selector Tree:
  // root
  //   kDescendantCombinator -> node_1("div")
  scoped_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSStyleRule> css_style_rule_1 =
      css_parser->ParseRule("div {}", base::SourceLocation(
                                          "[object SelectorTreeTest]", 1, 1))
          ->AsCSSStyleRule();
  selector_tree.AppendRule(css_style_rule_1);

  // Verify that ValidateVersionCompatibility does not report a usage error
  // when the minimum compatibility version is 1.
  base::VersionCompatibility::GetInstance()->SetMinimumVersion(1);
  EXPECT_TRUE(selector_tree.ValidateVersionCompatibility());

  ASSERT_EQ(
      0, selector_tree.children(selector_tree.root(), kChildCombinator).size());
  ASSERT_EQ(1,
            selector_tree.children(selector_tree.root(), kDescendantCombinator)
                .size());
  ASSERT_EQ(0,
            selector_tree.children(selector_tree.root(), kNextSiblingCombinator)
                .size());
  ASSERT_EQ(0, selector_tree.children(selector_tree.root(),
                                      kFollowingSiblingCombinator)
                   .size());

  const SelectorTree::Node* node_1 =
      selector_tree.children(selector_tree.root(), kDescendantCombinator)
          .begin()
          ->second;
  ASSERT_EQ(1, node_1->rules().size());
  EXPECT_EQ(css_style_rule_1, node_1->rules()[0]);
  EXPECT_EQ(Specificity(0, 0, 1), node_1->cumulative_specificity());
}

TEST(SelectorTreeTest, AppendRuleShouldNormalizeCompoundSelector) {
  SelectorTree selector_tree;

  // Selector Tree:
  // root
  //   kDescendantCombinator -> node_1(".class#id")
  scoped_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSStyleRule> css_style_rule_1 =
      css_parser->ParseRule(
                    ".class#id {}",
                    base::SourceLocation("[object SelectorTreeTest]", 1, 1))
          ->AsCSSStyleRule();
  scoped_refptr<CSSStyleRule> css_style_rule_2 =
      css_parser->ParseRule(
                    "#id.class {}",
                    base::SourceLocation("[object SelectorTreeTest]", 1, 1))
          ->AsCSSStyleRule();
  selector_tree.AppendRule(css_style_rule_1);
  selector_tree.AppendRule(css_style_rule_2);

  // Verify that ValidateVersionCompatibility does not report a usage error
  // when the minimum compatibility version is 1.
  base::VersionCompatibility::GetInstance()->SetMinimumVersion(1);
  EXPECT_TRUE(selector_tree.ValidateVersionCompatibility());

  ASSERT_EQ(
      0, selector_tree.children(selector_tree.root(), kChildCombinator).size());
  ASSERT_EQ(1,
            selector_tree.children(selector_tree.root(), kDescendantCombinator)
                .size());
  ASSERT_EQ(0,
            selector_tree.children(selector_tree.root(), kNextSiblingCombinator)
                .size());
  ASSERT_EQ(0, selector_tree.children(selector_tree.root(),
                                      kFollowingSiblingCombinator)
                   .size());
}

TEST(SelectorTreeTest, AppendRuleSimpleShouldTakeTwoIdenticalRules) {
  SelectorTree selector_tree;

  // Selector Tree:
  // root
  //   kDescendantCombinator -> node_1("div")
  scoped_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSStyleRule> css_style_rule_1 =
      css_parser->ParseRule("div {}", base::SourceLocation(
                                          "[object SelectorTreeTest]", 1, 1))
          ->AsCSSStyleRule();
  scoped_refptr<CSSStyleRule> css_style_rule_2 =
      css_parser->ParseRule("div {}", base::SourceLocation(
                                          "[object SelectorTreeTest]", 1, 1))
          ->AsCSSStyleRule();
  selector_tree.AppendRule(css_style_rule_1);
  selector_tree.AppendRule(css_style_rule_2);

  // Verify that ValidateVersionCompatibility does not report a usage error
  // when the minimum compatibility version is 1.
  base::VersionCompatibility::GetInstance()->SetMinimumVersion(1);
  EXPECT_TRUE(selector_tree.ValidateVersionCompatibility());

  ASSERT_EQ(
      0, selector_tree.children(selector_tree.root(), kChildCombinator).size());
  ASSERT_EQ(1,
            selector_tree.children(selector_tree.root(), kDescendantCombinator)
                .size());
  ASSERT_EQ(0,
            selector_tree.children(selector_tree.root(), kNextSiblingCombinator)
                .size());
  ASSERT_EQ(0, selector_tree.children(selector_tree.root(),
                                      kFollowingSiblingCombinator)
                   .size());
  const SelectorTree::Node* node_1 =
      selector_tree.children(selector_tree.root(), kDescendantCombinator)
          .begin()
          ->second;
  ASSERT_EQ(2, node_1->rules().size());
  EXPECT_EQ(css_style_rule_1, node_1->rules()[0]);
  EXPECT_EQ(css_style_rule_2, node_1->rules()[1]);
  EXPECT_EQ(Specificity(0, 0, 1), node_1->cumulative_specificity());
}

TEST(SelectorTreeTest, AppendRuleSimpleShouldTakeTwoDesendantSelectors) {
  SelectorTree selector_tree;

  // Selector Tree:
  // root
  //   kNone -> node_1("div")
  //     kChildCombinator -> node_2("span")
  scoped_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSStyleRule> css_style_rule_1 =
      css_parser->ParseRule("div {}", base::SourceLocation(
                                          "[object SelectorTreeTest]", 1, 1))
          ->AsCSSStyleRule();
  scoped_refptr<CSSStyleRule> css_style_rule_2 =
      css_parser->ParseRule(
                    "div span {}",
                    base::SourceLocation("[object SelectorTreeTest]", 1, 1))
          ->AsCSSStyleRule();
  selector_tree.AppendRule(css_style_rule_1);
  selector_tree.AppendRule(css_style_rule_2);

  // Verify that ValidateVersionCompatibility does not report a usage error
  // when the minimum compatibility version is 1.
  base::VersionCompatibility::GetInstance()->SetMinimumVersion(1);
  EXPECT_TRUE(selector_tree.ValidateVersionCompatibility());

  ASSERT_EQ(
      0, selector_tree.children(selector_tree.root(), kChildCombinator).size());
  ASSERT_EQ(1,
            selector_tree.children(selector_tree.root(), kDescendantCombinator)
                .size());
  ASSERT_EQ(0,
            selector_tree.children(selector_tree.root(), kNextSiblingCombinator)
                .size());
  ASSERT_EQ(0, selector_tree.children(selector_tree.root(),
                                      kFollowingSiblingCombinator)
                   .size());
  const SelectorTree::Node* node_1 =
      selector_tree.children(selector_tree.root(), kDescendantCombinator)
          .begin()
          ->second;
  ASSERT_EQ(1, node_1->rules().size());
  EXPECT_EQ(css_style_rule_1, node_1->rules()[0]);
  EXPECT_EQ(Specificity(0, 0, 1), node_1->cumulative_specificity());

  ASSERT_EQ(0, selector_tree.children(node_1, kChildCombinator).size());
  ASSERT_EQ(1, selector_tree.children(node_1, kDescendantCombinator).size());
  ASSERT_EQ(0, selector_tree.children(node_1, kNextSiblingCombinator).size());
  ASSERT_EQ(0,
            selector_tree.children(node_1, kFollowingSiblingCombinator).size());
  const SelectorTree::Node* node_2 =
      selector_tree.children(node_1, kDescendantCombinator).begin()->second;
  ASSERT_EQ(1, node_2->rules().size());
  EXPECT_EQ(css_style_rule_2, node_2->rules()[0]);
  EXPECT_EQ(Specificity(0, 0, 2), node_2->cumulative_specificity());
}

TEST(SelectorTreeTest, AppendRuleTwoDifferentNotSelectorsForSameElement) {
  SelectorTree selector_tree;

  // Selector Tree:
  // root
  //   kDescendantCombinator -> node_1("body:not(.class-1)")
  //   kDescendantCombinator -> node_2("body:not(.class-2)")
  scoped_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSStyleRule> css_style_rule_1 =
      css_parser
          ->ParseRule("body:not(.class-1) {}",
                      base::SourceLocation("[object SelectorTreeTest]", 1, 1))
          ->AsCSSStyleRule();
  scoped_refptr<CSSStyleRule> css_style_rule_2 =
      css_parser
          ->ParseRule("body:not(.class-2) {}",
                      base::SourceLocation("[object SelectorTreeTest]", 1, 1))
          ->AsCSSStyleRule();
  selector_tree.AppendRule(css_style_rule_1);
  selector_tree.AppendRule(css_style_rule_2);

  // Verify that ValidateVersionCompatibility does not report a usage error
  // when the minimum compatibility version is 16.
  base::VersionCompatibility::GetInstance()->SetMinimumVersion(16);
  EXPECT_TRUE(selector_tree.ValidateVersionCompatibility());

  // Verify that ValidateVersionCompatibility reports a usage error when the
  // minimum compatibility version is 1.
  base::VersionCompatibility::GetInstance()->SetMinimumVersion(1);
  EXPECT_FALSE(selector_tree.ValidateVersionCompatibility());

  ASSERT_EQ(
      0, selector_tree.children(selector_tree.root(), kChildCombinator).size());
  ASSERT_EQ(2,
            selector_tree.children(selector_tree.root(), kDescendantCombinator)
                .size());
  ASSERT_EQ(0,
            selector_tree.children(selector_tree.root(), kNextSiblingCombinator)
                .size());
  ASSERT_EQ(0, selector_tree
                   .children(selector_tree.root(), kFollowingSiblingCombinator)
                   .size());
  auto node_iter =
      selector_tree.children(selector_tree.root(), kDescendantCombinator)
          .begin();
  const SelectorTree::Node* node_1 = node_iter->second;
  ASSERT_EQ(1, node_1->rules().size());
  EXPECT_EQ(css_style_rule_1, node_1->rules()[0]);
  EXPECT_EQ(Specificity(0, 1, 1), node_1->cumulative_specificity());

  const SelectorTree::Node* node_2 = (++node_iter)->second;
  ASSERT_EQ(1, node_2->rules().size());
  EXPECT_EQ(css_style_rule_2, node_2->rules()[0]);
  EXPECT_EQ(Specificity(0, 1, 1), node_1->cumulative_specificity());
}

TEST(SelectorTreeTest, AppendRuleTwoNotSelectorsForDifferentElements) {
  SelectorTree selector_tree;

  // Selector Tree:
  // root
  //   kDescendantCombinator -> node_1("body:not(.class-1)")
  //   kDescendantCombinator -> node_2("div:not(.class-1)")
  scoped_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSStyleRule> css_style_rule_1 =
      css_parser
          ->ParseRule("body:not(.class-1) {}",
                      base::SourceLocation("[object SelectorTreeTest]", 1, 1))
          ->AsCSSStyleRule();
  scoped_refptr<CSSStyleRule> css_style_rule_2 =
      css_parser
          ->ParseRule("div:not(.class-1) {}",
                      base::SourceLocation("[object SelectorTreeTest]", 1, 1))
          ->AsCSSStyleRule();
  selector_tree.AppendRule(css_style_rule_1);
  selector_tree.AppendRule(css_style_rule_2);

  // Verify that ValidateVersionCompatibility does not report a usage error
  // when the minimum compatibility version is 1.
  base::VersionCompatibility::GetInstance()->SetMinimumVersion(1);
  EXPECT_TRUE(selector_tree.ValidateVersionCompatibility());

  ASSERT_EQ(
      0, selector_tree.children(selector_tree.root(), kChildCombinator).size());
  ASSERT_EQ(2,
            selector_tree.children(selector_tree.root(), kDescendantCombinator)
                .size());
  ASSERT_EQ(0,
            selector_tree.children(selector_tree.root(), kNextSiblingCombinator)
                .size());
  ASSERT_EQ(0, selector_tree
                   .children(selector_tree.root(), kFollowingSiblingCombinator)
                   .size());
  auto node_iter =
      selector_tree.children(selector_tree.root(), kDescendantCombinator)
          .begin();
  const SelectorTree::Node* node_1 = node_iter->second;
  ASSERT_EQ(1, node_1->rules().size());
  EXPECT_EQ(css_style_rule_1, node_1->rules()[0]);
  EXPECT_EQ(Specificity(0, 1, 1), node_1->cumulative_specificity());

  const SelectorTree::Node* node_2 = (++node_iter)->second;
  ASSERT_EQ(1, node_2->rules().size());
  EXPECT_EQ(css_style_rule_2, node_2->rules()[0]);
  EXPECT_EQ(Specificity(0, 1, 1), node_1->cumulative_specificity());
}

TEST(SelectorTreeTest, AppendRuleTwoIdenticalNotSelectors) {
  SelectorTree selector_tree;

  // Selector Tree:
  // root
  //   kDescendantCombinator -> node_1("body:not(.class-1")
  scoped_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  scoped_refptr<CSSStyleRule> css_style_rule_1 =
      css_parser
          ->ParseRule("body:not(.class-1) {}",
                      base::SourceLocation("[object SelectorTreeTest]", 1, 1))
          ->AsCSSStyleRule();
  scoped_refptr<CSSStyleRule> css_style_rule_2 =
      css_parser
          ->ParseRule("body:not(.class-1) {}",
                      base::SourceLocation("[object SelectorTreeTest]", 1, 1))
          ->AsCSSStyleRule();
  selector_tree.AppendRule(css_style_rule_1);
  selector_tree.AppendRule(css_style_rule_2);

  // Verify that ValidateVersionCompatibility does not report a usage error
  // when the minimum compatibility version is 1.
  base::VersionCompatibility::GetInstance()->SetMinimumVersion(1);
  EXPECT_TRUE(selector_tree.ValidateVersionCompatibility());

  ASSERT_EQ(
      0, selector_tree.children(selector_tree.root(), kChildCombinator).size());
  ASSERT_EQ(1,
            selector_tree.children(selector_tree.root(), kDescendantCombinator)
                .size());
  ASSERT_EQ(0,
            selector_tree.children(selector_tree.root(), kNextSiblingCombinator)
                .size());
  ASSERT_EQ(0, selector_tree
                   .children(selector_tree.root(), kFollowingSiblingCombinator)
                   .size());
  const SelectorTree::Node* node_1 =
      selector_tree.children(selector_tree.root(), kDescendantCombinator)
          .begin()
          ->second;
  ASSERT_EQ(2, node_1->rules().size());
  EXPECT_EQ(css_style_rule_1, node_1->rules()[0]);
  EXPECT_EQ(css_style_rule_2, node_1->rules()[1]);
  EXPECT_EQ(Specificity(0, 1, 1), node_1->cumulative_specificity());
}

}  // namespace cssom
}  // namespace cobalt

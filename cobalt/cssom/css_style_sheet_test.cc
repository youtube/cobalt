// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/cssom/css_media_rule.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_rule_style_declaration.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/media_feature.h"
#include "cobalt/cssom/media_feature_keyword_value.h"
#include "cobalt/cssom/media_feature_names.h"
#include "cobalt/cssom/media_list.h"
#include "cobalt/cssom/media_query.h"
#include "cobalt/cssom/selector.h"
#include "cobalt/cssom/style_sheet_list.h"
#include "cobalt/cssom/testing/mock_css_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

using ::testing::_;
using ::testing::Return;

class MockMutationObserver : public MutationObserver {
 public:
  MOCK_METHOD0(OnCSSMutation, void());
};

class CSSStyleSheetTest : public ::testing::Test {
 protected:
  CSSStyleSheetTest() : css_style_sheet_(new CSSStyleSheet(&css_parser_)) {
    css_style_sheet_->SetOriginClean(true);
    StyleSheetVector style_sheets;
    style_sheets.push_back(css_style_sheet_);
    style_sheet_list_ = new StyleSheetList(style_sheets, &mutation_observer_);
  }
  ~CSSStyleSheetTest() override {}

  const scoped_refptr<CSSStyleSheet> css_style_sheet_;
  scoped_refptr<StyleSheetList> style_sheet_list_;
  MockMutationObserver mutation_observer_;
  testing::MockCSSParser css_parser_;
};

TEST_F(CSSStyleSheetTest, InsertRule) {
  const std::string css_text = "div { font-size: 100px; color: #0047ab; }";

  EXPECT_CALL(css_parser_, ParseRule(css_text, _))
      .WillOnce(Return(scoped_refptr<CSSRule>()));
  css_style_sheet_->InsertRuleSameOrigin(css_text, 0);
}

TEST_F(CSSStyleSheetTest, CSSRuleListIsCached) {
  scoped_refptr<CSSRuleList> rule_list_1 =
      css_style_sheet_->css_rules_same_origin();
  scoped_refptr<CSSRuleList> rule_list_2 =
      css_style_sheet_->css_rules_same_origin();
  ASSERT_EQ(rule_list_1, rule_list_2);
}

TEST_F(CSSStyleSheetTest, CSSRuleListIsLive) {
  scoped_refptr<CSSRuleList> rule_list =
      css_style_sheet_->css_rules_same_origin();
  ASSERT_EQ(0, rule_list->length());
  ASSERT_FALSE(rule_list->Item(0).get());

  scoped_refptr<CSSStyleRule> rule =
      new CSSStyleRule(Selectors(), new CSSRuleStyleDeclaration(NULL));

  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);
  rule_list->AppendCSSRule(rule);
  css_style_sheet_->set_css_rules(rule_list);
  ASSERT_EQ(1, rule_list->length());
  ASSERT_EQ(rule, rule_list->Item(0));
  ASSERT_FALSE(rule_list->Item(1).get());
  ASSERT_EQ(rule_list, css_style_sheet_->css_rules_same_origin());
}

TEST_F(CSSStyleSheetTest, CSSMutationIsReportedAtStyleSheetList) {
  // A call to OnCSSMutation on the CSSStyleSheet should result in a call to
  // OnCSSMutation in the MutationObserver registered with the StyleSheetList
  // that is the parent of the CSSStyleSheet.

  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);
  css_style_sheet_->OnCSSMutation();
}

TEST_F(CSSStyleSheetTest, CSSMutationIsRecordedAfterMediaRuleAddition) {
  // When a CSSMediaRule is added to a CSSStyleSheet, it should result in a call
  // to OnCSSMutation() in the MutationObserver after the next call to
  // EvaluateMediaRules(), even when called with the same media parameters as
  // before. That also tests that OnMediaRuleMutation() should be called and
  // that the flag it sets should be honored.
  scoped_refptr<CSSRuleList> rule_list =
      css_style_sheet_->css_rules_same_origin();
  // A CSSMediaRule with no expression always evaluates to true.
  scoped_refptr<CSSMediaRule> rule = new CSSMediaRule();

  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(0);
  css_style_sheet_->EvaluateMediaRules(math::Size(1920, 1080));

  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);
  rule_list->AppendCSSRule(rule);

  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);
  css_style_sheet_->EvaluateMediaRules(math::Size(1920, 1080));
}

TEST_F(CSSStyleSheetTest, CSSMutationIsRecordedForAddingFalseMediaRule) {
  // Adding a CSSMediaRule that is false should result in a call to
  // OnCSSMutation() only when the rule is added, for the added MediaRule
  // itself. It should not call OnCSSMutation when it's evaluated, because no
  // rules are added or removed at that time for a new rule that is false.
  scoped_refptr<MediaQuery> media_query(new MediaQuery(false));
  scoped_refptr<MediaList> media_list(new MediaList());
  media_list->Append(media_query);
  scoped_refptr<CSSMediaRule> rule = new CSSMediaRule(media_list, NULL);
  scoped_refptr<CSSRuleList> rule_list =
      css_style_sheet_->css_rules_same_origin();

  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);
  rule_list->AppendCSSRule(rule);

  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(0);
  css_style_sheet_->EvaluateMediaRules(math::Size(1920, 1080));
}

TEST_F(CSSStyleSheetTest, CSSMutationIsRecordedAfterMediaValueChanges) {
  // Changing a media value (width or height) should result in a call to
  // OnCSSMutation() if the media rule condition value changes.

  // We first need to build a CSSMediaRule that holds a media at-rule that
  // can change value. We choose '(orientation:landscape)'.
  scoped_refptr<MediaFeatureKeywordValue> property =
      MediaFeatureKeywordValue::GetLandscape();
  scoped_refptr<MediaFeature> media_feature(
      new MediaFeature(kOrientationMediaFeature, property));
  media_feature->set_operator(kEquals);
  scoped_ptr<MediaFeatures> media_features(new MediaFeatures);
  media_features->push_back(media_feature);
  scoped_refptr<MediaQuery> media_query(
      new MediaQuery(true, media_features.Pass()));
  scoped_refptr<MediaList> media_list(new MediaList());
  media_list->Append(media_query);
  scoped_refptr<CSSMediaRule> rule = new CSSMediaRule(media_list, NULL);

  // This should result in a call to OnCSSMutation(), because a media rule is
  // added to the style sheet.

  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);
  css_style_sheet_->css_rules_same_origin()->AppendCSSRule(rule);

  // This should result in a call to OnCSSMutation(), because the added media
  // rule evaluates to true, so its rule list needs to be traversed for the next
  // rule matching.

  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);
  css_style_sheet_->EvaluateMediaRules(math::Size(1920, 1080));

  // This should not result in a call to OnCSSMutation(), because changing the
  // width to 1280 does not change the CSSMediaRule condition.

  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(0);
  css_style_sheet_->EvaluateMediaRules(math::Size(1280, 1080));

  // This should result in a call to OnCSSMutation(), because changing the width
  // to 640 makes the CSSMediaRule condition change. The display orientation is
  // now Portrait instead of Landscape.

  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);
  css_style_sheet_->EvaluateMediaRules(math::Size(640, 1080));
}

}  // namespace cssom
}  // namespace cobalt

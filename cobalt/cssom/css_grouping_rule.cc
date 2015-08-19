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

#include "cobalt/cssom/css_grouping_rule.h"

#include "cobalt/cssom/css_rule_list.h"

namespace cobalt {
namespace cssom {

CSSGroupingRule::CSSGroupingRule() : parent_style_sheet_(NULL) {}

CSSGroupingRule::CSSGroupingRule(
    const scoped_refptr<CSSRuleList>& css_rule_list)
    : css_rule_list_(css_rule_list) {}

CSSGroupingRule::CSSGroupingRule(StyleSheet* parent_style_sheet)
    : parent_style_sheet_(parent_style_sheet) {}

void CSSGroupingRule::set_css_rules(
    const scoped_refptr<CSSRuleList>& css_rule_list) {
  css_rule_list_ = css_rule_list;
  if (parent_style_sheet_ && css_rule_list_) {
    css_rule_list_->AttachToStyleSheet(parent_style_sheet_);
  }
}

scoped_refptr<CSSRuleList> CSSGroupingRule::css_rules() {
  if (!css_rule_list_) {
    css_rule_list_ = new CSSRuleList();
  }
  return css_rule_list_;
}

void CSSGroupingRule::AttachToStyleSheet(StyleSheet* style_sheet) {
  parent_style_sheet_ = style_sheet;
  css_rule_list_->AttachToStyleSheet(style_sheet);
}

unsigned int CSSGroupingRule::InsertRule(
    const scoped_refptr<CSSStyleRule>& css_rule, unsigned int index) {
  return css_rule_list_->InsertRule(css_rule, index);
}

CSSGroupingRule::~CSSGroupingRule() {}

}  // namespace cssom
}  // namespace cobalt

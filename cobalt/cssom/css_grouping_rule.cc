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

CSSGroupingRule::CSSGroupingRule() {}

CSSGroupingRule::CSSGroupingRule(
    const scoped_refptr<CSSRuleList>& css_rule_list)
    : css_rule_list_(css_rule_list) {}

void CSSGroupingRule::set_css_rules(
    const scoped_refptr<CSSRuleList>& css_rule_list) {
  DCHECK(css_rule_list);
  if (parent_style_sheet()) {
    css_rule_list->AttachToCSSStyleSheet(parent_style_sheet());
  }
  css_rule_list_ = css_rule_list;
}

scoped_refptr<CSSRuleList> CSSGroupingRule::css_rules() {
  if (!css_rule_list_) {
    set_css_rules(new CSSRuleList());
  }
  return css_rule_list_;
}

unsigned int CSSGroupingRule::InsertRule(const std::string& rule,
                                         unsigned int index) {
  return css_rules()->InsertRule(rule, index);
}

void CSSGroupingRule::AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) {
  set_parent_style_sheet(style_sheet);
  if (css_rule_list_) {
    css_rule_list_->AttachToCSSStyleSheet(style_sheet);
  }
}

CSSGroupingRule::~CSSGroupingRule() {}

}  // namespace cssom
}  // namespace cobalt

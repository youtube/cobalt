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
#include "cobalt/cssom/css_style_sheet.h"

namespace cobalt {
namespace cssom {

CSSGroupingRule::CSSGroupingRule() {}

CSSGroupingRule::CSSGroupingRule(
    const scoped_refptr<CSSRuleList>& css_rule_list)
    : css_rule_list_(css_rule_list) {}

const scoped_refptr<CSSRuleList>& CSSGroupingRule::css_rules() {
  if (!css_rule_list_) {
    set_css_rules(new CSSRuleList());
  }
  return css_rule_list_;
}

unsigned int CSSGroupingRule::InsertRule(const std::string& rule,
                                         unsigned int index) {
  return css_rules()->InsertRule(rule, index);
}

void CSSGroupingRule::set_css_rules(
    const scoped_refptr<CSSRuleList>& css_rule_list) {
  DCHECK(css_rule_list);
  if (css_rule_list == css_rule_list_) {
    return;
  }
  if (parent_style_sheet()) {
    css_rule_list->AttachToCSSStyleSheet(parent_style_sheet());
    bool rules_possibly_added_or_changed_or_removed =
        (css_rule_list->length() > 0) ||
        (css_rule_list_ && css_rule_list_->length() > 0);
    if (rules_possibly_added_or_changed_or_removed) {
      parent_style_sheet()->OnCSSMutation();
    }
  }
  css_rule_list_ = css_rule_list;
}

CSSGroupingRule::~CSSGroupingRule() {}

}  // namespace cssom
}  // namespace cobalt

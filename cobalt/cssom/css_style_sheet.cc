/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/cssom/css_style_sheet.h"

#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/css_rule_list.h"

namespace cobalt {
namespace cssom {

scoped_refptr<CSSStyleSheet> CSSStyleSheet::Create() {
  return make_scoped_refptr(new CSSStyleSheet());
}

scoped_refptr<CSSRuleList> CSSStyleSheet::css_rules() {
  if (css_rule_list_) {
    return css_rule_list_.get();
  }

  scoped_refptr<CSSRuleList> css_rule_list = CSSRuleList::Create(this);
  css_rule_list_ = css_rule_list->AsWeakPtr();
  return css_rule_list;
}

void CSSStyleSheet::AppendRule(const scoped_refptr<CSSRule>& css_rule) {
  css_rules_.push_back(css_rule);
}

CSSStyleSheet::CSSStyleSheet() {}

CSSStyleSheet::~CSSStyleSheet() {}

}  // namespace cssom
}  // namespace cobalt

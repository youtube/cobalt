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

#include "cobalt/cssom/css_keyframes_rule.h"

#include "cobalt/cssom/css_rule_visitor.h"

namespace cobalt {
namespace cssom {

CSSKeyframesRule::CSSKeyframesRule(const std::string& name,
                                   const scoped_refptr<CSSRuleList>& css_rules)
    : name_(name), css_rules_(css_rules) {}

std::string CSSKeyframesRule::css_text() const {
  NOTIMPLEMENTED();
  return "";
}

void CSSKeyframesRule::set_css_text(const std::string& /*css_text*/) {
  NOTIMPLEMENTED();
}

const std::string& CSSKeyframesRule::name() const { return name_; }

const scoped_refptr<CSSRuleList>& CSSKeyframesRule::css_rules() const {
  return css_rules_;
}

void CSSKeyframesRule::Accept(CSSRuleVisitor* visitor) {
  visitor->VisitCSSKeyframesRule(this);
}

void CSSKeyframesRule::AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) {
  set_parent_style_sheet(style_sheet);
  css_rules_->AttachToCSSStyleSheet(style_sheet);
}

CSSKeyframesRule::~CSSKeyframesRule() {}

}  // namespace cssom
}  // namespace cobalt

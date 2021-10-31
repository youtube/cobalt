// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/css_keyframe_rule.h"

#include "cobalt/cssom/css_rule_visitor.h"

namespace cobalt {
namespace cssom {

CSSKeyframeRule::CSSKeyframeRule(
    const std::vector<float>& offsets,
    const scoped_refptr<CSSRuleStyleDeclaration>& style)
    : offsets_(offsets), style_(style) {}

std::string CSSKeyframeRule::css_text(
    script::ExceptionState* exception_state) const {
  NOTIMPLEMENTED();
  return "";
}

void CSSKeyframeRule::set_css_text(const std::string& css_text,
                                   script::ExceptionState* exception_state) {
  NOTIMPLEMENTED();
}

std::string CSSKeyframeRule::key_text() const {
  NOTIMPLEMENTED();
  return "";
}

void CSSKeyframeRule::Accept(CSSRuleVisitor* visitor) {
  visitor->VisitCSSKeyframeRule(this);
}

void CSSKeyframeRule::AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) {
  set_parent_style_sheet(style_sheet);
}

CSSKeyframeRule::~CSSKeyframeRule() {}

}  // namespace cssom
}  // namespace cobalt

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

#include "cobalt/cssom/css_media_rule.h"

#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_rule_visitor.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/media_list.h"
#include "cobalt/cssom/viewport_size.h"

namespace cobalt {
namespace cssom {

CSSMediaRule::CSSMediaRule(const scoped_refptr<MediaList>& media_list,
                           const scoped_refptr<CSSRuleList>& css_rule_list)
    : CSSConditionRule(css_rule_list), media_list_(media_list) {
  DCHECK(media_list_);
}

const scoped_refptr<MediaList>& CSSMediaRule::media() const {
  return media_list_;
}

std::string CSSMediaRule::css_text(
    script::ExceptionState* exception_state) const {
  // TODO: Serialize the media rule to implement css_text.
  //   https://www.w3.org/TR/cssom/#dom-cssrule-csstext
  NOTIMPLEMENTED() << "CSSMediaRule serialization not implemented yet.";
  return "";
}

void CSSMediaRule::set_css_text(const std::string& css_text,
                                script::ExceptionState* exception_state) {
  // TODO: Parse the given text into a new CSSMediaRule.
  if (parent_style_sheet()) {
    parent_style_sheet()->OnMediaRuleMutation();
  }
  NOTIMPLEMENTED() << "CSSMediaRule css_text setting not implemented yet.";
}

std::string CSSMediaRule::condition_text() { return media_list_->media_text(); }

void CSSMediaRule::set_condition_text(const std::string& condition) {
  media_list_->set_media_text(condition);
}

void CSSMediaRule::Accept(CSSRuleVisitor* visitor) {
  visitor->VisitCSSMediaRule(this);
}

bool CSSMediaRule::EvaluateConditionValueAndReturnIfChanged(
    const ViewportSize& viewport_size) {
  return SetConditionValueAndTestIfChanged(
      media_list_->EvaluateConditionValue(viewport_size));
}

void CSSMediaRule::AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) {
  // If the parent style sheet ever gets changed, we should call
  // OnMediaRuleMutation() on the old style sheet here.
  DCHECK(!parent_style_sheet() || parent_style_sheet() == style_sheet);

  set_parent_style_sheet(style_sheet);

  style_sheet->OnMediaRuleMutation();
  css_rules()->AttachToCSSStyleSheet(style_sheet);
}

CSSMediaRule::~CSSMediaRule() {}

}  // namespace cssom
}  // namespace cobalt

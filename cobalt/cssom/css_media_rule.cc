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

#include "cobalt/cssom/css_media_rule.h"

#include "cobalt/cssom/css_grouping_rule.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_rule_visitor.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/media_list.h"

namespace cobalt {
namespace cssom {

CSSMediaRule::CSSMediaRule() {}

CSSMediaRule::CSSMediaRule(const scoped_refptr<MediaList>& media_list,
                           const scoped_refptr<CSSRuleList>& css_rule_list)
    : CSSConditionRule(css_rule_list), media_list_(media_list) {}

scoped_refptr<MediaList> CSSMediaRule::media() { return media_list_; }

std::string CSSMediaRule::css_text() const {
  // TODO(***REMOVED***): Serialize the media rule to implement css_text.
  //   http://www.w3.org/TR/cssom/#dom-cssrule-csstext
  NOTIMPLEMENTED() << "CSSMediaRule serialization not implemented yet.";
  return "";
}

void CSSMediaRule::set_css_text(const std::string& /* css_text */) {
  // TODO(***REMOVED***): Parse the given text into a new CSSMediaRule.
  NOTIMPLEMENTED() << "CSSMediaRule css_text setting not implemented yet.";
}

void CSSMediaRule::Accept(CSSRuleVisitor* visitor) {
  visitor->VisitCSSMediaRule(this);
}

std::string CSSMediaRule::condition_text() {
  return media_list_ ? media_list_->media_text() : "";
}

void CSSMediaRule::set_condition_text(const std::string& condition) {
  if (!media_list_) {
    DCHECK(parent_style_sheet());
    media_list_ = new MediaList(parent_style_sheet()->css_parser());
  }
  media_list_->set_media_text(condition);
}

bool CSSMediaRule::GetCachedConditionValue() {
  // TODO(***REMOVED***): implement returing the result of the media rule expression.
  NOTIMPLEMENTED();
  return false;
}

bool CSSMediaRule::EvaluateConditionValue() {
  // TODO(***REMOVED***): Implement recalculation of the condition.
  NOTIMPLEMENTED();
  return false;
}

CSSMediaRule::~CSSMediaRule() {}

}  // namespace cssom
}  // namespace cobalt

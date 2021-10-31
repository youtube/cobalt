// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/css_keyframes_rule.h"

#include <algorithm>
#include <set>

#include "cobalt/cssom/css_keyframe_rule.h"
#include "cobalt/cssom/css_rule_visitor.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/style_sheet_list.h"

namespace cobalt {
namespace cssom {

CSSKeyframesRule::CSSKeyframesRule(const std::string& name,
                                   const scoped_refptr<CSSRuleList>& css_rules)
    : name_(name), css_rules_(css_rules) {
  UpdateSortedKeyframes();
}

std::string CSSKeyframesRule::css_text(
    script::ExceptionState* exception_state) const {
  NOTIMPLEMENTED();
  return "";
}

void CSSKeyframesRule::set_css_text(const std::string& css_text,
                                    script::ExceptionState* exception_state) {
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

namespace {
class OffsetComparator {
 public:
  bool operator()(const CSSKeyframesRule::KeyframeInfo& lhs,
                  const CSSKeyframesRule::KeyframeInfo& rhs) const {
    return lhs.offset < rhs.offset;
  }
};
}  // namespace

void CSSKeyframesRule::UpdateSortedKeyframes() {
  if (!css_rules_) {
    sorted_keyframes_.clear();
    return;
  }

  // Use a set to construct a list of keyframes sorted by offset, and then
  // when finished finalize the results into sorted_keyframes_ for easy
  // access later.
  std::set<KeyframeInfo, OffsetComparator> sorted_keyframe_set;

  for (unsigned int i = 0; i < css_rules_->length(); ++i) {
    CSSKeyframeRule* keyframe_rule =
        base::polymorphic_downcast<CSSKeyframeRule*>(css_rules_->Item(i).get());

    const std::vector<float>& offsets = keyframe_rule->offsets();
    for (unsigned int j = 0; j < offsets.size(); ++j) {
      KeyframeInfo keyframe;
      keyframe.offset = offsets[j];
      keyframe.style = keyframe_rule->style()->data();
      sorted_keyframe_set.insert(keyframe);
    }
  }

  sorted_keyframes_ = std::vector<KeyframeInfo>(sorted_keyframe_set.begin(),
                                                sorted_keyframe_set.end());
}

}  // namespace cssom
}  // namespace cobalt

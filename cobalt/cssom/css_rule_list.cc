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

#include "cobalt/cssom/css_rule_list.h"

#include <limits>

#include "base/logging.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/css_rule_visitor.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"

namespace cobalt {
namespace cssom {

CSSRuleList::CSSRuleList() : parent_css_style_sheet_(NULL) {}

unsigned int CSSRuleList::length() const {
  CHECK_LE(css_rules_.size(), std::numeric_limits<unsigned int>::max());
  return static_cast<unsigned int>(css_rules_.size());
}

scoped_refptr<CSSRule> CSSRuleList::Item(unsigned int index) const {
  return index < css_rules_.size() ? css_rules_[index] : NULL;
}

unsigned int CSSRuleList::InsertRule(const std::string& rule,
                                     unsigned int index) {
  // TODO: Currently we only support appending rule to the end of the rule
  // list. Properly implement insertion if necessary.
  DCHECK(parent_css_style_sheet_);
  scoped_refptr<CSSRule> css_rule =
      parent_css_style_sheet_->css_parser()->ParseRule(
          rule, GetInlineSourceLocation());

  if (css_rule.get() == NULL) {
    // TODO: Throw a SyntaxError exception instead of logging.
    LOG(ERROR) << "SyntaxError";
    return 0;
  }

  if (index > css_rules_.size()) {
    // TODO: Throw a IndexSizeError exception instead of logging.
    LOG(ERROR) << "IndexSizeError";
    return 0;
  }

  if (index != css_rules_.size()) {
    LOG(WARNING) << "InsertRule will always append the rule to the end of the "
                    "rule list.";
  }

  AppendCSSRule(css_rule);
  return index;
}

void CSSRuleList::AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) {
  DCHECK(style_sheet);
  parent_css_style_sheet_ = style_sheet;
  for (CSSRules::iterator it = css_rules_.begin(); it != css_rules_.end();
       ++it) {
    DCHECK(*it);
    (*it)->AttachToCSSStyleSheet(style_sheet);
  }
}

void CSSRuleList::AppendCSSRule(const scoped_refptr<CSSRule>& css_rule) {
  if (parent_css_style_sheet_) {
    css_rule->AttachToCSSStyleSheet(parent_css_style_sheet_);
    parent_css_style_sheet_->OnCSSMutation();
  }
  css_rule->SetIndex(next_index_);
  next_index_ += css_rule->IndexWidth();
  css_rules_.push_back(css_rule);
}

void CSSRuleList::Accept(CSSRuleVisitor* visitor) {
  for (CSSRules::const_iterator it = css_rules_.begin(); it != css_rules_.end();
       ++it) {
    (*it)->Accept(visitor);
  }
}

CSSRuleList::~CSSRuleList() {}

}  // namespace cssom
}  // namespace cobalt

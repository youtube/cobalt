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

#include "cobalt/cssom/css_rule_list.h"

#include <limits>

#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"

namespace cobalt {
namespace cssom {

CSSRuleList::CSSRuleList(
    const scoped_refptr<const CSSStyleSheet>& css_style_sheet)
    : css_style_sheet_(css_style_sheet) {}

scoped_refptr<CSSStyleRule> CSSRuleList::Item(unsigned int index) const {
  return index < css_style_sheet_->css_rules_.size()
             ? css_style_sheet_->css_rules_[index]
             : NULL;
}

unsigned int CSSRuleList::length() const {
  CHECK_LE(css_style_sheet_->css_rules_.size(),
           std::numeric_limits<unsigned int>::max());
  return static_cast<unsigned int>(css_style_sheet_->css_rules_.size());
}

CSSRuleList::~CSSRuleList() {}

}  // namespace cssom
}  // namespace cobalt

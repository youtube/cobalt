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

#include "cobalt/cssom/css_condition_rule.h"

namespace cobalt {
namespace cssom {

bool CSSConditionRule::SetConditionValueAndTestIfChanged(
    bool cached_condition_value) {
  bool cached_condition_value_changed =
      cached_condition_value != cached_condition_value_;
  cached_condition_value_ = cached_condition_value;
  return cached_condition_value_changed;
}

CSSConditionRule::CSSConditionRule(
    const scoped_refptr<CSSRuleList>& css_rule_list)
    : CSSGroupingRule(css_rule_list), cached_condition_value_(false) {}

}  // namespace cssom
}  // namespace cobalt

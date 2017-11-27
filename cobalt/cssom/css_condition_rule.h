// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_CSS_CONDITION_RULE_H_
#define COBALT_CSSOM_CSS_CONDITION_RULE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/css_grouping_rule.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSRuleList;
class CSSRuleVisitor;

// The CSSConditionRule interface represents all the "conditional" at-rules,
// which consist of a condition and a statement block.
//   https://www.w3.org/TR/css3-conditional/#cssconditionrule
class CSSConditionRule : public CSSGroupingRule {
 public:
  // Web API: CSSConditionRule
  virtual std::string condition_text() = 0;
  virtual void set_condition_text(const std::string& condition) = 0;

  // Custom, not in any spec.
  //
  // From CSSRule.
  void Accept(CSSRuleVisitor* visitor) override {
    UNREFERENCED_PARAMETER(visitor);
    NOTREACHED();
  }

  // Returns the cached result of evaluating the condition.
  bool condition_value() { return cached_condition_value_; }

  // Sets the cached condition value, returns true if the value has changed,
  // false if not.
  bool SetConditionValueAndTestIfChanged(bool cached_condition_value);

  DEFINE_WRAPPABLE_TYPE(CSSConditionRule);

 protected:
  CSSConditionRule();
  explicit CSSConditionRule(const scoped_refptr<CSSRuleList>& css_rule_list);

  ~CSSConditionRule() override {}

 private:
  bool cached_condition_value_;

  DISALLOW_COPY_AND_ASSIGN(CSSConditionRule);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_CONDITION_RULE_H_

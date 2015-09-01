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

#ifndef CSSOM_CSS_CONDITION_RULE_H_
#define CSSOM_CSS_CONDITION_RULE_H_

#include "cobalt/cssom/css_grouping_rule.h"

#include <string>

namespace cobalt {
namespace cssom {

class CSSRuleVisitor;
class CSSStyleSheet;

// The CSSConditionRule interface represents all the "conditional" at-rules,
// which consist of a condition and a statement block.
//   http://www.w3.org/TR/css3-conditional/#cssconditionrule
class CSSConditionRule : public CSSGroupingRule {
 public:
  CSSConditionRule();
  explicit CSSConditionRule(const scoped_refptr<CSSRuleList>& css_rule_list);

  virtual std::string condition_text() = 0;
  virtual void set_condition_text(const std::string& condition) = 0;

  // Custom, not in any spec.
  //

  // Returns the cached result of evaluating the condition.
  virtual bool GetCachedConditionValue() = 0;

  // Evaluates the condition expression and caches the resulting condition
  // value. Returns true if the cached condition value has changed.
  virtual bool EvaluateConditionValue() = 0;

  // From CSSRule.
  void Accept(CSSRuleVisitor* visitor) OVERRIDE {
    UNREFERENCED_PARAMETER(visitor);
    NOTREACHED();
  }

  DEFINE_WRAPPABLE_TYPE(CSSConditionRule);

 protected:
  virtual ~CSSConditionRule() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CSSConditionRule);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_CONDITION_RULE_H_

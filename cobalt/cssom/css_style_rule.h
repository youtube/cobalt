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

#ifndef CSSOM_CSS_STYLE_RULE_H_
#define CSSOM_CSS_STYLE_RULE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/cascade_priority.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/selector.h"

namespace cobalt {
namespace cssom {

class CSSRuleVisitor;
class CSSStyleDeclaration;
class StyleSheet;

// The CSSStyleRule interface represents a style rule.
//   http://dev.w3.org/csswg/cssom/#the-cssstylerule-interface
class CSSStyleRule : public CSSRule {
 public:
  CSSStyleRule();
  CSSStyleRule(Selectors selectors,
               const scoped_refptr<CSSStyleDeclaration>& style);

  // Web API: CSSStyleRule
  const scoped_refptr<CSSStyleDeclaration>& style() const;

  // Web API: CSSRule
  //

  Type type() const OVERRIDE { return kStyleRule; }
  std::string css_text() const OVERRIDE;
  void set_css_text(const std::string& css_text) OVERRIDE;

  // Custom, not in any spec.
  //

  // From CSSRule.
  void Accept(CSSRuleVisitor* visitor) OVERRIDE;
  void AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) OVERRIDE;

  const Selectors& selectors() const { return selectors_; }

  DEFINE_WRAPPABLE_TYPE(CSSStyleRule);

 private:
  ~CSSStyleRule() OVERRIDE;

  Selectors selectors_;
  scoped_refptr<CSSStyleDeclaration> style_;
};

typedef base::hash_set<scoped_refptr<CSSStyleRule> > CSSRuleSet;
typedef std::pair<scoped_refptr<CSSStyleRule>, CascadePriority>
    RuleWithCascadePriority;
typedef std::vector<RuleWithCascadePriority> RulesWithCascadePriority;

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_STYLE_RULE_H_

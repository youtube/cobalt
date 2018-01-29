// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_CSS_STYLE_RULE_H_
#define COBALT_CSSOM_CSS_STYLE_RULE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/cascade_precedence.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/css_rule_style_declaration.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/selector.h"

namespace cobalt {
namespace cssom {

class CSSRuleVisitor;
class CSSStyleSheet;

// The CSSStyleRule interface represents a style rule.
//   https://www.w3.org/TR/2013/WD-cssom-20131205/#the-cssstylerule-interface
class CSSStyleRule : public CSSRule {
 public:
  CSSStyleRule();
  CSSStyleRule(Selectors selectors,
               const scoped_refptr<CSSRuleStyleDeclaration>& style);

  // Web API: CSSStyleRule
  const scoped_refptr<CSSStyleDeclaration> style() const;

  // Web API: CSSRule
  //
  Type type() const override { return kStyleRule; }
  std::string css_text(script::ExceptionState* exception_state) const override;
  void set_css_text(const std::string& css_text,
                    script::ExceptionState* exception_state) override;

  // Custom, not in any spec.
  //
  // From CSSRule.
  void Accept(CSSRuleVisitor* visitor) override;
  void AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) override;
  CSSStyleRule* AsCSSStyleRule() override { return this; }

  const Selectors& selectors() const { return selectors_; }

  bool added_to_selector_tree() const { return added_to_selector_tree_; }
  void set_added_to_selector_tree(bool added_to_selector_tree) {
    added_to_selector_tree_ = added_to_selector_tree;
  }

  // Custom.

  const scoped_refptr<CSSDeclaredStyleData>& declared_style_data() const {
    DCHECK(style_);
    return style_->data();
  }

  DEFINE_WRAPPABLE_TYPE(CSSStyleRule);

 private:
  ~CSSStyleRule() override;

  Selectors selectors_;
  scoped_refptr<CSSRuleStyleDeclaration> style_;
  bool added_to_selector_tree_;

  DISALLOW_COPY_AND_ASSIGN(CSSStyleRule);
};

typedef std::pair<scoped_refptr<CSSStyleRule>, CascadePrecedence>
    RuleWithCascadePrecedence;
typedef std::vector<RuleWithCascadePrecedence> RulesWithCascadePrecedence;

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_STYLE_RULE_H_

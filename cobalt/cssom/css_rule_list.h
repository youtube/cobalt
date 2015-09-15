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

#ifndef CSSOM_CSS_RULE_LIST_H_
#define CSSOM_CSS_RULE_LIST_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSMediaRule;
class CSSParser;
class CSSRuleVisitor;
class CSSStyleRule;
class CSSStyleSheet;

// The CSSRuleList interface represents an ordered collection of CSS
// style rules.
//   http://dev.w3.org/csswg/cssom/#the-cssrulelist-interface
class CSSRuleList : public base::SupportsWeakPtr<CSSRuleList>,
                    public script::Wrappable {
 public:
  CSSRuleList();

  // Web API: CSSRuleList
  //

  // Returns the index-th CSSRule object in the collection.
  // Returns null if there is no index-th object in the collection.
  scoped_refptr<CSSRule> Item(unsigned int index) const;

  // Returns the number of CSSRule objects represented by the collection.
  unsigned int length() const;

  // Custom, not in any spec.
  //

  // The are common methods for the CSSRuleList member in CSSGroupingRule and
  // CSSStyleSheet.

  CSSStyleSheet* ParentCSSStyleSheet() { return parent_style_sheet_; }

  // Returns a read-only, live object representing the CSS rules.
  CSSRules const* css_rules() const { return &css_rules_; }

  // Inserts a new rule into the list. This is a Web API in CSSGroupingRule and
  // CSSStyleSheet. It takes a string as input and parses it into a rule.
  unsigned int InsertRule(const std::string& rule, unsigned int index);

  // From StyleSheet.
  void AttachToCSSStyleSheet(CSSStyleSheet* style_sheet);

  // Appends a CSSStyleRule to the rule list.
  void AppendCSSStyleRule(const scoped_refptr<CSSStyleRule>& css_style_rule);

  // Appends a CSSMediaRule to the rule list.
  void AppendCSSMediaRule(const scoped_refptr<CSSMediaRule>& css_media_rule);

  void Accept(CSSRuleVisitor* visitor);

  DEFINE_WRAPPABLE_TYPE(CSSRuleList);

 private:
  ~CSSRuleList();

  CSSRules css_rules_;
  CSSStyleSheet* parent_style_sheet_;

  DISALLOW_COPY_AND_ASSIGN(CSSRuleList);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_RULE_LIST_H_

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

#ifndef COBALT_CSSOM_CSS_RULE_LIST_H_
#define COBALT_CSSOM_CSS_RULE_LIST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSRule;
class CSSRuleVisitor;
class CSSStyleSheet;

// The CSSRuleList interface represents an ordered collection of CSS
// style rules.
//   https://www.w3.org/TR/2013/WD-cssom-20131205/#the-cssrulelist-interface
class CSSRuleList : public base::SupportsWeakPtr<CSSRuleList>,
                    public script::Wrappable {
 public:
  CSSRuleList();

  // Web API: CSSRuleList
  //
  // Returns the number of CSSRule objects represented by the collection.
  unsigned int length() const;

  // The item(index) method must return the indexth CSSRule object in the
  // collection. If there is no indexth object in the collection, then the
  // method must return null.
  scoped_refptr<CSSRule> Item(unsigned int index) const;

  // Custom, not in any spec.
  //

  CSSStyleSheet* parent_css_style_sheet() { return parent_css_style_sheet_; }

  // Inserts a new rule into the list. This is used by CSSGroupingRule and
  // CSSStyleSheet. It takes a string as input and parses it into a rule.
  unsigned int InsertRule(const std::string& rule, unsigned int index);

  void AttachToCSSStyleSheet(CSSStyleSheet* style_sheet);

  void AppendCSSRule(const scoped_refptr<CSSRule>& css_rule);

  void Accept(CSSRuleVisitor* visitor);

  DEFINE_WRAPPABLE_TYPE(CSSRuleList);

 private:
  typedef std::vector<scoped_refptr<CSSRule> > CSSRules;

  ~CSSRuleList();

  CSSRules css_rules_;
  CSSStyleSheet* parent_css_style_sheet_;

  int next_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CSSRuleList);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_RULE_LIST_H_

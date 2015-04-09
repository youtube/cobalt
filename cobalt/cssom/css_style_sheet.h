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

#ifndef CSSOM_CSS_STYLE_SHEET_H_
#define CSSOM_CSS_STYLE_SHEET_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "cobalt/cssom/style_sheet.h"

namespace cobalt {
namespace cssom {

class CSSRuleList;
class CSSStyleRule;

// The CSSStyleSheet interface represents a CSS style sheet.
//   http://dev.w3.org/csswg/cssom/#the-cssstylesheet-interface
class CSSStyleSheet : public StyleSheet {
 public:
  CSSStyleSheet();

  // Web API: CSSStyleSheet
  //

  // Returns a read-only, live object representing the CSS rules.
  scoped_refptr<CSSRuleList> css_rules();

  // Custom, not in any spec.
  //
  void AppendRule(const scoped_refptr<CSSStyleRule>& css_rule);

  DEFINE_WRAPPABLE_TYPE(CSSStyleSheet);

 private:
  ~CSSStyleSheet() OVERRIDE;

  typedef std::vector<scoped_refptr<CSSStyleRule> > CSSRules;
  CSSRules css_rules_;

  base::WeakPtr<CSSRuleList> css_rule_list_;

  // Since CSSRuleList is merely a proxy, it needs access to CSS rules stored
  // in the stylesheet.
  friend class CSSRuleList;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_STYLE_SHEET_H_

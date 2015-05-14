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

class CSSParser;
class CSSRuleList;
class CSSStyleRule;
class StyleSheetList;

// The CSSStyleSheet interface represents a CSS style sheet.
//   http://dev.w3.org/csswg/cssom/#the-cssstylesheet-interface
// TODO(***REMOVED***): This interface currently assumes all rules are style rules.
// Handle other kinds of rules properly.
class CSSStyleSheet : public StyleSheet {
 public:
  CSSStyleSheet();
  explicit CSSStyleSheet(CSSParser* css_parser);

  // Web API: CSSStyleSheet
  //

  // Returns a read-only, live object representing the CSS rules.
  scoped_refptr<CSSRuleList> css_rules();

  // Inserts a new rule into the current style sheet. This Web API takes a
  // string as input and parses it into a rule.
  unsigned int InsertRule(const std::string& rule, unsigned int index);

  // Custom, not in any spec.
  //

  // From StyleSheet.
  void AttachToStyleSheetList(StyleSheetList* style_sheet_list) OVERRIDE;

  // Appends a CSSStyleRule to the current style sheet.
  void AppendCSSStyleRule(const scoped_refptr<CSSStyleRule>& css_style_rule);

  void SetLocationUrl(const GURL& url) OVERRIDE;
  GURL& LocationUrl() OVERRIDE;
  StyleSheetList* ParentStyleSheetList() OVERRIDE;

  DEFINE_WRAPPABLE_TYPE(CSSStyleSheet);

 private:
  ~CSSStyleSheet() OVERRIDE;

  typedef std::vector<scoped_refptr<CSSStyleRule> > CSSRules;
  CSSRules css_rules_;

  base::WeakPtr<CSSRuleList> css_rule_list_;

  StyleSheetList* parent_style_sheet_list_;
  CSSParser* css_parser_;
  GURL location_url_;

  // Since CSSRuleList is merely a proxy, it needs access to CSS rules stored
  // in the stylesheet.
  friend class CSSRuleList;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_STYLE_SHEET_H_

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

#include "cobalt/cssom/css_style_sheet.h"

#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"

namespace cobalt {
namespace cssom {

CSSStyleSheet::CSSStyleSheet()
    : parent_style_sheet_list_(NULL), css_parser_(NULL) {}

CSSStyleSheet::CSSStyleSheet(CSSParser* css_parser)
    : parent_style_sheet_list_(NULL), css_parser_(css_parser) {}

scoped_refptr<CSSRuleList> CSSStyleSheet::css_rules() {
  if (css_rule_list_) {
    return css_rule_list_.get();
  }

  scoped_refptr<CSSRuleList> css_rule_list = new CSSRuleList(this);
  css_rule_list_ = css_rule_list->AsWeakPtr();
  return css_rule_list;
}

unsigned int CSSStyleSheet::InsertRule(const std::string& rule,
                                       unsigned int index) {
  scoped_refptr<CSSStyleRule> css_rule = css_parser_->ParseStyleRule(
      rule, base::SourceLocation("[object CSSStyleSheet]", 1, 1));

  if (!css_rule) {
    return 0;
  }

  if (index > css_rules()->length()) {
    // TODO(***REMOVED***): Throw JS IndexSizeError.
    LOG(ERROR) << "IndexSizeError";
    return 0;
  }

  // TODO(***REMOVED***): Currently we only support appending rule to the end of the
  // rule list, which is the use case in performance spike and ***REMOVED***. Properly
  // implement insertion if necessary.
  if (index != css_rules()->length()) {
    LOG(WARNING) << "InsertRule will always append the rule to the end of the "
                    "rule list.";
  }

  AppendCSSStyleRule(css_rule);
  return index;
}

void CSSStyleSheet::AttachToStyleSheetList(StyleSheetList* style_sheet_list) {
  parent_style_sheet_list_ = style_sheet_list;
  for (CSSRules::iterator it = css_rules_.begin(); it != css_rules_.end();
       ++it) {
    (*it)->AttachToStyleSheet(this);
  }
}

void CSSStyleSheet::AppendCSSStyleRule(
    const scoped_refptr<CSSStyleRule>& css_style_rule) {
  css_rules_.push_back(css_style_rule);
  if (parent_style_sheet_list_) {
    css_style_rule->AttachToStyleSheet(this);
  }
}

void CSSStyleSheet::SetLocationUrl(const GURL& url) { location_url_ = url; }

GURL& CSSStyleSheet::LocationUrl() { return location_url_; }

StyleSheetList* CSSStyleSheet::ParentStyleSheetList() {
  return parent_style_sheet_list_;
}

CSSStyleSheet::~CSSStyleSheet() {}

}  // namespace cssom
}  // namespace cobalt

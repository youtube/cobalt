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

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/cssom/style_sheet.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace cssom {

class CSSParser;
class CSSRuleList;
class CSSStyleRule;
class StyleSheetList;

typedef base::hash_set<scoped_refptr<CSSStyleRule> > CSSRuleSet;
typedef base::hash_map<std::string, CSSRuleSet> StringToCSSRuleSetMap;

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

  // Set the css rules for the style sheet.
  void set_css_rules(const scoped_refptr<CSSRuleList>& css_rule_list);

  // Returns a read-only, live object representing the CSS rules.
  scoped_refptr<CSSRuleList> css_rules();

  // Inserts a new rule into the current style sheet. This Web API takes a
  // string as input and parses it into a rule.
  unsigned int InsertRule(const std::string& rule, unsigned int index);

  // Custom, not in any spec.
  //

  // From StyleSheet.
  void AttachToStyleSheetList(StyleSheetList* style_sheet_list) OVERRIDE;

  void SetLocationUrl(const GURL& url) OVERRIDE;
  GURL& LocationUrl() OVERRIDE;
  StyleSheetList* ParentStyleSheetList() OVERRIDE;

  // If the rule indexes are dirty, as indicated by the rule_indexes_dirty flag,
  // assign the priority index to each rule in the rule list, and index the
  // rules by selectors.
  void MaybeUpdateRuleIndexes();

  // This should be called when there is a change in the set of active CSS rules
  // for the style sheet. This can happen when a rule is added to or removed
  // from the css_rule_list or when the css_rule_list is replaced. This can also
  // happen when rules become active or inactive due to a change in the value of
  // GetCachedConditionValue() of a CSSConditionRule derived rule.
  void set_rule_indexes_dirty() { rule_indexes_dirty_ = true; }

  // Should be called when a media feature may have changed, triggering a
  // possible recalculation of the media rule expressions, and the rule indexes.
  // TODO(***REMOVED***): Call this when a media feature changes.
  void OnMediaFeatureChanged();

  const StringToCSSRuleSetMap& class_selector_rules_map() const {
    return class_selector_rules_map_;
  }
  const StringToCSSRuleSetMap& id_selector_rules_map() const {
    return id_selector_rules_map_;
  }
  const StringToCSSRuleSetMap& type_selector_rules_map() const {
    return type_selector_rules_map_;
  }
  const CSSRuleSet& empty_pseudo_class_rules() const {
    return empty_pseudo_class_rules_;
  }

  CSSParser* css_parser() const { return css_parser_; }

  DEFINE_WRAPPABLE_TYPE(CSSStyleSheet);

 private:
  class CSSStyleRuleIndexer;
  class CSSRuleIndexer;

  ~CSSStyleSheet() OVERRIDE;

  StringToCSSRuleSetMap class_selector_rules_map_;
  StringToCSSRuleSetMap id_selector_rules_map_;
  StringToCSSRuleSetMap type_selector_rules_map_;
  CSSRuleSet empty_pseudo_class_rules_;

  scoped_refptr<CSSRuleList> css_rule_list_;

  StyleSheetList* parent_style_sheet_list_;
  CSSParser* const css_parser_;
  GURL location_url_;

  bool rule_indexes_dirty_;

  // Since CSSRuleList is merely a proxy, it needs access to CSS rules stored
  // in the stylesheet.
  friend class CSSRuleList;

  DISALLOW_COPY_AND_ASSIGN(CSSStyleSheet);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_STYLE_SHEET_H_

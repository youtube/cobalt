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
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/cssom/style_sheet.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace cssom {

class CSSParser;
class CSSRuleList;
class CSSStyleRule;
class PropertyValue;
class StyleSheetList;

typedef base::hash_set<scoped_refptr<CSSStyleRule> > CSSRuleSet;
typedef base::hash_map<std::string, CSSRuleSet> StringToCSSRuleSetMap;

// The CSSStyleSheet interface represents a CSS style sheet.
//   http://www.w3.org/TR/2013/WD-cssom-20131205/#the-cssstylesheet-interface
class CSSStyleSheet : public StyleSheet, public MutationObserver {
 public:
  CSSStyleSheet();
  explicit CSSStyleSheet(CSSParser* css_parser);

  // Web API: CSSStyleSheet
  //
  // Returns a read-only, live object representing the CSS rules.
  const scoped_refptr<CSSRuleList>& css_rules();

  // Inserts a new rule into the current style sheet. This Web API takes a
  // string as input and parses it into a rule.
  unsigned int InsertRule(const std::string& rule, unsigned int index);

  // Custom, not in any spec.
  //
  // From StyleSheet.
  void AttachToStyleSheetList(StyleSheetList* style_sheet_list) OVERRIDE;
  void SetLocationUrl(const GURL& url) OVERRIDE { location_url_ = url; }
  GURL& LocationUrl() OVERRIDE { return location_url_; }
  StyleSheetList* ParentStyleSheetList() OVERRIDE {
    return parent_style_sheet_list_;
  }
  scoped_refptr<CSSStyleSheet> AsCSSStyleSheet() OVERRIDE { return this; }

  // From MutationObserver.
  void OnCSSMutation() OVERRIDE;

  CSSParser* css_parser() const { return css_parser_; }

  void set_css_rules(const scoped_refptr<CSSRuleList>& css_rule_list);

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

  // If the rule indexes are dirty, as indicated by the rule_indexes_dirty flag,
  // assign the priority index to each rule in the rule list, and index the
  // rules by selectors.
  void MaybeUpdateRuleIndexes();

  // This performs a recalculation of the media rule expressions, if needed.
  void EvaluateMediaRules(const scoped_refptr<PropertyValue>& width,
                          const scoped_refptr<PropertyValue>& height);

  // Should be called when a media rule is added or modified. It sets a flag
  // that is reset in EvaluateMediaRules().
  void OnMediaRuleMutation() { media_rules_changed_ = true; }

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

  // When true, this means that a rule has been added or modified, and the rule
  // indexes need to be updated. This flag is set in OnCSSMutation(), and
  // cleared in MaybeUpdateRuleIndexes().
  bool rule_indexes_dirty_;

  // When true, this means a media rule has been added or modified, and a
  // re-evaluation of the media at-rules is needed.
  bool media_rules_changed_;

  // This stores the most recent media parameters, used to detect when they
  // change, which will require a re-evaluation of the media rule expressions.
  scoped_refptr<PropertyValue> previous_media_width_;
  scoped_refptr<PropertyValue> previous_media_height_;

  // Since CSSRuleList is merely a proxy, it needs access to CSS rules stored
  // in the stylesheet.
  friend class CSSRuleList;

  DISALLOW_COPY_AND_ASSIGN(CSSStyleSheet);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_STYLE_SHEET_H_

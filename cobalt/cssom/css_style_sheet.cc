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

#include "cobalt/cssom/adjacent_selector.h"
#include "cobalt/cssom/after_pseudo_element.h"
#include "cobalt/cssom/before_pseudo_element.h"
#include "cobalt/cssom/child_combinator.h"
#include "cobalt/cssom/class_selector.h"
#include "cobalt/cssom/complex_selector.h"
#include "cobalt/cssom/compound_selector.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/descendant_combinator.h"
#include "cobalt/cssom/empty_pseudo_class.h"
#include "cobalt/cssom/id_selector.h"
#include "cobalt/cssom/selector_visitor.h"
#include "cobalt/cssom/type_selector.h"

namespace cobalt {
namespace cssom {

//////////////////////////////////////////////////////////////////////////
// CSSStyleSheet::RuleIndexer
//////////////////////////////////////////////////////////////////////////

// This class is used to index the css style rules according to the simple
// selectors in the last compound selector.
class CSSStyleSheet::RuleIndexer : public SelectorVisitor {
 public:
  RuleIndexer(CSSStyleRule* css_style_rule, CSSStyleSheet* css_style_sheet)
      : css_style_rule_(css_style_rule), css_style_sheet_(css_style_sheet) {}

  void VisitAfterPseudoElement(
      AfterPseudoElement* /* after_pseudo_element */) OVERRIDE {
    // Do nothing.
  }

  void VisitBeforePseudoElement(
      BeforePseudoElement* /* before_pseudo_element */) OVERRIDE {
    // Do nothing.
  }

  void VisitClassSelector(ClassSelector* class_selector) OVERRIDE {
    StringToCSSRuleSetMap& rules_map =
        css_style_sheet_->class_selector_rules_map_;
    rules_map[class_selector->class_name()].insert(css_style_rule_);
  }

  void VisitIdSelector(IdSelector* id_selector) OVERRIDE {
    StringToCSSRuleSetMap& rules_map = css_style_sheet_->id_selector_rules_map_;
    rules_map[id_selector->id()].insert(css_style_rule_);
  }

  void VisitTypeSelector(TypeSelector* type_selector) OVERRIDE {
    StringToCSSRuleSetMap& rules_map =
        css_style_sheet_->type_selector_rules_map_;
    rules_map[type_selector->element_name()].insert(css_style_rule_);
  }

  void VisitEmptyPseudoClass(
      EmptyPseudoClass* /* empty_pseudo_class */) OVERRIDE {
    css_style_sheet_->empty_pseudo_class_rules_.insert(css_style_rule_);
  }

  void VisitCompoundSelector(CompoundSelector* compound_selector) OVERRIDE {
    for (Selectors::const_iterator selector_iterator =
             compound_selector->selectors().begin();
         selector_iterator != compound_selector->selectors().end();
         ++selector_iterator) {
      (*selector_iterator)->Accept(this);
    }
  }

  void VisitAdjacentSelector(AdjacentSelector* adjacent_selector) OVERRIDE {
    adjacent_selector->last_selector()->Accept(this);
  }

  void VisitComplexSelector(ComplexSelector* complex_selector) OVERRIDE {
    complex_selector->last_selector()->Accept(this);
  }

 private:
  CSSStyleRule* css_style_rule_;
  CSSStyleSheet* css_style_sheet_;

  DISALLOW_COPY_AND_ASSIGN(RuleIndexer);
};

//////////////////////////////////////////////////////////////////////////
// CSSStyleSheet
//////////////////////////////////////////////////////////////////////////

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

  if (index > css_rules_.size()) {
    // TODO(***REMOVED***): Throw JS IndexSizeError.
    LOG(ERROR) << "IndexSizeError";
    return 0;
  }

  // TODO(***REMOVED***): Currently we only support appending rule to the end of the
  // rule list, which is the use case in performance spike and ***REMOVED***. Properly
  // implement insertion if necessary.
  if (index != css_rules_.size()) {
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
  css_style_rule->set_index(static_cast<int>(css_rules_.size()));
  css_rules_.push_back(css_style_rule);
  if (parent_style_sheet_list_) {
    css_style_rule->AttachToStyleSheet(this);
  }
  for (Selectors::const_iterator it = css_style_rule->selectors().begin();
       it != css_style_rule->selectors().end(); ++it) {
    RuleIndexer rule_indexer(css_style_rule, this);
    (*it)->Accept(&rule_indexer);
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

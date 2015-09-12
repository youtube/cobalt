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
#include "cobalt/cssom/css_condition_rule.h"
#include "cobalt/cssom/css_grouping_rule.h"
#include "cobalt/cssom/css_media_rule.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_rule_visitor.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/descendant_combinator.h"
#include "cobalt/cssom/empty_pseudo_class.h"
#include "cobalt/cssom/id_selector.h"
#include "cobalt/cssom/selector.h"
#include "cobalt/cssom/selector_visitor.h"
#include "cobalt/cssom/type_selector.h"

namespace cobalt {
namespace cssom {
namespace {

//////////////////////////////////////////////////////////////////////////
// MediaRuleUpdater
//////////////////////////////////////////////////////////////////////////

class MediaRuleUpdater : public CSSRuleVisitor {
 public:
  MediaRuleUpdater(const scoped_refptr<PropertyValue>& width,
                   const scoped_refptr<PropertyValue>& height)
      : any_condition_value_changed_(false), width_(width), height_(height) {}

  void VisitCSSStyleRule(CSSStyleRule* css_style_rule) OVERRIDE {
    UNREFERENCED_PARAMETER(css_style_rule);
    // Do nothing.
  }

  void VisitCSSMediaRule(CSSMediaRule* css_media_rule) OVERRIDE {
    bool condition_value_changed =
        css_media_rule->EvaluateConditionValueAndReturnIfChanged(width_,
                                                                 height_);
    any_condition_value_changed_ |= condition_value_changed;
  }

  bool AnyConditionValueChanged() { return any_condition_value_changed_; }

 private:
  bool any_condition_value_changed_;
  const scoped_refptr<PropertyValue>& width_;
  const scoped_refptr<PropertyValue>& height_;

  DISALLOW_COPY_AND_ASSIGN(MediaRuleUpdater);
};

}  // namespace

//////////////////////////////////////////////////////////////////////////
// CSSStyleSheet::CSSStyleRuleIndexer
//////////////////////////////////////////////////////////////////////////

// This class is used to index the css style rules according to the simple
// selectors in the last compound selector.
class CSSStyleSheet::CSSStyleRuleIndexer : public SelectorVisitor {
 public:
  CSSStyleRuleIndexer(CSSStyleRule* css_style_rule,
                      CSSStyleSheet* css_style_sheet)
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

  DISALLOW_COPY_AND_ASSIGN(CSSStyleRuleIndexer);
};

//////////////////////////////////////////////////////////////////////////
// CSSStyleSheet::CSSRuleIndexer
//////////////////////////////////////////////////////////////////////////

class CSSStyleSheet::CSSRuleIndexer : public CSSRuleVisitor {
 public:
  explicit CSSRuleIndexer(CSSStyleSheet* css_style_sheet)
      : next_css_rule_priority_index_(0), css_style_sheet_(css_style_sheet) {}

  void VisitCSSStyleRule(CSSStyleRule* css_style_rule) OVERRIDE {
    css_style_rule->set_index(next_css_rule_priority_index_++);
    for (Selectors::const_iterator it = css_style_rule->selectors().begin();
         it != css_style_rule->selectors().end(); ++it) {
      CSSStyleSheet::CSSStyleRuleIndexer rule_indexer(css_style_rule,
                                                      css_style_sheet_);
      (*it)->Accept(&rule_indexer);
    }
  }

  void VisitCSSMediaRule(CSSMediaRule* css_media_rule) OVERRIDE {
    css_media_rule->set_index(next_css_rule_priority_index_++);
    if (css_media_rule->GetCachedConditionValue()) {
      css_media_rule->css_rules()->Accept(this);
    }
  }

 private:
  // The priority index to be used for the next appended CSS Rule.
  // This is used for the Appearance parameter of the CascadePriority.
  //   http://www.w3.org/TR/css-cascade-3/#cascade-order
  int next_css_rule_priority_index_;

  CSSStyleSheet* css_style_sheet_;

  DISALLOW_COPY_AND_ASSIGN(CSSRuleIndexer);
};

//////////////////////////////////////////////////////////////////////////
// CSSStyleSheet
//////////////////////////////////////////////////////////////////////////

CSSStyleSheet::CSSStyleSheet()
    : parent_style_sheet_list_(NULL),
      css_parser_(NULL),
      rule_indexes_dirty_(false),
      media_rules_changed_(false) {}

CSSStyleSheet::CSSStyleSheet(CSSParser* css_parser)
    : parent_style_sheet_list_(NULL),
      css_parser_(css_parser),
      rule_indexes_dirty_(false),
      media_rules_changed_(false) {}

void CSSStyleSheet::set_css_rules(
    const scoped_refptr<CSSRuleList>& css_rule_list) {
  DCHECK(css_rule_list);
  if (css_rule_list == css_rule_list_) {
    return;
  }
  if (parent_style_sheet_list_) {
    css_rule_list->AttachToCSSStyleSheet(this);
  }
  bool rules_possibly_added_or_changed_or_removed =
      (css_rule_list->length() > 0) ||
      (css_rule_list_ && css_rule_list_->length() > 0);
  css_rule_list_ = css_rule_list;
  if (rules_possibly_added_or_changed_or_removed) {
    OnCSSMutation();
  }
}

scoped_refptr<CSSRuleList> CSSStyleSheet::css_rules() {
  if (!css_rule_list_) {
    set_css_rules(new CSSRuleList());
  }
  DCHECK(css_rule_list_);
  return css_rule_list_;
}

unsigned int CSSStyleSheet::InsertRule(const std::string& rule,
                                       unsigned int index) {
  return css_rules()->InsertRule(rule, index);
}

void CSSStyleSheet::OnCSSMutation() {
  rule_indexes_dirty_ = true;
  if (parent_style_sheet_list_) {
    parent_style_sheet_list_->OnCSSMutation();
  }
}

void CSSStyleSheet::AttachToStyleSheetList(StyleSheetList* style_sheet_list) {
  parent_style_sheet_list_ = style_sheet_list;
  if (css_rule_list_) {
    css_rule_list_->AttachToCSSStyleSheet(this);
  }
}

void CSSStyleSheet::SetLocationUrl(const GURL& url) { location_url_ = url; }

GURL& CSSStyleSheet::LocationUrl() { return location_url_; }

StyleSheetList* CSSStyleSheet::ParentStyleSheetList() {
  return parent_style_sheet_list_;
}

void CSSStyleSheet::MaybeUpdateRuleIndexes() {
  if (rule_indexes_dirty_) {
    CSSRuleIndexer rule_indexer(this);
    css_rule_list_->Accept(&rule_indexer);
    rule_indexes_dirty_ = false;
  }
}

void CSSStyleSheet::EvaluateMediaRules(
    const scoped_refptr<PropertyValue>& width,
    const scoped_refptr<PropertyValue>& height) {
  // If the media rules change, we have to do an update.
  bool update_media_rules = media_rules_changed_;
  media_rules_changed_ = false;

  if (!css_rule_list_) {
    return;
  }

  // If the media parameters change, we have to do an update.
  if (!update_media_rules) {
    if (!width || !previous_media_width_ ||
        width->Equals(*previous_media_width_) || !height ||
        !previous_media_height_ || height->Equals(*previous_media_height_)) {
      update_media_rules = true;
    }
  }

  if (update_media_rules) {
    // Evaluate the media rule conditions, and signal a css mutation if any
    // condition values have changed.
    MediaRuleUpdater media_rule_updater(width, height);
    css_rule_list_->Accept(&media_rule_updater);
    if (media_rule_updater.AnyConditionValueChanged()) {
      OnCSSMutation();
    }
    previous_media_width_ = width;
    previous_media_height_ = height;
  }
}

CSSStyleSheet::~CSSStyleSheet() {}

}  // namespace cssom
}  // namespace cobalt

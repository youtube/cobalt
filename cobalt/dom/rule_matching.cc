/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/rule_matching.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "base/string_util.h"
#include "cobalt/base/unused.h"
#include "cobalt/cssom/after_pseudo_element.h"
#include "cobalt/cssom/before_pseudo_element.h"
#include "cobalt/cssom/child_combinator.h"
#include "cobalt/cssom/class_selector.h"
#include "cobalt/cssom/combinator.h"
#include "cobalt/cssom/combinator_visitor.h"
#include "cobalt/cssom/complex_selector.h"
#include "cobalt/cssom/compound_selector.h"
#include "cobalt/cssom/descendant_combinator.h"
#include "cobalt/cssom/empty_pseudo_class.h"
#include "cobalt/cssom/following_sibling_combinator.h"
#include "cobalt/cssom/id_selector.h"
#include "cobalt/cssom/next_sibling_combinator.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/pseudo_element.h"
#include "cobalt/cssom/selector_visitor.h"
#include "cobalt/cssom/simple_selector.h"
#include "cobalt/cssom/type_selector.h"
#include "cobalt/dom/dom_token_list.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/pseudo_element.h"
#include "cobalt/math/safe_integer_conversions.h"

namespace cobalt {
namespace dom {
namespace {

//////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////

// Copies all rules in |map| with given |key| to |rule_set|.
void CopyMatchingRules(const cssom::StringToCSSRuleSetMap& map,
                       const std::string& key, cssom::CSSRuleSet* rule_set) {
  if (key.empty()) {
    return;
  }
  cssom::StringToCSSRuleSetMap::const_iterator find_result = map.find(key);
  if (find_result != map.end()) {
    rule_set->insert(find_result->second.begin(), find_result->second.end());
  }
}


// Matches an element against a selector. If the element doesn't match, returns
// NULL, otherwise returns the result of advancing the pointer to the element
// according to the combinators. If the optional pseudo_element_type output is
// given, it will be set to the type of pseudo element addressed by the
// selector, or to kNotPseudoElementType if no pseudo element is addressed.
Element* MatchSelectorAndElement(cssom::Selector* selector, Element* element,
                                 PseudoElementType* pseudo_element_type);
Element* MatchSelectorAndElement(cssom::Selector* selector, Element* element) {
  return MatchSelectorAndElement(selector, element, NULL);
}


//////////////////////////////////////////////////////////////////////////
// Combinator matcher
//////////////////////////////////////////////////////////////////////////

class CombinatorMatcher : public cssom::CombinatorVisitor {
 public:
  explicit CombinatorMatcher(Element* element) : element_(element) {
    DCHECK(element);
  }

  // Child combinator describes a childhood relationship between two elements.
  //   http://www.w3.org/TR/selectors4/#child-combinators
  void VisitChildCombinator(cssom::ChildCombinator* child_combinator) OVERRIDE {
    element_ = MatchSelectorAndElement(child_combinator->selector(),
                                       element_->parent_element());
  }

  // Next-sibling combinator describes that the elements represented by the two
  // compound selectors share the same parent in the document tree and the
  // element represented by the first compound selector immediately precedes the
  // element represented by the second one.
  //   http://www.w3.org/TR/selectors4/#adjacent-sibling-combinators
  void VisitNextSiblingCombinator(
      cssom::NextSiblingCombinator* next_sibling_combinator) OVERRIDE {
    element_ = MatchSelectorAndElement(next_sibling_combinator->selector(),
                                       element_->previous_element_sibling());
  }

  // Descendant combinator describes an element that is the descendant of
  // another element in the document tree.
  //   http://www.w3.org/TR/selectors4/#descendant-combinators
  void VisitDescendantCombinator(
      cssom::DescendantCombinator* descendant_combinator) OVERRIDE {
    do {
      element_ = element_->parent_element();
      Element* element =
          MatchSelectorAndElement(descendant_combinator->selector(), element_);
      if (element) {
        element_ = element;
        return;
      }
    } while (element_);
  }

  // Following-sibling combinator describes that the elements represented by the
  // two compound selectors share the same parent in the document tree and the
  // element represented by the first compound selector precedes (not
  // necessarily immediately) the element represented by the second one.
  //   http://www.w3.org/TR/selectors4/#general-sibling-combinators
  void VisitFollowingSiblingCombinator(
      cssom::FollowingSiblingCombinator* following_sibling_combinator)
      OVERRIDE {
    do {
      element_ = element_->previous_element_sibling();
      Element* element = MatchSelectorAndElement(
          following_sibling_combinator->selector(), element_);
      if (element) {
        element_ = element;
        return;
      }
    } while (element_);
  }

  Element* element() { return element_; }

 private:
  Element* element_;
  DISALLOW_COPY_AND_ASSIGN(CombinatorMatcher);
};

//////////////////////////////////////////////////////////////////////////
// Selector matcher
//////////////////////////////////////////////////////////////////////////

class SelectorMatcher : public cssom::SelectorVisitor {
 public:
  explicit SelectorMatcher(Element* element,
                           PseudoElementType* pseudo_element_type)
      : element_(element), pseudo_element_type_(pseudo_element_type) {
    DCHECK(element);
    if (pseudo_element_type_) {
      *pseudo_element_type_ = kNotPseudoElementType;
    }
  }
  explicit SelectorMatcher(Element* element)
      : element_(element), pseudo_element_type_(NULL) {
    DCHECK(element);
  }

  // The :after pseudo-element represents a generated element.
  //   http://www.w3.org/TR/CSS21/generate.html#before-after-content
  void VisitAfterPseudoElement(cssom::AfterPseudoElement*) OVERRIDE {
    // Only matches if we asked for the PseudoElementType
    if (pseudo_element_type_) {
      DCHECK_NE(*pseudo_element_type_, kBeforePseudoElementType);
      *pseudo_element_type_ = kAfterPseudoElementType;
    }
  }

  // The :before pseudo-element represents a generated element.
  //   http://www.w3.org/TR/CSS21/generate.html#before-after-content
  void VisitBeforePseudoElement(cssom::BeforePseudoElement*) OVERRIDE {
    // Only matches if we asked for the PseudoElementType
    if (pseudo_element_type_) {
      DCHECK_NE(*pseudo_element_type_, kAfterPseudoElementType);
      *pseudo_element_type_ = kBeforePseudoElementType;
    }
  }

  // The class selector represents an element belonging to the class identified
  // by the identifier.
  //   http://www.w3.org/TR/selectors4/#class-selector
  void VisitClassSelector(cssom::ClassSelector* class_selector) OVERRIDE {
    if (!element_->class_list()->ContainsValid(class_selector->class_name())) {
      element_ = NULL;
    }
  }

  // The :empty pseudo-class represents an element that has no content children.
  //   http://www.w3.org/TR/selectors4/#empty-pseudo
  void VisitEmptyPseudoClass(cssom::EmptyPseudoClass*) OVERRIDE {
    if (!element_->IsEmpty()) {
      element_ = NULL;
    }
  }

  // Unsupported pseudo-class never matches.
  void VisitUnsupportedPseudoClass(cssom::UnsupportedPseudoClass*) OVERRIDE {
    element_ = NULL;
  }

  // An ID selector represents an element instance that has an identifier that
  // matches the identifier in the ID selector.
  //   http://www.w3.org/TR/selectors4/#id-selector
  void VisitIdSelector(cssom::IdSelector* id_selector) OVERRIDE {
    if (id_selector->id() != element_->id()) {
      element_ = NULL;
    }
  }

  // A type selector represents an instance of the element type in the document
  // tree.
  //   http://www.w3.org/TR/selectors4/#type-selector
  void VisitTypeSelector(cssom::TypeSelector* type_selector) OVERRIDE {
    if (!LowerCaseEqualsASCII(type_selector->element_name(),
                              element_->tag_name().c_str())) {
      element_ = NULL;
    }
  }

  // A compound selector is a chain of simple selectors that are not separated
  // by a combinator.
  //   http://www.w3.org/TR/selectors4/#compound
  void VisitCompoundSelector(
      cssom::CompoundSelector* compound_selector) OVERRIDE {
    DCHECK_GT(compound_selector->simple_selectors().size(), 0U);
    // Iterate through all the simple selectors. If any of the simple selectors
    // doesn't match, the compound selector doesn't match.
    for (cssom::CompoundSelector::SimpleSelectors::const_iterator
             selector_iterator = compound_selector->simple_selectors().begin();
         selector_iterator != compound_selector->simple_selectors().end();
         ++selector_iterator) {
      element_ = MatchSelectorAndElement(*selector_iterator, element_,
                                         pseudo_element_type_);
      if (!element_) {
        return;
      }
    }
  }

  // A complex selector is a chain of one or more compound selectors separated
  // by combinators.
  //   http://www.w3.org/TR/selectors4/#complex
  void VisitComplexSelector(cssom::ComplexSelector* complex_selector) OVERRIDE {
    // Match the element against the last adjacent selector.
    element_ = MatchSelectorAndElement(complex_selector->first_selector(),
                                       element_, pseudo_element_type_);
    if (!element_) {
      return;
    }

    // Iterate through all the indirect combinators and advance the pointer. If
    // any of the indirect combinators doesn't match, the complex selector
    // doesn't match.
    for (cssom::Combinators::const_reverse_iterator combinator_iterator =
             complex_selector->combinators().rbegin();
         combinator_iterator != complex_selector->combinators().rend();
         ++combinator_iterator) {
      CombinatorMatcher combinator_matcher(element_);
      (*combinator_iterator)->Accept(&combinator_matcher);
      element_ = combinator_matcher.element();
      if (!element_) {
        return;
      }
    }
  }

  Element* element() const { return element_; }

 private:
  Element* element_;
  PseudoElementType* pseudo_element_type_;
  DISALLOW_COPY_AND_ASSIGN(SelectorMatcher);
};

//////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////

Element* MatchSelectorAndElement(cssom::Selector* selector, Element* element,
                                 PseudoElementType* pseudo_element_type) {
  DCHECK(selector);
  if (!element) {
    return NULL;
  }
  SelectorMatcher selector_matcher(element, pseudo_element_type);
  selector->Accept(&selector_matcher);
  return selector_matcher.element();
}

}  // namespace

//////////////////////////////////////////////////////////////////////////
// Interface
//////////////////////////////////////////////////////////////////////////

bool MatchRuleAndElement(cssom::CSSStyleRule* rule, Element* element) {
  for (cssom::Selectors::const_iterator selector_iterator =
           rule->selectors().begin();
       selector_iterator != rule->selectors().end(); ++selector_iterator) {
    DCHECK(*selector_iterator);
    if (MatchSelectorAndElement(*selector_iterator, element)) {
      return true;
    }
  }
  return false;
}

void EvaluateStyleSheetMediaRules(
    const scoped_refptr<cssom::CSSStyleDeclarationData>& root_computed_style,
    const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet,
    const scoped_refptr<cssom::StyleSheetList>& author_style_sheets) {
  scoped_refptr<cssom::PropertyValue> width(root_computed_style->width());
  scoped_refptr<cssom::PropertyValue> height(root_computed_style->height());

  TRACE_EVENT0("cobalt::dom", "EvaluateStyleSheetMediaRules()");
  user_agent_style_sheet->EvaluateMediaRules(width, height);
  DCHECK(author_style_sheets);
  for (unsigned int style_sheet_index = 0;
       style_sheet_index < author_style_sheets->length(); ++style_sheet_index) {
    scoped_refptr<cssom::CSSStyleSheet> style_sheet =
        author_style_sheets->Item(style_sheet_index)->AsCSSStyleSheet();
    style_sheet->EvaluateMediaRules(width, height);
  }
}

void UpdateStyleSheetRuleIndexes(
    const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet,
    const scoped_refptr<cssom::StyleSheetList>& author_style_sheets) {
  TRACE_EVENT0("cobalt::dom", "UpdateStyleSheetRuleIndexes()");
  user_agent_style_sheet->MaybeUpdateRuleIndexes();
  DCHECK(author_style_sheets);
  for (unsigned int style_sheet_index = 0;
       style_sheet_index < author_style_sheets->length(); ++style_sheet_index) {
    scoped_refptr<cssom::CSSStyleSheet> style_sheet =
        author_style_sheets->Item(style_sheet_index)->AsCSSStyleSheet();
    style_sheet->MaybeUpdateRuleIndexes();
  }
}

void UpdateMatchingRulesFromStyleSheet(
    const scoped_refptr<cssom::CSSStyleSheet>& style_sheet,
    HTMLElement* element, cssom::Origin origin) {
  cssom::CSSRuleSet candidate_rules;

  // TODO(***REMOVED***): Take !important into consideration
  // If the rule has '!important', the origin value used for the
  // cssom::CascadePriority should not be kNormal(.*) but kImportant${1}

  DCHECK(element->matching_rules());

  // Add candidate rules according to class name.
  scoped_refptr<DOMTokenList> class_list = element->class_list();
  for (unsigned int index = 0; index < class_list->length(); ++index) {
    CopyMatchingRules(style_sheet->class_selector_rules_map(),
                      class_list->NonNullItem(index), &candidate_rules);
  }
  // Add candidate rules according to id.
  CopyMatchingRules(style_sheet->id_selector_rules_map(), element->id(),
                    &candidate_rules);

  // Add candidate rules according to type.
  CopyMatchingRules(style_sheet->type_selector_rules_map(), element->tag_name(),
                    &candidate_rules);

  // Add candidate rules according to emptiness.
  if (element->IsEmpty()) {
    candidate_rules.insert(style_sheet->empty_pseudo_class_rules().begin(),
                           style_sheet->empty_pseudo_class_rules().end());
  }

  scoped_ptr<PseudoElement> pseudo_elements[kMaxPseudoElementType];

  // Go through all the candidate rules.
  for (cssom::CSSRuleSet::const_iterator rule_iterator =
           candidate_rules.begin();
       rule_iterator != candidate_rules.end(); ++rule_iterator) {
    scoped_refptr<cssom::CSSStyleRule> rule = *rule_iterator;
    // Match the element against all selectors in the selector list of the
    // rule. Add the rule to matching rules if any selector matches the element,
    // and use the highest specificity.
    bool matches[kMaxAnyElementType] = {false};

    cssom::Specificity specificity[kMaxAnyElementType];
    for (cssom::Selectors::const_iterator selector_iterator =
             rule->selectors().begin();
         selector_iterator != rule->selectors().end(); ++selector_iterator) {
      DCHECK(*selector_iterator);
      PseudoElementType pseudo_element_type;
      if (MatchSelectorAndElement(*selector_iterator, element,
                                  &pseudo_element_type)) {
        matches[pseudo_element_type] = true;
        if (specificity[pseudo_element_type] <
            (*selector_iterator)->GetSpecificity()) {
          specificity[pseudo_element_type] =
              (*selector_iterator)->GetSpecificity();
        }
      }
    }

    if (matches[kNotPseudoElementType]) {
      cssom::CascadePriority cascade_priority(
          origin, specificity[kNotPseudoElementType],
          cssom::Appearance(style_sheet->index(), rule->index()));
      element->matching_rules()->push_back(
          std::make_pair(rule, cascade_priority));
    }

    for (int pseudo_element_type = 0;
         pseudo_element_type < kMaxPseudoElementType; ++pseudo_element_type) {
      if (matches[pseudo_element_type]) {
        cssom::CascadePriority cascade_priority(
            origin, specificity[pseudo_element_type],
            cssom::Appearance(style_sheet->index(), rule->index()));
        if (!pseudo_elements[pseudo_element_type]) {
          pseudo_elements[pseudo_element_type].reset(new dom::PseudoElement());
        }
        pseudo_elements[pseudo_element_type]->matching_rules()->push_back(
            std::make_pair(rule, cascade_priority));
      }
    }
  }

  // Add all rules matching normal elements to the pseudo elements with a
  // matching rule, and set the rules on the element.
  for (int i = 0; i < kMaxPseudoElementType; ++i) {
    PseudoElementType pseudo_element_type = static_cast<PseudoElementType>(i);
    if (pseudo_elements[pseudo_element_type]) {
      element->set_pseudo_element(
          pseudo_element_type, pseudo_elements[pseudo_element_type].release());
    }
  }
}

namespace {

bool IsCompoundSelectorAndElementMatching(
    cssom::CompoundSelector* compound_selector, Element* element) {
  DCHECK(compound_selector);
  DCHECK(element);

  return MatchSelectorAndElement(compound_selector, element) != NULL;
}

void AddRulesOnNodeToElement(HTMLElement* element,
                             cssom::SelectorTree::Node* node) {
  cssom::PseudoElement* pseudo_element =
      node->compound_selector->pseudo_element();

  // Where to add matching rules.
  cssom::RulesWithCascadePriority* target_matching_rules;

  if (!pseudo_element) {
    target_matching_rules = element->matching_rules();
  } else {
    PseudoElementType pseudo_element_type = kNotPseudoElementType;

    if (pseudo_element->AsAfterPseudoElement()) {
      pseudo_element_type = kAfterPseudoElementType;
    } else if (pseudo_element->AsBeforePseudoElement()) {
      pseudo_element_type = kBeforePseudoElementType;
    } else {
      NOTREACHED();
    }

    // Make sure the pseudo element exists under element.
    if (!element->pseudo_element(pseudo_element_type)) {
      element->set_pseudo_element(pseudo_element_type,
                                  new dom::PseudoElement());
    }

    target_matching_rules =
        element->pseudo_element(pseudo_element_type)->matching_rules();
  }

  element->rule_matching_state()->matching_nodes.insert(
      std::make_pair(node, base::Unused()));

  for (cssom::SelectorTree::Rules::iterator rule_iterator = node->rules.begin();
       rule_iterator != node->rules.end(); ++rule_iterator) {
    base::WeakPtr<cssom::CSSStyleRule> rule = *rule_iterator;
    if (!rule) {
      continue;
    }
    DCHECK(rule->parent_style_sheet());
    cssom::CascadePriority precedence(
        pseudo_element ? cssom::kNormalOverride
                       : rule->parent_style_sheet()->origin(),
        node->cumulative_specificity,
        cssom::Appearance(rule->parent_style_sheet()->index(), rule->index()));
    target_matching_rules->push_back(std::make_pair(rule.get(), precedence));
  }
}

void GatherCandidateNodeSets(cssom::SelectorTree::SelectorTextToNodesMap* map,
                             const std::string& key,
                             cssom::SelectorTree::NodeSet* matching_nodes) {
  cssom::SelectorTree::SelectorTextToNodesMap::iterator it = map->find(key);
  if (it != map->end()) {
    cssom::SelectorTree::Nodes& nodes = it->second;
    for (cssom::SelectorTree::Nodes::iterator nodes_iterator = nodes.begin();
         nodes_iterator != nodes.end(); ++nodes_iterator) {
      matching_nodes->insert(std::make_pair(*nodes_iterator, base::Unused()));
    }
  }
}

void ForEachChildOnNodes(
    cssom::SelectorTree::NodeSet* node_set,
    cssom::CombinatorType combinator_type, HTMLElement* element,
    base::Callback<void(HTMLElement* element, cssom::SelectorTree::Node*)>
        callback) {
  // Iterate through all nodes in node_set.
  for (cssom::SelectorTree::NodeSet::iterator node_iterator = node_set->begin();
       node_iterator != node_set->end(); ++node_iterator) {
    cssom::SelectorTree::Node* node = node_iterator->first;

    cssom::SelectorTree::NodeSet candidate_nodes;

    // Gather candidate sets in node's children under the given combinator.

    // Type selector.
    GatherCandidateNodeSets(&(node->type_selector_nodes_map[combinator_type]),
                            element->tag_name(), &candidate_nodes);

    // Class selector.
    scoped_refptr<DOMTokenList> class_list = element->class_list();
    for (unsigned int index = 0; index < class_list->length(); ++index) {
      GatherCandidateNodeSets(
          &(node->class_selector_nodes_map[combinator_type]),
          class_list->NonNullItem(index), &candidate_nodes);
    }

    // Id selector.
    GatherCandidateNodeSets(&(node->id_selector_nodes_map[combinator_type]),
                            element->id(), &candidate_nodes);

    // Empty pseudo class.
    if (element->IsEmpty()) {
      cssom::SelectorTree::Nodes& nodes =
          node->empty_pseudo_class_nodes[combinator_type];
      for (cssom::SelectorTree::Nodes::iterator nodes_iterator = nodes.begin();
           nodes_iterator != nodes.end(); ++nodes_iterator) {
        candidate_nodes.insert(std::make_pair(*nodes_iterator, base::Unused()));
      }
    }

    // Check all candidate nodes can run callback for matching nodes.
    for (cssom::SelectorTree::NodeSet::iterator candidate_node_iterator =
             candidate_nodes.begin();
         candidate_node_iterator != candidate_nodes.end();
         ++candidate_node_iterator) {
      cssom::SelectorTree::Node* candidate_node =
          candidate_node_iterator->first;

      if (IsCompoundSelectorAndElementMatching(
              candidate_node->compound_selector, element)) {
        callback.Run(element, candidate_node);
      }
    }
  }
}

void CalculateMatchingRulesRecursively(HTMLElement* current_element,
                                       cssom::SelectorTree* selector_tree) {
  // Clear the node's matching rules and potential node sets.
  current_element->ClearMatchingRules();

  // Get parent and previous sibling of the current element.
  HTMLElement* parent = current_element->parent_element()
                            ? current_element->parent_element()->AsHTMLElement()
                            : NULL;
  HTMLElement* previous_sibling =
      current_element->previous_element_sibling()
          ? current_element->previous_element_sibling()->AsHTMLElement()
          : NULL;

  // Calculate current element's matching nodes.

  if (parent) {
    // Child combinator.
    ForEachChildOnNodes(&parent->rule_matching_state()->matching_nodes,
                        cssom::kChildCombinator, current_element,
                        base::Bind(&AddRulesOnNodeToElement));

    // Descendant combinator.
    ForEachChildOnNodes(
        &parent->rule_matching_state()->descendant_potential_nodes,
        cssom::kDescendantCombinator, current_element,
        base::Bind(&AddRulesOnNodeToElement));
  } else {
    // Descendant combinator from root.
    ForEachChildOnNodes(selector_tree->root_set(), cssom::kDescendantCombinator,
                        current_element, base::Bind(&AddRulesOnNodeToElement));
  }

  if (previous_sibling) {
    // Next sibling combinator.
    ForEachChildOnNodes(
        &previous_sibling->rule_matching_state()->matching_nodes,
        cssom::kNextSiblingCombinator, current_element,
        base::Bind(&AddRulesOnNodeToElement));

    // Following sibling combinator.
    ForEachChildOnNodes(&previous_sibling->rule_matching_state()
                             ->following_sibling_potential_nodes,
                        cssom::kFollowingSiblingCombinator, current_element,
                        base::Bind(&AddRulesOnNodeToElement));
  }

  // Calculate current element's descendant potential nodes.
  if (parent) {
    current_element->rule_matching_state()->descendant_potential_nodes.insert(
        parent->rule_matching_state()->descendant_potential_nodes.begin(),
        parent->rule_matching_state()->descendant_potential_nodes.end());
  } else {
    current_element->rule_matching_state()->descendant_potential_nodes.insert(
        std::make_pair(selector_tree->root(), base::Unused()));
  }
  current_element->rule_matching_state()->descendant_potential_nodes.insert(
      current_element->rule_matching_state()->matching_nodes.begin(),
      current_element->rule_matching_state()->matching_nodes.end());

  // Calculate current element's following sibling potential nodes.
  if (previous_sibling) {
    current_element->rule_matching_state()
        ->following_sibling_potential_nodes.insert(
            previous_sibling->rule_matching_state()
                ->following_sibling_potential_nodes.begin(),
            previous_sibling->rule_matching_state()
                ->following_sibling_potential_nodes.end());
  }
  current_element->rule_matching_state()
      ->following_sibling_potential_nodes.insert(
          current_element->rule_matching_state()->matching_nodes.begin(),
          current_element->rule_matching_state()->matching_nodes.end());

  // Calculate matching rules for current element's children.
  for (Element* element = current_element->first_element_child(); element;
       element = element->next_element_sibling()) {
    HTMLElement* html_element = element->AsHTMLElement();
    DCHECK(html_element);
    CalculateMatchingRulesRecursively(html_element, selector_tree);
  }
}

}  // namespace

void UpdateMatchingRulesUsingSelectorTree(HTMLElement* dom_root,
                                          cssom::SelectorTree* selector_tree) {
  CalculateMatchingRulesRecursively(dom_root, selector_tree);
}

}  // namespace dom
}  // namespace cobalt

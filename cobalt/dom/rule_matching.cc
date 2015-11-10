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
#include "cobalt/cssom/not_pseudo_class.h"
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

// Matches an element against a selector. If the element doesn't match, returns
// NULL, otherwise returns the result of advancing the pointer to the element
// according to the combinators.
Element* MatchSelectorAndElement(cssom::Selector* selector, Element* element);

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
  explicit SelectorMatcher(Element* element) : element_(element) {
    DCHECK(element);
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

  // The class selector represents an element belonging to the class identified
  // by the identifier.
  //   http://www.w3.org/TR/selectors4/#class-selector
  void VisitClassSelector(cssom::ClassSelector* class_selector) OVERRIDE {
    if (!element_->class_list()->ContainsValid(class_selector->class_name())) {
      element_ = NULL;
    }
  }

  // An ID selector represents an element instance that has an identifier that
  // matches the identifier in the ID selector.
  //   http://www.w3.org/TR/selectors4/#id-selector
  void VisitIdSelector(cssom::IdSelector* id_selector) OVERRIDE {
    if (id_selector->id() != element_->id()) {
      element_ = NULL;
    }
  }

  // The :active pseudo-class applies while an element is being activated by the
  // user. For example, between the times the user presses the mouse button and
  // releases it. On systems with more than one mouse button, :active applies
  // only to the primary or primary activation button (typically the "left"
  // mouse button), and any aliases thereof.
  //   http://www.w3.org/TR/selectors4/#active-pseudo
  void VisitActivePseudoClass(cssom::ActivePseudoClass*) OVERRIDE {
    NOTIMPLEMENTED();
    element_ = NULL;
  }

  // The :empty pseudo-class represents an element that has no content children.
  //   http://www.w3.org/TR/selectors4/#empty-pseudo
  void VisitEmptyPseudoClass(cssom::EmptyPseudoClass*) OVERRIDE {
    if (!element_->IsEmpty()) {
      element_ = NULL;
    }
  }

  // The :focus pseudo-class applies while an element has the focus (accepts
  // keyboard or mouse events, or other forms of input).
  //   http://www.w3.org/TR/selectors4/#focus-pseudo
  void VisitFocusPseudoClass(cssom::FocusPseudoClass*) OVERRIDE {
    if (!element_->HasFocus()) {
      element_ = NULL;
    }
  }

  // The :hover pseudo-class applies while the user designates an element with a
  // pointing device, but does not necessarily activate it. For example, a
  // visual user agent could apply this pseudo-class when the cursor (mouse
  // pointer) hovers over a box generated by the element. Interactive user
  // agents that cannot detect hovering due to hardware limitations (e.g., a pen
  // device that does not detect hovering) are still conforming.
  //   http://www.w3.org/TR/selectors4/#hover-pseudo
  void VisitHoverPseudoClass(cssom::HoverPseudoClass*) OVERRIDE {
    NOTIMPLEMENTED();
    element_ = NULL;
  }

  // The negation pseudo-class, :not(), is a functional pseudo-class taking a
  // selector list as an argument. It represents an element that is not
  // represented by its argument.
  //   http://www.w3.org/TR/selectors4/#negation-pseudo
  void VisitNotPseudoClass(cssom::NotPseudoClass* not_pseudo_class) OVERRIDE {
    if (MatchSelectorAndElement(not_pseudo_class->selector(), element_)) {
      element_ = NULL;
    }
  }

  // Unsupported pseudo-class never matches.
  void VisitUnsupportedPseudoClass(cssom::UnsupportedPseudoClass*) OVERRIDE {
    element_ = NULL;
  }

  // Pseudo elements doesn't affect whether the selector matches or not.

  // The :after pseudo-element represents a generated element.
  //   http://www.w3.org/TR/CSS21/generate.html#before-after-content
  void VisitAfterPseudoElement(cssom::AfterPseudoElement*) OVERRIDE {}

  // The :before pseudo-element represents a generated element.
  //   http://www.w3.org/TR/CSS21/generate.html#before-after-content
  void VisitBeforePseudoElement(cssom::BeforePseudoElement*) OVERRIDE {}

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
      element_ = MatchSelectorAndElement(*selector_iterator, element_);
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
    element_ =
        MatchSelectorAndElement(complex_selector->first_selector(), element_);
    if (!element_) {
      return;
    }

    // Iterate through all the combinators and advance the pointer. If any of
    // the combinators doesn't match, the complex selector doesn't match.
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
  DISALLOW_COPY_AND_ASSIGN(SelectorMatcher);
};

//////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////

Element* MatchSelectorAndElement(cssom::Selector* selector, Element* element) {
  DCHECK(selector);
  if (!element) {
    return NULL;
  }
  SelectorMatcher selector_matcher(element);
  selector->Accept(&selector_matcher);
  return selector_matcher.element();
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
      element->set_pseudo_element(
          pseudo_element_type,
          make_scoped_ptr(new dom::PseudoElement(element)));
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

void GatherCandidateNodesFromMap(
    cssom::SelectorTree::SelectorTextToNodesMap* map, const std::string& key,
    cssom::SelectorTree::NodeSet* candidate_nodes) {
  cssom::SelectorTree::SelectorTextToNodesMap::iterator it = map->find(key);
  if (it != map->end()) {
    cssom::SelectorTree::Nodes& nodes = it->second;
    for (cssom::SelectorTree::Nodes::iterator nodes_iterator = nodes.begin();
         nodes_iterator != nodes.end(); ++nodes_iterator) {
      candidate_nodes->insert(std::make_pair(*nodes_iterator, base::Unused()));
    }
  }
}

void GatherCandidateNodesFromSet(
    cssom::SelectorTree::Nodes* nodes,
    cssom::SelectorTree::NodeSet* candidate_nodes) {
  for (cssom::SelectorTree::Nodes::iterator nodes_iterator = nodes->begin();
       nodes_iterator != nodes->end(); ++nodes_iterator) {
    candidate_nodes->insert(std::make_pair(*nodes_iterator, base::Unused()));
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
    GatherCandidateNodesFromMap(
        &(node->type_selector_nodes_map[combinator_type]), element->tag_name(),
        &candidate_nodes);

    // Class selector.
    scoped_refptr<DOMTokenList> class_list = element->class_list();
    for (unsigned int index = 0; index < class_list->length(); ++index) {
      GatherCandidateNodesFromMap(
          &(node->class_selector_nodes_map[combinator_type]),
          class_list->NonNullItem(index), &candidate_nodes);
    }

    // Id selector.
    GatherCandidateNodesFromMap(&(node->id_selector_nodes_map[combinator_type]),
                                element->id(), &candidate_nodes);

    // Empty pseudo class.
    if (element->IsEmpty()) {
      GatherCandidateNodesFromSet(
          &(node->empty_pseudo_class_nodes[combinator_type]), &candidate_nodes);
    }

    // Focus pseudo class.
    if (element->HasFocus()) {
      GatherCandidateNodesFromSet(
          &(node->focus_pseudo_class_nodes[combinator_type]), &candidate_nodes);
    }

    // Not pseudo class.
    GatherCandidateNodesFromSet(
        &(node->not_pseudo_class_nodes[combinator_type]), &candidate_nodes);

    // Check all candidate nodes and run callback for matching nodes.
    for (cssom::SelectorTree::NodeSet::iterator candidate_node_iterator =
             candidate_nodes.begin();
         candidate_node_iterator != candidate_nodes.end();
         ++candidate_node_iterator) {
      cssom::SelectorTree::Node* candidate_node =
          candidate_node_iterator->first;

      if (MatchSelectorAndElement(candidate_node->compound_selector, element)) {
        callback.Run(element, candidate_node);
      }
    }
  }
}

void CalculateMatchingRules(HTMLElement* current_element,
                            cssom::SelectorTree* selector_tree) {
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

  if (selector_tree->has_sibling_combinators()) {
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
  if (selector_tree->has_sibling_combinators()) {
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
  }

  // Set the valid flag on the element.
  current_element->SetMatchingRulesValid();
}

void CalculateMatchingRulesRecursively(HTMLElement* current_element,
                                       cssom::SelectorTree* selector_tree) {
  if (!current_element->matching_rules_valid()) {
    CalculateMatchingRules(current_element, selector_tree);
  }

  // Calculate matching rules for current element's children.
  for (Element* element = current_element->first_element_child(); element;
       element = element->next_element_sibling()) {
    HTMLElement* html_element = element->AsHTMLElement();
    DCHECK(html_element);
    CalculateMatchingRulesRecursively(html_element, selector_tree);
  }
}

}  // namespace

//////////////////////////////////////////////////////////////////////////
// Public APIs
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

void UpdateMatchingRulesUsingSelectorTree(HTMLElement* dom_root,
                                          cssom::SelectorTree* selector_tree) {
  CalculateMatchingRulesRecursively(dom_root, selector_tree);
}

}  // namespace dom
}  // namespace cobalt

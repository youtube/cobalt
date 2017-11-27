// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/rule_matching.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "base/string_util.h"
#include "cobalt/base/unused.h"
#include "cobalt/cssom/after_pseudo_element.h"
#include "cobalt/cssom/attribute_selector.h"
#include "cobalt/cssom/before_pseudo_element.h"
#include "cobalt/cssom/cascade_precedence.h"
#include "cobalt/cssom/child_combinator.h"
#include "cobalt/cssom/class_selector.h"
#include "cobalt/cssom/combinator.h"
#include "cobalt/cssom/combinator_visitor.h"
#include "cobalt/cssom/complex_selector.h"
#include "cobalt/cssom/compound_selector.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/descendant_combinator.h"
#include "cobalt/cssom/empty_pseudo_class.h"
#include "cobalt/cssom/following_sibling_combinator.h"
#include "cobalt/cssom/id_selector.h"
#include "cobalt/cssom/next_sibling_combinator.h"
#include "cobalt/cssom/not_pseudo_class.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/pseudo_element.h"
#include "cobalt/cssom/selector_tree.h"
#include "cobalt/cssom/selector_visitor.h"
#include "cobalt/cssom/simple_selector.h"
#include "cobalt/cssom/type_selector.h"
#include "cobalt/cssom/universal_selector.h"
#include "cobalt/dom/attr.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_token_list.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/node_descendants_iterator.h"
#include "cobalt/dom/node_list.h"
#include "cobalt/dom/pseudo_element.h"
#include "cobalt/math/safe_integer_conversions.h"

namespace cobalt {
namespace dom {
namespace {

using cssom::SelectorTree;
const size_t kRuleMatchingNodeSetSize = 60;

//////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////

// Matches an element against a selector. If the element doesn't match, returns
// NULL, otherwise returns the result of advancing the pointer to the element
// according to the combinators.
Element* MatchSelectorAndElement(cssom::Selector* selector, Element* element,
                                 bool matching_combinators);

//////////////////////////////////////////////////////////////////////////
// Combinator matcher
//////////////////////////////////////////////////////////////////////////

class CombinatorMatcher : public cssom::CombinatorVisitor {
 public:
  explicit CombinatorMatcher(Element* element) : element_(element) {
    DCHECK(element);
  }

  // Child combinator describes a childhood relationship between two elements.
  //   https://www.w3.org/TR/selectors4/#child-combinators
  void VisitChildCombinator(cssom::ChildCombinator* child_combinator) override {
    element_ = MatchSelectorAndElement(child_combinator->left_selector(),
                                       element_->parent_element(), true);
  }

  // Next-sibling combinator describes that the elements represented by the two
  // compound selectors share the same parent in the document tree and the
  // element represented by the first compound selector immediately precedes the
  // element represented by the second one.
  //   https://www.w3.org/TR/selectors4/#adjacent-sibling-combinators
  void VisitNextSiblingCombinator(
      cssom::NextSiblingCombinator* next_sibling_combinator) override {
    element_ =
        MatchSelectorAndElement(next_sibling_combinator->left_selector(),
                                element_->previous_element_sibling(), true);
  }

  // Descendant combinator describes an element that is the descendant of
  // another element in the document tree.
  //   https://www.w3.org/TR/selectors4/#descendant-combinators
  void VisitDescendantCombinator(
      cssom::DescendantCombinator* descendant_combinator) override {
    do {
      element_ = element_->parent_element();
      Element* element = MatchSelectorAndElement(
          descendant_combinator->left_selector(), element_, true);
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
  //   https://www.w3.org/TR/selectors4/#general-sibling-combinators
  void VisitFollowingSiblingCombinator(
      cssom::FollowingSiblingCombinator* following_sibling_combinator)
      override {
    do {
      element_ = element_->previous_element_sibling();
      Element* element = MatchSelectorAndElement(
          following_sibling_combinator->left_selector(), element_, true);
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
  explicit SelectorMatcher(Element* element)
      : element_(element), matching_combinators_(false) {
    DCHECK(element);
  }
  SelectorMatcher(Element* element, bool matching_combinators)
      : element_(element), matching_combinators_(matching_combinators) {
    DCHECK(element);
  }

  // The universal selector represents the qualified name of any element type.
  //   https://www.w3.org/TR/selectors4/#universal-selector
  void VisitUniversalSelector(
      cssom::UniversalSelector* /* universal_selector */) override {}

  // A type selector represents an instance of the element type in the document
  // tree.
  //   https://www.w3.org/TR/selectors4/#type-selector
  void VisitTypeSelector(cssom::TypeSelector* type_selector) override {
    if (type_selector->element_name() != element_->local_name()) {
      element_ = NULL;
    }
  }

  // An attribute selector represents an element that has an attribute that
  // matches the attribute represented by the attribute selector.
  //   https://www.w3.org/TR/selectors4/#attribute-selector
  void VisitAttributeSelector(
      cssom::AttributeSelector* attribute_selector) override {
    if (!element_->HasAttribute(attribute_selector->attribute_name().c_str())) {
      element_ = NULL;
      return;
    }

    switch (attribute_selector->value_match_type()) {
      case cssom::AttributeSelector::kNoMatch:
        return;
      case cssom::AttributeSelector::kEquals:
        if (element_->GetAttribute(attribute_selector->attribute_name().c_str())
                .value() != attribute_selector->attribute_value()) {
          element_ = NULL;
        }
        return;
      case cssom::AttributeSelector::kIncludes:
      case cssom::AttributeSelector::kDashMatch:
      case cssom::AttributeSelector::kBeginsWith:
      case cssom::AttributeSelector::kEndsWith:
      case cssom::AttributeSelector::kContains:
        NOTIMPLEMENTED();
        element_ = NULL;
        return;
    }
  }

  // The class selector represents an element belonging to the class identified
  // by the identifier.
  //   https://www.w3.org/TR/selectors4/#class-selector
  void VisitClassSelector(cssom::ClassSelector* class_selector) override {
    if (!element_->class_list()->ContainsValid(class_selector->class_name())) {
      element_ = NULL;
    }
  }

  // An ID selector represents an element instance that has an identifier that
  // matches the identifier in the ID selector.
  //   https://www.w3.org/TR/selectors4/#id-selector
  void VisitIdSelector(cssom::IdSelector* id_selector) override {
    if (id_selector->id() != element_->id()) {
      element_ = NULL;
    }
  }

  // The :active pseudo-class applies while an element is being activated by the
  // user. For example, between the times the user presses the mouse button and
  // releases it. On systems with more than one mouse button, :active applies
  // only to the primary or primary activation button (typically the "left"
  // mouse button), and any aliases thereof.
  //   https://www.w3.org/TR/selectors4/#active-pseudo
  void VisitActivePseudoClass(cssom::ActivePseudoClass*) override {
    NOTIMPLEMENTED();
    element_ = NULL;
  }

  // The :empty pseudo-class represents an element that has no content children.
  //   https://www.w3.org/TR/selectors4/#empty-pseudo
  void VisitEmptyPseudoClass(cssom::EmptyPseudoClass*) override {
    if (!element_->IsEmpty()) {
      element_ = NULL;
    }
  }

  // The :focus pseudo-class applies while an element has the focus (accepts
  // keyboard or mouse events, or other forms of input).
  //   https://www.w3.org/TR/selectors4/#focus-pseudo
  void VisitFocusPseudoClass(cssom::FocusPseudoClass*) override {
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
  //   https://www.w3.org/TR/selectors4/#hover-pseudo
  void VisitHoverPseudoClass(cssom::HoverPseudoClass*) override {
    if (!element_->AsHTMLElement() ||
        !element_->AsHTMLElement()->IsDesignated()) {
      element_ = NULL;
    }
  }

  // The negation pseudo-class, :not(), is a functional pseudo-class taking a
  // selector list as an argument. It represents an element that is not
  // represented by its argument.
  //   https://www.w3.org/TR/selectors4/#negation-pseudo
  void VisitNotPseudoClass(cssom::NotPseudoClass* not_pseudo_class) override {
    if (MatchSelectorAndElement(not_pseudo_class->selector(), element_, true)) {
      element_ = NULL;
    }
  }

  // Pseudo elements doesn't affect whether the selector matches or not.

  // The :after pseudo-element represents a generated element.
  //   https://www.w3.org/TR/CSS21/generate.html#before-after-content
  void VisitAfterPseudoElement(cssom::AfterPseudoElement*) override {}

  // The :before pseudo-element represents a generated element.
  //   https://www.w3.org/TR/CSS21/generate.html#before-after-content
  void VisitBeforePseudoElement(cssom::BeforePseudoElement*) override {}

  // A compound selector is a chain of simple selectors that are not separated
  // by a combinator.
  //   https://www.w3.org/TR/selectors4/#compound
  void VisitCompoundSelector(
      cssom::CompoundSelector* compound_selector) override {
    DCHECK_GT(compound_selector->simple_selectors().size(), 0U);

    // Iterate through all the simple selectors. If any of the simple selectors
    // doesn't match, the compound selector doesn't match.
    for (cssom::CompoundSelector::SimpleSelectors::const_iterator
             selector_iterator = compound_selector->simple_selectors().begin();
         selector_iterator != compound_selector->simple_selectors().end();
         ++selector_iterator) {
      element_ = MatchSelectorAndElement(*selector_iterator, element_, false);
      if (!element_) {
        return;
      }
    }

    // Check combinator.
    if (matching_combinators_ && compound_selector->left_combinator()) {
      CombinatorMatcher combinator_matcher(element_);
      compound_selector->left_combinator()->Accept(&combinator_matcher);
      element_ = combinator_matcher.element();
    }
  }

  // A complex selector is a chain of one or more compound selectors separated
  // by combinators.
  //   https://www.w3.org/TR/selectors4/#complex
  void VisitComplexSelector(cssom::ComplexSelector* complex_selector) override {
    element_ = MatchSelectorAndElement(complex_selector->last_selector(),
                                       element_, true);
  }

  Element* element() const { return element_; }

 private:
  Element* element_;
  bool matching_combinators_;
  DISALLOW_COPY_AND_ASSIGN(SelectorMatcher);
};

//////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////

Element* MatchSelectorAndElement(cssom::Selector* selector, Element* element,
                                 bool matching_combinators) {
  DCHECK(selector);
  if (!element) {
    return NULL;
  }

  SelectorMatcher selector_matcher(element, matching_combinators);
  selector->Accept(&selector_matcher);
  return selector_matcher.element();
}

void AddRulesOnNodeToElement(HTMLElement* element,
                             const SelectorTree::Node* node) {
  cssom::PseudoElement* pseudo_element =
      node->compound_selector()->pseudo_element();

  // Where to add matching rules.
  cssom::RulesWithCascadePrecedence* target_matching_rules;

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

  element->rule_matching_state()->matching_nodes.insert(node);

  for (SelectorTree::Rules::const_iterator rule_iterator =
           node->rules().begin();
       rule_iterator != node->rules().end(); ++rule_iterator) {
    cssom::CSSStyleRule* rule = *rule_iterator;
    if (!rule) {
      continue;
    }
    DCHECK(rule->parent_style_sheet());
    cssom::CascadePrecedence precedence(
        pseudo_element ? cssom::kNormalOverride
                       : rule->parent_style_sheet()->origin(),
        node->cumulative_specificity(),
        cssom::Appearance(rule->parent_style_sheet()->index(), rule->index()));
    target_matching_rules->push_back(std::make_pair(rule, precedence));
  }
}

void GatherCandidateNodesFromMap(
    cssom::SimpleSelectorType simple_selector_type,
    cssom::CombinatorType combinator_type,
    const SelectorTree::SelectorTextToNodesMap* map, base::Token key,
    SelectorTree::NodeSet<kRuleMatchingNodeSetSize>* candidate_nodes) {
  SelectorTree::SelectorTextToNodesMap::const_iterator it = map->find(key);
  if (it != map->end()) {
    const SelectorTree::SimpleSelectorNodes& nodes = it->second;
    for (SelectorTree::SimpleSelectorNodes::const_iterator nodes_iterator =
             nodes.begin();
         nodes_iterator != nodes.end(); ++nodes_iterator) {
      if (nodes_iterator->simple_selector_type == simple_selector_type &&
          nodes_iterator->combinator_type == combinator_type) {
        candidate_nodes->insert(nodes_iterator->node);
      }
    }
  }
}

void GatherCandidateNodesFromSet(
    cssom::PseudoClassType pseudo_class_type,
    cssom::CombinatorType combinator_type,
    const SelectorTree::PseudoClassNodes& pseudo_class_nodes,
    SelectorTree::NodeSet<kRuleMatchingNodeSetSize>* candidate_nodes) {
  for (SelectorTree::PseudoClassNodes::const_iterator nodes_iterator =
           pseudo_class_nodes.begin();
       nodes_iterator != pseudo_class_nodes.end(); ++nodes_iterator) {
    if (nodes_iterator->pseudo_class_type == pseudo_class_type &&
        nodes_iterator->combinator_type == combinator_type) {
      candidate_nodes->insert(nodes_iterator->node);
    }
  }
}

template <class NodeSet>
void ForEachChildOnNodes(
    const NodeSet& node_set, cssom::CombinatorType combinator_type,
    HTMLElement* element,
    base::Callback<void(HTMLElement* element, const SelectorTree::Node*)>
        callback) {
  // Gathering Phase: Generate candidate nodes from the node set.
  SelectorTree::NodeSet<kRuleMatchingNodeSetSize> candidate_nodes;

  const std::vector<base::Token>& element_class_list =
      element->class_list()->GetTokens();

  // Don't retrieve the element's attributes until they are needed. Retrieving
  // them typically requires the creation of a NamedNodeMap.
  scoped_refptr<NamedNodeMap> element_attributes;

  // Iterate through all nodes in node_set.
  for (typename NodeSet::const_iterator node_iterator = node_set.begin();
       node_iterator != node_set.end(); ++node_iterator) {
    const SelectorTree::Node* node = *node_iterator;

    if (!node->HasCombinator(combinator_type)) {
      continue;
    }

    // Gather candidate sets in node's children under the given combinator.

    const SelectorTree::SelectorTextToNodesMap* selector_nodes_map =
        node->selector_nodes_map();
    if (selector_nodes_map) {
      // Universal selector.
      if (node->HasSimpleSelector(cssom::kUniversalSelector, combinator_type)) {
        GatherCandidateNodesFromMap(cssom::kUniversalSelector, combinator_type,
                                    selector_nodes_map, base::Token(),
                                    &candidate_nodes);
      }

      // Type selector.
      if (node->HasSimpleSelector(cssom::kTypeSelector, combinator_type)) {
        GatherCandidateNodesFromMap(cssom::kTypeSelector, combinator_type,
                                    selector_nodes_map, element->local_name(),
                                    &candidate_nodes);
      }

      // Attribute selector.
      if (node->HasSimpleSelector(cssom::kAttributeSelector, combinator_type)) {
        // If the element's attributes have not been retrieved yet, then
        // retrieve them now.
        if (!element_attributes) {
          element_attributes = element->attributes();
        }

        for (unsigned int index = 0; index < element_attributes->length();
             ++index) {
          GatherCandidateNodesFromMap(
              cssom::kAttributeSelector, combinator_type, selector_nodes_map,
              base::Token(element_attributes->Item(index)->name()),
              &candidate_nodes);
        }
      }

      // Class selector.
      if (node->HasSimpleSelector(cssom::kClassSelector, combinator_type)) {
        for (size_t index = 0; index < element_class_list.size(); ++index) {
          GatherCandidateNodesFromMap(
              cssom::kClassSelector, combinator_type, selector_nodes_map,
              element_class_list[index], &candidate_nodes);
        }
      }

      // Id selector.
      if (node->HasSimpleSelector(cssom::kIdSelector, combinator_type)) {
        GatherCandidateNodesFromMap(cssom::kIdSelector, combinator_type,
                                    selector_nodes_map, element->id(),
                                    &candidate_nodes);
      }
    }

    if (node->HasAnyPseudoClass()) {
      // Empty pseudo class.
      if (node->HasPseudoClass(cssom::kEmptyPseudoClass, combinator_type) &&
          element->IsEmpty()) {
        GatherCandidateNodesFromSet(cssom::kEmptyPseudoClass, combinator_type,
                                    node->pseudo_class_nodes(),
                                    &candidate_nodes);
      }

      // Focus pseudo class.
      if (node->HasPseudoClass(cssom::kFocusPseudoClass, combinator_type) &&
          element->HasFocus()) {
        GatherCandidateNodesFromSet(cssom::kFocusPseudoClass, combinator_type,
                                    node->pseudo_class_nodes(),
                                    &candidate_nodes);
      }

      // Hover pseudo class.
      if (node->HasPseudoClass(cssom::kHoverPseudoClass, combinator_type) &&
          element->IsDesignated()) {
        GatherCandidateNodesFromSet(cssom::kHoverPseudoClass, combinator_type,
                                    node->pseudo_class_nodes(),
                                    &candidate_nodes);
      }

      // Not pseudo class.
      if (node->HasPseudoClass(cssom::kNotPseudoClass, combinator_type)) {
        GatherCandidateNodesFromSet(cssom::kNotPseudoClass, combinator_type,
                                    node->pseudo_class_nodes(),
                                    &candidate_nodes);
      }
    }
  }

  // Verifying Phase: Check all candidate nodes and run callback for matching
  // nodes.
  for (SelectorTree::NodeSet<kRuleMatchingNodeSetSize>::const_iterator
           candidate_node_iterator = candidate_nodes.begin();
       candidate_node_iterator != candidate_nodes.end();
       ++candidate_node_iterator) {
    const SelectorTree::Node* candidate_node = *candidate_node_iterator;

    // Verify that this node is a match:
    // 1. If the candidate node's compound selector doesn't require a visit to
    //    verify the match, then the act of gathering the node as a candidate
    //    proved the match.
    // 2. Otherwise, the node requires additional verification checks, so call
    //    MatchSelectorAndElement().
    if (!candidate_node->compound_selector()
             ->requires_rule_matching_verification_visit() ||
        MatchSelectorAndElement(candidate_node->compound_selector(), element,
                                false)) {
      callback.Run(element, candidate_node);
    }
  }
}

bool MatchRuleAndElement(cssom::CSSStyleRule* rule, Element* element) {
  for (cssom::Selectors::const_iterator selector_iterator =
           rule->selectors().begin();
       selector_iterator != rule->selectors().end(); ++selector_iterator) {
    DCHECK(*selector_iterator);
    if (MatchSelectorAndElement(*selector_iterator, element, true)) {
      return true;
    }
  }
  return false;
}

}  // namespace

//////////////////////////////////////////////////////////////////////////
// Public APIs
//////////////////////////////////////////////////////////////////////////

void UpdateMatchingRules(HTMLElement* current_element) {
  DCHECK(current_element);
  DCHECK(current_element->node_document());
  SelectorTree* selector_tree =
      current_element->node_document()->selector_tree();

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
    ForEachChildOnNodes(parent->rule_matching_state()->matching_nodes,
                        cssom::kChildCombinator, current_element,
                        base::Bind(&AddRulesOnNodeToElement));

    // Descendant combinator.
    ForEachChildOnNodes(
        parent->rule_matching_state()->descendant_potential_nodes,
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
          previous_sibling->rule_matching_state()->matching_nodes,
          cssom::kNextSiblingCombinator, current_element,
          base::Bind(&AddRulesOnNodeToElement));

      // Following sibling combinator.
      ForEachChildOnNodes(previous_sibling->rule_matching_state()
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
        selector_tree->root());
  }
  // Walk all of the matching nodes, adding any that have a descendant
  // combinator as a descendant potential node. Any missing that combinator can
  // never match descendants.
  for (dom::HTMLElement::MatchingNodes::const_iterator iter =
           current_element->rule_matching_state()->matching_nodes.begin();
       iter != current_element->rule_matching_state()->matching_nodes.end();
       ++iter) {
    if ((*iter)->HasCombinator(cssom::kDescendantCombinator)) {
      // It is possible for the two lists to contain duplicate nodes, so only
      // add the node if it isn't a duplicate.
      current_element->rule_matching_state()->descendant_potential_nodes.insert(
          *iter, true /*check_for_duplicate*/);
    }
  }

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
    // Walk all of the matching nodes, adding any that have a following sibling
    // combinator as a following sibling potential node. Any missing that
    // combinator can never match following siblings..
    for (dom::HTMLElement::MatchingNodes::const_iterator iter =
             current_element->rule_matching_state()->matching_nodes.begin();
         iter != current_element->rule_matching_state()->matching_nodes.end();
         ++iter) {
      if ((*iter)->HasCombinator(cssom::kFollowingSiblingCombinator)) {
        // It is possible for the two lists to contain duplicate nodes, so only
        // add the node if it isn't a duplicate.
        current_element->rule_matching_state()
            ->following_sibling_potential_nodes.insert(
                *iter, true /*check_for_duplicate*/);
      }
    }
  }

  current_element->set_matching_rules_valid();
}

scoped_refptr<Element> QuerySelector(Node* node, const std::string& selectors,
                                     cssom::CSSParser* css_parser) {
  DCHECK(css_parser);

  // Generate a rule with the given selectors and no style.
  scoped_refptr<cssom::CSSRule> css_rule =
      css_parser->ParseRule(selectors + " {}", node->GetInlineSourceLocation());
  if (!css_rule) {
    return NULL;
  }
  scoped_refptr<cssom::CSSStyleRule> css_style_rule =
      css_rule->AsCSSStyleRule();
  if (!css_style_rule) {
    return NULL;
  }

  // Iterate through the descendants of the node and find the first matching
  // element if any.
  NodeDescendantsIterator iterator(node);
  Node* child = iterator.First();
  while (child) {
    if (child->IsElement()) {
      scoped_refptr<Element> element = child->AsElement();
      if (MatchRuleAndElement(css_style_rule, element)) {
        return element;
      }
    }
    child = iterator.Next();
  }
  return NULL;
}

scoped_refptr<NodeList> QuerySelectorAll(Node* node,
                                         const std::string& selectors,
                                         cssom::CSSParser* css_parser) {
  DCHECK(css_parser);

  scoped_refptr<NodeList> node_list = new NodeList();

  // Generate a rule with the given selectors and no style.
  scoped_refptr<cssom::CSSRule> css_rule =
      css_parser->ParseRule(selectors + " {}", node->GetInlineSourceLocation());
  if (!css_rule) {
    return node_list;
  }
  scoped_refptr<cssom::CSSStyleRule> css_style_rule =
      css_rule->AsCSSStyleRule();
  if (!css_style_rule) {
    return node_list;
  }

  // Iterate through the descendants of the node and find the matching elements.
  NodeDescendantsIterator iterator(node);
  Node* child = iterator.First();
  while (child) {
    if (child->IsElement()) {
      scoped_refptr<Element> element = child->AsElement();
      if (MatchRuleAndElement(css_style_rule, element)) {
        node_list->AppendNode(element);
      }
    }
    child = iterator.Next();
  }
  return node_list;
}

}  // namespace dom
}  // namespace cobalt

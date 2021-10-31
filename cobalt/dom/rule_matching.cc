// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
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

struct NodeCombinatorType {
  NodeCombinatorType(const SelectorTree::Node* node,
                     cssom::CombinatorType combinator_type)
      : node(node), combinator_type(combinator_type) {}

  const SelectorTree::Node* node;
  cssom::CombinatorType combinator_type;
};

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
      cssom::UniversalSelector* universal_selector) override {}

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
      element_ =
          MatchSelectorAndElement(selector_iterator->get(), element_, false);
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

void GatherCandidateNodesFromSelectorNodesMap(
    cssom::SimpleSelectorType simple_selector_type,
    cssom::CombinatorType combinator_type,
    const SelectorTree::Node* parent_node,
    const SelectorTree::SelectorTextToNodesMap* map, base::Token key,
    SelectorTree::NodePairs* candidate_nodes) {
  SelectorTree::SelectorTextToNodesMap::const_iterator it = map->find(key);
  if (it != map->end()) {
    const SelectorTree::SimpleSelectorNodes& nodes = it->second;
    for (const auto& nodes_iterator : nodes) {
      if (nodes_iterator.simple_selector_type == simple_selector_type &&
          nodes_iterator.combinator_type == combinator_type) {
        candidate_nodes->emplace_back(parent_node, nodes_iterator.node);
      }
    }
  }
}

void GatherCandidateNodesFromPseudoClassNodes(
    cssom::PseudoClassType pseudo_class_type,
    cssom::CombinatorType combinator_type,
    const SelectorTree::Node* parent_node,
    const SelectorTree::PseudoClassNodes& pseudo_class_nodes,
    SelectorTree::NodePairs* candidate_nodes) {
  for (const auto& nodes_iterator : pseudo_class_nodes) {
    if (nodes_iterator.pseudo_class_type == pseudo_class_type &&
        nodes_iterator.combinator_type == combinator_type) {
      candidate_nodes->emplace_back(parent_node, nodes_iterator.node);
    }
  }
}

bool GatherNodeModificationsAndUpdateTargetNodes(
    const SelectorTree::Nodes& source_nodes, SelectorTree::Nodes* target_nodes,
    SelectorTree::Nodes* added_nodes, SelectorTree::Nodes* removed_nodes) {
  if (source_nodes == *target_nodes) {
    return false;
  }

  added_nodes->clear();
  removed_nodes->clear();

  if (target_nodes->empty()) {
    // If the previous state of the nodes (|target_nodes|) is empty, then all
    // nodes in the new state (|source_nodes|) are being added.
    *added_nodes = source_nodes;
  } else if (source_nodes.empty()) {
    // If the new state (|source_nodes|) is empty, then all nodes in the
    // previous state (|target_nodes|) are being removed.
    *removed_nodes = *target_nodes;
  } else {
    // If neither the previous state nor the new state or empty, then the lists
    // must be manually searched to determine what is being added and removed.
    // Remove all nodes in |source_nodes| from |target_nodes|. Any of the nodes
    // that are not found in |target_nodes| are newly added nodes.
    size_t start_index = 0;
    for (const auto& check_node : source_nodes) {
      bool node_found = false;
      for (size_t index = start_index; index < target_nodes->size(); ++index) {
        if ((*target_nodes)[index] == check_node) {
          if (index == start_index) {
            ++start_index;
          }
          node_found = true;
          (*target_nodes)[index] = NULL;
          break;
        }
      }
      if (!node_found) {
        added_nodes->push_back(check_node);
      }
    }
    // Find nodes that remain in |target_nodes|. These did not exist in
    // |source_nodes| and are newly removed nodes.
    for (const auto& check_node : *target_nodes) {
      if (check_node != NULL) {
        removed_nodes->push_back(check_node);
      }
    }
  }

  *target_nodes = source_nodes;

  // Return whether or not any modifications were found.
  return !added_nodes->empty() || !removed_nodes->empty();
}

// Returns true if a matching node was added.
bool GatherMatchingNodes(const SelectorTree::Nodes& nodes,
                         cssom::CombinatorType combinator_type,
                         HTMLElement* element,
                         SelectorTree::NodePairs* scratchpad_node_pairs) {
  bool matching_node_added = false;

  // Gathering Phase: Generate candidate nodes from the nodes.
  SelectorTree::NodePairs* candidate_node_pairs = scratchpad_node_pairs;
  candidate_node_pairs->clear();

  const std::vector<base::Token>& element_class_list =
      element->class_list()->GetTokens();

  // Don't retrieve the element's attributes until they are needed. Retrieving
  // them typically requires the creation of a NamedNodeMap.
  scoped_refptr<NamedNodeMap> element_attributes;

  // Iterate through all nodes.
  for (const auto& node : nodes) {
    if (!node->HasCombinator(combinator_type)) {
      continue;
    }

    // Gather candidate nodes in node's children under the given combinator.
    const SelectorTree::SelectorTextToNodesMap* selector_nodes_map =
        node->selector_nodes_map();
    if (selector_nodes_map) {
      // Universal selector.
      if (node->HasSimpleSelector(cssom::kUniversalSelector, combinator_type)) {
        GatherCandidateNodesFromSelectorNodesMap(
            cssom::kUniversalSelector, combinator_type, node,
            selector_nodes_map, base::Token(), candidate_node_pairs);
      }

      // Type selector.
      if (node->HasSimpleSelector(cssom::kTypeSelector, combinator_type)) {
        GatherCandidateNodesFromSelectorNodesMap(
            cssom::kTypeSelector, combinator_type, node, selector_nodes_map,
            element->local_name(), candidate_node_pairs);
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
          GatherCandidateNodesFromSelectorNodesMap(
              cssom::kAttributeSelector, combinator_type, node,
              selector_nodes_map,
              base::Token(element_attributes->Item(index)->name()),
              candidate_node_pairs);
        }
      }

      // Class selector.
      if (node->HasSimpleSelector(cssom::kClassSelector, combinator_type)) {
        for (const auto& element_class : element_class_list) {
          GatherCandidateNodesFromSelectorNodesMap(
              cssom::kClassSelector, combinator_type, node, selector_nodes_map,
              element_class, candidate_node_pairs);
        }
      }

      // Id selector.
      if (node->HasSimpleSelector(cssom::kIdSelector, combinator_type)) {
        GatherCandidateNodesFromSelectorNodesMap(
            cssom::kIdSelector, combinator_type, node, selector_nodes_map,
            element->id(), candidate_node_pairs);
      }
    }

    // Only check for specific pseudo classes when the node has as least one.
    if (node->HasAnyPseudoClass()) {
      // Empty pseudo class.
      if (node->HasPseudoClass(cssom::kEmptyPseudoClass, combinator_type) &&
          element->IsEmpty()) {
        GatherCandidateNodesFromPseudoClassNodes(
            cssom::kEmptyPseudoClass, combinator_type, node,
            node->pseudo_class_nodes(), candidate_node_pairs);
      }

      // Focus pseudo class.
      if (node->HasPseudoClass(cssom::kFocusPseudoClass, combinator_type) &&
          element->HasFocus()) {
        GatherCandidateNodesFromPseudoClassNodes(
            cssom::kFocusPseudoClass, combinator_type, node,
            node->pseudo_class_nodes(), candidate_node_pairs);
      }

      // Hover pseudo class.
      if (node->HasPseudoClass(cssom::kHoverPseudoClass, combinator_type) &&
          element->IsDesignated()) {
        GatherCandidateNodesFromPseudoClassNodes(
            cssom::kHoverPseudoClass, combinator_type, node,
            node->pseudo_class_nodes(), candidate_node_pairs);
      }

      // Not pseudo class.
      if (node->HasPseudoClass(cssom::kNotPseudoClass, combinator_type)) {
        GatherCandidateNodesFromPseudoClassNodes(
            cssom::kNotPseudoClass, combinator_type, node,
            node->pseudo_class_nodes(), candidate_node_pairs);
      }
    }
  }

  // Verifying Phase: Check all candidate nodes and run callback for matching
  // nodes.
  for (const auto& candidate_node_pair : *candidate_node_pairs) {
    const SelectorTree::Node* candidate_node = candidate_node_pair.second;

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
      matching_node_added = true;
      element->rule_matching_state()->matching_nodes_parent_nodes.push_back(
          candidate_node_pair.first);
      element->rule_matching_state()->matching_nodes.push_back(candidate_node);
    }
  }

  return matching_node_added;
}

// Returns true if a matching node was removed.
bool RemoveNodesFromMatchingNodes(
    const SelectorTree::Nodes& parent_nodes_to_remove,
    SelectorTree::Nodes* parent_nodes, SelectorTree::Nodes* matching_nodes) {
  DCHECK_EQ(parent_nodes->size(), matching_nodes->size());
  bool node_removed = false;

  // Find any |parent_nodes| that are being removed. Remove these from both
  // |parent_nodes| and |matching_nodes|, as these two containers are kept in
  // sync.
  for (const auto& parent_node_to_remove : parent_nodes_to_remove) {
    for (size_t index = 0; index < parent_nodes->size();) {
      if (parent_node_to_remove == (*parent_nodes)[index]) {
        node_removed = true;
        parent_nodes->erase(parent_nodes->begin() + index);
        matching_nodes->erase(matching_nodes->begin() + index);
      } else {
        ++index;
      }
    }
  }

  return node_removed;
}

// Returns true if the matching nodes were modified.
bool UpdateMatchingNodesFromNodeModifications(
    const SelectorTree::Nodes& added_nodes,
    const SelectorTree::Nodes& removed_nodes,
    cssom::CombinatorType combinator_type, HTMLElement* element,
    SelectorTree::NodePairs* scratchpad_node_pairs) {
  // First gather the matching nodes found in |added_nodes|.
  bool matching_nodes_modified = GatherMatchingNodes(
      added_nodes, combinator_type, element, scratchpad_node_pairs);

  // Now remove any matching nodes found in |removed_nodes|.
  HTMLElement::RuleMatchingState* element_matching_state =
      element->rule_matching_state();
  matching_nodes_modified |= RemoveNodesFromMatchingNodes(
      removed_nodes, &element_matching_state->matching_nodes_parent_nodes,
      &element_matching_state->matching_nodes);

  return matching_nodes_modified;
}

void UpdateRuleMatchingStateCombinatorNodes(
    HTMLElement::RuleMatchingState* matching_state,
    cssom::CombinatorType combinator_type) {
  // Only descendant and following sibling combinators are supported.
  DCHECK(combinator_type == cssom::kDescendantCombinator ||
         combinator_type == cssom::kFollowingSiblingCombinator);
  if (!matching_state) {
    return;
  }

  DCHECK(matching_state->is_set);
  bool& is_dirty = combinator_type == cssom::kDescendantCombinator
                       ? matching_state->are_descendant_nodes_dirty
                       : matching_state->are_following_sibling_nodes_dirty;
  if (!is_dirty) {
    return;
  }

  const SelectorTree::Nodes& base_combinator_nodes =
      combinator_type == cssom::kDescendantCombinator
          ? matching_state->parent_descendant_nodes
          : matching_state->previous_sibling_following_sibling_nodes;
  SelectorTree::Nodes& combinator_nodes =
      combinator_type == cssom::kDescendantCombinator
          ? matching_state->descendant_nodes
          : matching_state->following_sibling_nodes;

  // First copy the base combinator nodes and then add any matching nodes that
  // have the required combinator and are not already included within the list.
  combinator_nodes = base_combinator_nodes;
  for (const auto& node : matching_state->matching_nodes) {
    if (node->HasCombinator(combinator_type) &&
        std::find(base_combinator_nodes.begin(), base_combinator_nodes.end(),
                  node) == base_combinator_nodes.end()) {
      combinator_nodes.push_back(node);
    }
  }

  is_dirty = false;
}

// Returns true if the matching state was modified.
bool UpdateElementRuleMatchingState(HTMLElement* element) {
  SelectorTree* selector_tree = element->node_document()->selector_tree();
  bool sibling_matching_active = selector_tree->has_sibling_combinators();

  // Retrieve the parent and previous sibling of the current element.
  HTMLElement* parent = element->parent_element()
                            ? element->parent_element()->AsHTMLElement()
                            : NULL;
  HTMLElement* previous_sibling =
      sibling_matching_active && element->previous_element_sibling()
          ? element->previous_element_sibling()->AsHTMLElement()
          : NULL;

  // Retrieve the rule matching state of this element, its parent, and its
  // previous sibling.
  HTMLElement::RuleMatchingState* element_matching_state =
      element->rule_matching_state();
  HTMLElement::RuleMatchingState* parent_matching_state =
      parent ? parent->rule_matching_state() : NULL;
  HTMLElement::RuleMatchingState* previous_sibling_matching_state =
      previous_sibling ? previous_sibling->rule_matching_state() : NULL;

  // A parent must always have valid matching rules when its child is being
  // updated.
  DCHECK(!parent || parent->matching_rules_valid());

  // Ensure that the previous sibling has updated matching rules. This is
  // necessary because Document::UpdateComputedStyleOnElementAndAncestor() does
  // not update previous siblings so it is possible for an element to have its
  // computed style updated while previous siblings have invalid matching rules.
  if (previous_sibling) {
    UpdateElementMatchingRules(previous_sibling);
    DCHECK(previous_sibling->matching_rules_valid());
  }

  // Ensure that the required combinator nodes of the parent and previous
  // sibling are up to date. These are lazily updated, so they won't be updated
  // until the first child/next sibling needs them.
  UpdateRuleMatchingStateCombinatorNodes(parent_matching_state,
                                         cssom::kDescendantCombinator);
  UpdateRuleMatchingStateCombinatorNodes(previous_sibling_matching_state,
                                         cssom::kFollowingSiblingCombinator);

  bool matching_nodes_modified = !element_matching_state->is_set;

  // Gather modifications between the element's previous state and current state
  // for each combinator type, and use them to update the element's matching
  // nodes.
  SelectorTree::Nodes* added_nodes = selector_tree->scratchpad_nodes_1();
  SelectorTree::Nodes* removed_nodes = selector_tree->scratchpad_nodes_2();

  // Child combinator
  if (parent_matching_state &&
      GatherNodeModificationsAndUpdateTargetNodes(
          parent_matching_state->matching_nodes,
          &element_matching_state->parent_matching_nodes, added_nodes,
          removed_nodes)) {
    matching_nodes_modified |= UpdateMatchingNodesFromNodeModifications(
        *added_nodes, *removed_nodes, cssom::kChildCombinator, element,
        selector_tree->scratchpad_node_pairs());
  }

  // Descendant combinator
  const SelectorTree::Nodes& parent_descendant_nodes =
      parent_matching_state ? parent_matching_state->descendant_nodes
                            : selector_tree->root_nodes();
  if (GatherNodeModificationsAndUpdateTargetNodes(
          parent_descendant_nodes,
          &element_matching_state->parent_descendant_nodes, added_nodes,
          removed_nodes)) {
    element_matching_state->are_descendant_nodes_dirty = true;
    matching_nodes_modified |= UpdateMatchingNodesFromNodeModifications(
        *added_nodes, *removed_nodes, cssom::kDescendantCombinator, element,
        selector_tree->scratchpad_node_pairs());
  }

  // Siblings only need to be checked if they're active.
  if (sibling_matching_active) {
    const SelectorTree::Nodes empty_nodes{};

    // Next sibling combinator
    const SelectorTree::Nodes& previous_sibling_matching_nodes =
        previous_sibling_matching_state
            ? previous_sibling_matching_state->matching_nodes
            : empty_nodes;
    if (GatherNodeModificationsAndUpdateTargetNodes(
            previous_sibling_matching_nodes,
            &element_matching_state->previous_sibling_matching_nodes,
            added_nodes, removed_nodes)) {
      matching_nodes_modified |= UpdateMatchingNodesFromNodeModifications(
          *added_nodes, *removed_nodes, cssom::kNextSiblingCombinator, element,
          selector_tree->scratchpad_node_pairs());
    }

    // Following sibling combinator
    const SelectorTree::Nodes& previous_sibling_following_sibling_nodes =
        previous_sibling_matching_state
            ? previous_sibling_matching_state->following_sibling_nodes
            : empty_nodes;
    if (GatherNodeModificationsAndUpdateTargetNodes(
            previous_sibling_following_sibling_nodes,
            &element_matching_state->previous_sibling_following_sibling_nodes,
            added_nodes, removed_nodes)) {
      element_matching_state->are_following_sibling_nodes_dirty = true;
      matching_nodes_modified |= UpdateMatchingNodesFromNodeModifications(
          *added_nodes, *removed_nodes, cssom::kFollowingSiblingCombinator,
          element, selector_tree->scratchpad_node_pairs());
    }
  }

  element_matching_state->is_set = true;

  // Matching node modifications may impact combinator nodes, so they must be
  // dirtied. However, they are lazily updated the first time that they are
  // needed by a child/following sibling, rather than immediately updated. This
  // prevents unnecessary updates of leaf elements and last sibling elements.
  if (matching_nodes_modified) {
    element_matching_state->are_descendant_nodes_dirty = true;
    element_matching_state->are_following_sibling_nodes_dirty = true;
  }

  return matching_nodes_modified;
}

void UpdateElementMatchingRulesFromRuleMatchingState(HTMLElement* element) {
  Document* element_document = element->node_document();

  // Store the element's and its pseudo element's old matching rules so that
  // they can be compared to the new matching rules after they are generated.
  cssom::RulesWithCascadePrecedence* element_old_matching_rules =
      element_document->scratchpad_html_element_matching_rules();
  *element_old_matching_rules = std::move(*element->matching_rules());
  element->matching_rules()->clear();

  for (int type = 0; type < kMaxPseudoElementType; ++type) {
    PseudoElement* pseudo_element =
        element->pseudo_element(PseudoElementType(type));
    if (pseudo_element) {
      cssom::RulesWithCascadePrecedence* old_pseudo_element_matching_rules =
          element_document->scratchpad_pseudo_element_matching_rules(
              PseudoElementType(type));
      *old_pseudo_element_matching_rules =
          std::move(*pseudo_element->matching_rules());
      pseudo_element->matching_rules()->clear();
    }
  }

  // Walk all matching nodes, generating matching rules for the element and
  // its pseudo elements from them.
  for (const auto& node : element->rule_matching_state()->matching_nodes) {
    // Determine the matching rules that this specific node targets. If the
    // node does not have a pseudo element, then this will be the element's
    // matching rules; otherwise, it'll be the matching rules associated with
    // the pseudo element's type.
    cssom::RulesWithCascadePrecedence* target_matching_rules;

    cssom::PseudoElement* pseudo_element =
        node->compound_selector()->pseudo_element();
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

      // Make sure the pseudo element exists under element. If not, create it
      // and clear out the old matching rules since the pseudo element had no
      // pre-existing matching rules.
      if (!element->pseudo_element(pseudo_element_type)) {
        element->SetPseudoElement(
            pseudo_element_type,
            base::WrapUnique(new dom::PseudoElement(element)));

        cssom::RulesWithCascadePrecedence* old_pseudo_element_matching_rules =
            element_document->scratchpad_pseudo_element_matching_rules(
                PseudoElementType(pseudo_element_type));
        old_pseudo_element_matching_rules->clear();
      }

      target_matching_rules =
          element->pseudo_element(pseudo_element_type)->matching_rules();
    }

    // Add all of the node's rules to the target matching rules.
    for (const auto& weak_rule : node->rules()) {
      const auto rule = weak_rule.get();
      if (!rule) {
        continue;
      }
      DCHECK(rule->parent_style_sheet());
      cssom::CascadePrecedence precedence(
          pseudo_element ? cssom::kNormalOverride
                         : rule->parent_style_sheet()->origin(),
          node->cumulative_specificity(),
          cssom::Appearance(rule->parent_style_sheet()->index(),
                            rule->index()));
      target_matching_rules->push_back(std::make_pair(rule, precedence));
    }
  }

  // Check for if the newly generated matching rules are different for the
  // element or its pseudo elements.
  if (*element_old_matching_rules != *element->matching_rules()) {
    element->OnMatchingRulesModified();
  }

  for (int type = 0; type < kMaxPseudoElementType; ++type) {
    PseudoElement* pseudo_element =
        element->pseudo_element(PseudoElementType(type));
    if (pseudo_element) {
      cssom::RulesWithCascadePrecedence* old_pseudo_element_matching_rules =
          element_document->scratchpad_pseudo_element_matching_rules(
              PseudoElementType(type));
      if (*old_pseudo_element_matching_rules !=
          *pseudo_element->matching_rules()) {
        pseudo_element->set_computed_style_invalid();
        element->OnPseudoElementMatchingRulesModified();
      }
    }
  }
}

}  // namespace

//////////////////////////////////////////////////////////////////////////
// Public APIs
//////////////////////////////////////////////////////////////////////////

void UpdateElementMatchingRules(HTMLElement* element) {
  DCHECK(element);
  DCHECK(element->node_document());
  if (element->matching_rules_valid()) {
    return;
  }

  // Only update the matching rules if the rule matching state is modified.
  if (UpdateElementRuleMatchingState(element)) {
    UpdateElementMatchingRulesFromRuleMatchingState(element);
  }

  element->set_matching_rules_valid();
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

bool MatchRuleAndElement(cssom::CSSStyleRule* rule, Element* element) {
  for (cssom::Selectors::const_iterator selector_iterator =
           rule->selectors().begin();
       selector_iterator != rule->selectors().end(); ++selector_iterator) {
    DCHECK(*selector_iterator);
    if (MatchSelectorAndElement(selector_iterator->get(), element, true)) {
      return true;
    }
  }
  return false;
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

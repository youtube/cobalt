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

#include "cobalt/cssom/selector_tree.h"

#include "cobalt/cssom/complex_selector.h"
#include "cobalt/cssom/compound_selector.h"
#include "cobalt/cssom/css_style_rule.h"

namespace cobalt {
namespace cssom {

bool SelectorTree::CompoundNodeLessThan::operator()(
    const CompoundSelector* lhs, const CompoundSelector* rhs) const {
  return *lhs < *rhs;
}

SelectorTree::Node::Node()
    : combinator_mask_(0),
      compound_selector_(NULL),
      selector_mask_(0),
      pseudo_class_mask_(0) {}

SelectorTree::Node::Node(CompoundSelector* compound_selector,
                         Specificity parent_specificity)
    : combinator_mask_(0),
      compound_selector_(compound_selector),
      cumulative_specificity_(parent_specificity),
      selector_mask_(0),
      pseudo_class_mask_(0) {
  cumulative_specificity_.AddFrom(compound_selector_->GetSpecificity());
}

void SelectorTree::AppendRule(CSSStyleRule* rule) {
  for (Selectors::const_iterator it = rule->selectors().begin();
       it != rule->selectors().end(); ++it) {
    DCHECK((*it)->AsComplexSelector());
    Node* node = GetOrCreateNodeForComplexSelector((*it)->AsComplexSelector());
    node->rules().push_back(base::AsWeakPtr(rule));
  }
}

void SelectorTree::RemoveRule(CSSStyleRule* rule) {
  for (Selectors::const_iterator it = rule->selectors().begin();
       it != rule->selectors().end(); ++it) {
    DCHECK((*it)->AsComplexSelector());
    Node* node = GetOrCreateNodeForComplexSelector((*it)->AsComplexSelector());
    for (Rules::iterator remove_it = node->rules().begin();
         remove_it != node->rules().end(); ++remove_it) {
      if (*remove_it == rule) {
        node->rules().erase(remove_it);
        break;
      }
    }
  }
}

const SelectorTree::OwnedNodes& SelectorTree::children(
    const Node* node, CombinatorType combinator) {
  return owned_nodes_map_[std::make_pair(node, combinator)];
}

SelectorTree::Node* SelectorTree::GetOrCreateNodeForComplexSelector(
    ComplexSelector* complex_selector) {
  CompoundSelector* selector = complex_selector->first_selector();
  Node* node = GetOrCreateNodeForCompoundSelector(selector, &root_,
                                                  kDescendantCombinator);

  while (selector->right_combinator()) {
    Combinator* combinator = selector->right_combinator();
    selector = combinator->right_selector();
    node = GetOrCreateNodeForCompoundSelector(selector, node,
                                              combinator->GetCombinatorType());
  }

  return node;
}

SelectorTree::Node* SelectorTree::GetOrCreateNodeForCompoundSelector(
    CompoundSelector* compound_selector, Node* parent_node,
    CombinatorType combinator) {
  if (combinator == kNextSiblingCombinator ||
      combinator == kFollowingSiblingCombinator) {
    has_sibling_combinators_ = true;
  }

  OwnedNodes& owned_nodes =
      owned_nodes_map_[std::make_pair(parent_node, combinator)];
  OwnedNodes::iterator child_node_it = owned_nodes.find(compound_selector);
  if (child_node_it != owned_nodes.end()) {
    return child_node_it->second;
  }
  Node* child_node =
      new Node(compound_selector, parent_node->cumulative_specificity());
  parent_node->combinator_mask_ |= (1 << combinator);
  owned_nodes[compound_selector] = child_node;

  (*compound_selector->simple_selectors().begin())
      ->IndexSelectorTreeNode(parent_node, child_node, combinator);
  return child_node;
}

}  // namespace cssom
}  // namespace cobalt

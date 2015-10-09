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

#include "cobalt/cssom/class_selector.h"
#include "cobalt/cssom/complex_selector.h"
#include "cobalt/cssom/compound_selector.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/empty_pseudo_class.h"
#include "cobalt/cssom/id_selector.h"
#include "cobalt/cssom/type_selector.h"

namespace cobalt {
namespace cssom {

SelectorTree::Node::Node(CompoundSelector* compound_selector,
                         const Specificity& parent_specificity)
    : compound_selector(compound_selector),
      cumulative_specificity(parent_specificity) {
  cumulative_specificity.AddFrom(compound_selector->GetSpecificity());
}

void SelectorTree::AppendRule(CSSStyleRule* rule) {
  for (Selectors::const_iterator it = rule->selectors().begin();
       it != rule->selectors().end(); ++it) {
    DCHECK((*it)->AsComplexSelector());
    Node* node = GetOrCreateNodeForComplexSelector((*it)->AsComplexSelector());
    node->rules.push_back(base::AsWeakPtr(rule));
  }
}

void SelectorTree::RemoveRule(CSSStyleRule* rule) {
  for (Selectors::const_iterator it = rule->selectors().begin();
       it != rule->selectors().end(); ++it) {
    DCHECK((*it)->AsComplexSelector());
    Node* node = GetOrCreateNodeForComplexSelector((*it)->AsComplexSelector());
    for (Rules::iterator remove_it = node->rules.begin();
         remove_it != node->rules.end(); ++remove_it) {
      if (*remove_it == rule) {
        node->rules.erase(remove_it);
        break;
      }
    }
  }
}

SelectorTree::Node* SelectorTree::GetOrCreateNodeForComplexSelector(
    ComplexSelector* complex_selector) {
  Node* node = GetOrCreateNodeForCompoundSelector(
      complex_selector->first_selector(), &root_, kDescendantCombinator);

  for (Combinators::const_iterator it = complex_selector->combinators().begin();
       it != complex_selector->combinators().end(); ++it) {
    DCHECK((*it)->selector()->AsCompoundSelector());
    node = GetOrCreateNodeForCompoundSelector(
        (*it)->selector()->AsCompoundSelector(), node,
        (*it)->GetCombinatorType());
  }
  return node;
}

SelectorTree::Node* SelectorTree::GetOrCreateNodeForCompoundSelector(
    CompoundSelector* compound_selector, Node* parent_node,
    CombinatorType combinator) {
  for (OwnedNodes::iterator it = parent_node->children[combinator].begin();
       it != parent_node->children[combinator].end(); ++it) {
    if ((*it)->compound_selector->GetNormalizedSelectorText() ==
        compound_selector->GetNormalizedSelectorText()) {
      return *it;
    }
  }
  Node* child_node =
      new Node(compound_selector, parent_node->cumulative_specificity);
  parent_node->children[combinator].push_back(child_node);

  for (Selectors::const_iterator it = compound_selector->selectors().begin();
       it != compound_selector->selectors().end(); ++it) {
    (*it)->IndexSelectorTreeNode(parent_node, child_node, combinator);
  }
  return child_node;
}

}  // namespace cssom
}  // namespace cobalt

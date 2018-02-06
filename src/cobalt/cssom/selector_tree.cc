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

#include "cobalt/cssom/selector_tree.h"

#include <set>

#if defined(COBALT_ENABLE_VERSION_COMPATIBILITY_VALIDATIONS)
#include "cobalt/base/version_compatibility.h"
#endif  // defined(COBALT_ENABLE_VERSION_COMPATIBILITY_VALIDATIONS)
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

#if defined(COBALT_ENABLE_VERSION_COMPATIBILITY_VALIDATIONS)
namespace {

// This uses the old CompoundSelector compare logic that had a bug where 'not'
// pseudo classes with the same prefix would be treated as matching, regardless
// of their arguments.
struct PreVersion16CompoundSelectorLessThan {
  bool operator()(const CompoundSelector* lhs,
                  const CompoundSelector* rhs) const {
    const CompoundSelector::SimpleSelectors& lhs_simple_selectors =
        lhs->simple_selectors();
    const CompoundSelector::SimpleSelectors& rhs_simple_selectors =
        rhs->simple_selectors();
    if (lhs_simple_selectors.size() < rhs_simple_selectors.size()) {
      return true;
    }
    if (lhs_simple_selectors.size() > rhs_simple_selectors.size()) {
      return false;
    }

    for (size_t i = 0; i < lhs_simple_selectors.size(); ++i) {
      if (lhs_simple_selectors[i]->type() < rhs_simple_selectors[i]->type()) {
        return true;
      }
      if (lhs_simple_selectors[i]->type() > rhs_simple_selectors[i]->type()) {
        return false;
      }
      if (lhs_simple_selectors[i]->prefix() <
          rhs_simple_selectors[i]->prefix()) {
        return true;
      }
      if (lhs_simple_selectors[i]->prefix() >
          rhs_simple_selectors[i]->prefix()) {
        return false;
      }
      if (lhs_simple_selectors[i]->text() < rhs_simple_selectors[i]->text()) {
        return true;
      }
      if (lhs_simple_selectors[i]->text() > rhs_simple_selectors[i]->text()) {
        return false;
      }
    }

    return false;
  }
};

bool HasNotPseudoClassCompatibilityViolations(
    const SelectorTree::OwnedNodesMap& owned_nodes_map) {
  constexpr int kNotPseudoClassCobaltVersionFix = 16;
  if (base::VersionCompatibility::GetInstance()->GetMinimumVersion() >=
      kNotPseudoClassCobaltVersionFix) {
    return false;
  }

  typedef std::set<CompoundSelector*, PreVersion16CompoundSelectorLessThan>
      PreVersion16CompoundSelectorLessThanSet;

  // Walk all of the owned nodes looking for multiple compound selectors within
  // them that are treated as duplicates using the pre-version 16 compound
  // selector less than logic.
  PreVersion16CompoundSelectorLessThanSet unsupported_usage_set;
  for (const auto& owned_nodes_iterator : owned_nodes_map) {
    PreVersion16CompoundSelectorLessThanSet encountered_set;
    for (const auto& node_info : owned_nodes_iterator.second) {
      if (!encountered_set.insert(node_info.first).second) {
        unsupported_usage_set.insert(node_info.first);
      }
    }
  }

  // Log the cases of unsupported usage.
  for (const auto& unsupported_usage_iterator : unsupported_usage_set) {
    const std::string kUnsupportedNotUsageString =
        "Unsupported :not() pseudo class usage found. Multiple negation pseudo "
        "classes with the same prefix but different arguments are not "
        "supported in versions of Cobalt <= 15. Offending selectors: '";

    std::string selectors_string;
    const CompoundSelector::SimpleSelectors& simple_selectors =
        unsupported_usage_iterator->simple_selectors();
    for (const auto& simple_selector : simple_selectors) {
      selectors_string += simple_selector->prefix().c_str();
      if (simple_selector->type() == kPseudoClass &&
          simple_selector->GetContainedCompoundSelector() != NULL) {
        selectors_string += "not(...)";
      } else {
        selectors_string += simple_selector->text().c_str();
      }
    }

    base::VersionCompatibility::GetInstance()->ReportViolation(
        kUnsupportedNotUsageString + selectors_string);
  }
  return !unsupported_usage_set.empty();
}

}  // namespace

bool SelectorTree::ValidateVersionCompatibility() const {
  return !HasNotPseudoClassCompatibilityViolations(owned_nodes_map_);
}
#endif  // defined(COBALT_ENABLE_VERSION_COMPATIBILITY_VALIDATIONS)

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

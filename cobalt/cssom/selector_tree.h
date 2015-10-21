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

#ifndef CSSOM_SELECTOR_TREE_H_
#define CSSOM_SELECTOR_TREE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/hash_tables.h"
#include "base/basictypes.h"
#include "base/containers/small_map.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/base/unused.h"
#include "cobalt/cssom/combinator.h"
#include "cobalt/cssom/specificity.h"

namespace cobalt {
namespace cssom {

class CompoundSelector;
class ComplexSelector;
class CSSStyleRule;

// A selector tree is a tree structure that organizes compound selectors. It is
// used to accelerate rule matching. More details can be found in the doc:
// https://docs.google.com/document/d/1LTbSenGsGR94JTGg6DfZDXYB3MBBCp4C8dRC4rckt_8/
//
// TODO(***REMOVED***, b/24976289): Note that current selector tree does not support
// removing rule from it, and is assuming that the appended rule does not
// disappear. Support when needed.
class SelectorTree {
 public:
  typedef std::vector<base::WeakPtr<CSSStyleRule> > Rules;

  struct Node;

  typedef ScopedVector<Node> OwnedNodes;
  // NOTE: The array size of SmallMap and the decision to use std::map as the
  // underlying container type are based on extensive performance testing with
  // ***REMOVED***. Do not change these unless additional profiling data justifies it.
  typedef base::SmallMap<std::map<Node*, base::Unused>, 12> NodeSet;
  typedef std::vector<Node*> Nodes;
  // NOTE: The array size of SmallMap and the decision to use base::hash_map as
  // the underlying container type are based on extensive performance testing
  // with ***REMOVED***. Do not change these unless additional profiling data justifies
  // it.
  typedef base::SmallMap<base::hash_map<std::string, Nodes>, 2>
      SelectorTextToNodesMap;

  struct Node {
    Node() : compound_selector(NULL) {}
    Node(CompoundSelector* compound_selector,
         const Specificity& parent_specificity);

    // Pointer to one of the equivalent compound selectors.
    CompoundSelector* compound_selector;
    // Sum of specificity from root to this node.
    Specificity cumulative_specificity;

    // Children of the node, with given type of combinator.
    OwnedNodes children[kCombinatorCount];
    // Indexes for the children.
    SelectorTextToNodesMap type_selector_nodes_map[kCombinatorCount];
    SelectorTextToNodesMap class_selector_nodes_map[kCombinatorCount];
    SelectorTextToNodesMap id_selector_nodes_map[kCombinatorCount];
    Nodes empty_pseudo_class_nodes[kCombinatorCount];

    // Rules that end with this node.
    Rules rules;
  };

  SelectorTree() { root_set_.insert(std::make_pair(&root_, base::Unused())); }

  Node* root() { return &root_; }
  NodeSet* root_set() { return &root_set_; }

  void AppendRule(CSSStyleRule* rule);
  void RemoveRule(CSSStyleRule* rule);

 private:
  // Get or create node for complex selector, starting from root.
  Node* GetOrCreateNodeForComplexSelector(ComplexSelector* selector);

  // Get or create node for compound selector, starting from given node, using
  // given combiantor.
  Node* GetOrCreateNodeForCompoundSelector(CompoundSelector* compound_selector,
                                           Node* parent_node,
                                           CombinatorType combinator);

  Node root_;
  NodeSet root_set_;

  DISALLOW_COPY_AND_ASSIGN(SelectorTree);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_SELECTOR_TREE_H_

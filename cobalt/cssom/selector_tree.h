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

#ifndef COBALT_CSSOM_SELECTOR_TREE_H_
#define COBALT_CSSOM_SELECTOR_TREE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/containers/small_map.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/base/token.h"
#include "cobalt/cssom/combinator.h"
#include "cobalt/cssom/pseudo_class_type.h"
#include "cobalt/cssom/simple_selector_type.h"
#include "cobalt/cssom/specificity.h"

namespace cobalt {
namespace cssom {

class CompoundSelector;
class ComplexSelector;
class CSSStyleRule;

// A selector tree is a tree structure that organizes compound selectors. It is
// used to accelerate rule matching. More details can be found in the doc:
// https://docs.google.com/document/d/1LTbSenGsGR94JTGg6DfZDXYB3MBBCp4C8dRC4rckt_8/
class SelectorTree {
 public:
  class Node;

  typedef std::vector<base::WeakPtr<CSSStyleRule>> Rules;

  typedef std::vector<const Node*> Nodes;
  typedef std::vector<std::pair<const Node*, const Node*>> NodePairs;

  struct CompoundNodeLessThan {
    bool operator()(const CompoundSelector* lhs,
                    const CompoundSelector* rhs) const;
  };

  // This class holds references to Nodes allocated on the heap that are owned
  // by the same parent Node.  It deletes all contained Nodes on destruction.
  class OwnedNodes
      : public base::small_map<
            std::map<CompoundSelector*, Node*, CompoundNodeLessThan>, 2> {
   public:
    ~OwnedNodes() {
      for (iterator iter = begin(); iter != end(); ++iter) {
        delete iter->second;
      }
    }
  };

  typedef std::map<std::pair<const Node*, CombinatorType>, OwnedNodes>
      OwnedNodesMap;

  struct PseudoClassNode {
    PseudoClassType pseudo_class_type;
    CombinatorType combinator_type;
    Node* node;
  };

  struct SimpleSelectorNode {
    SimpleSelectorType simple_selector_type;
    CombinatorType combinator_type;
    Node* node;
  };

  typedef std::vector<SimpleSelectorNode> SimpleSelectorNodes;
  typedef std::vector<PseudoClassNode> PseudoClassNodes;
  // The vast majority of SelectorTextToNodesMap objects have 4 or fewer
  // selectors. However, they occasionally can number in the hundreds. Using
  // a SmallMap with an array size of 4, allows both cases to be handled
  // quickly.
  typedef base::small_map<base::hash_map<base::Token, SimpleSelectorNodes>, 4>
      SelectorTextToNodesMap;

  class Node {
   public:
    CompoundSelector* compound_selector() const { return compound_selector_; }
    Specificity cumulative_specificity() const {
      return cumulative_specificity_;
    }
    bool HasCombinator(CombinatorType type) const {
      return (combinator_mask_ & (1 << type)) != 0;
    }
    Rules& rules() { return rules_; }
    const Rules& rules() const { return rules_; }

    const PseudoClassNodes& pseudo_class_nodes() const {
      return pseudo_class_nodes_;
    }
    bool HasAnyPseudoClass() const { return pseudo_class_mask_ != 0; }
    bool HasPseudoClass(PseudoClassType pseudo_class_type,
                        CombinatorType combinator_type) const {
      return (pseudo_class_mask_ &
              (1u << (pseudo_class_type * kCombinatorCount +
                      combinator_type))) != 0;
    }
    void AppendPseudoClassNode(PseudoClassType pseudo_class_type,
                               CombinatorType combinator_type, Node* node) {
      PseudoClassNode child_node = {pseudo_class_type, combinator_type, node};
      pseudo_class_nodes_.push_back(child_node);
      pseudo_class_mask_ |=
          (1u << (pseudo_class_type * kCombinatorCount + combinator_type));
    }

    const SelectorTextToNodesMap* selector_nodes_map() const {
      return selector_nodes_map_.get();
    }
    bool HasSimpleSelector(SimpleSelectorType simple_selector_type,
                           CombinatorType combinator_type) const {
      return (selector_mask_ & (1u << (simple_selector_type * kCombinatorCount +
                                       combinator_type))) != 0;
    }
    void AppendSimpleSelector(base::Token name,
                              SimpleSelectorType simple_selector_type,
                              CombinatorType combinator_type, Node* node) {
      // Create the SelectorTextToNodesMap if this is the first simple selector
      // being appended.
      if (!selector_nodes_map_) {
        selector_nodes_map_.reset(new SelectorTextToNodesMap());
      }

      SimpleSelectorNode child_node = {simple_selector_type, combinator_type,
                                       node};
      (*selector_nodes_map_)[name].push_back(child_node);
      selector_mask_ |=
          (1u << (simple_selector_type * kCombinatorCount + combinator_type));
    }

   private:
    friend class SelectorTree;

    Node();
    Node(CompoundSelector* compound_selector, Specificity parent_specificity);

    // Bit mask used to quickly reject a certain combinator type.
    uint8 combinator_mask_;
    COMPILE_ASSERT(sizeof(uint8) * 8 >= kCombinatorCount,
                   combinator_mask_does_not_have_enough_bits);

    // Pointer to one of the equivalent compound selectors.
    CompoundSelector* compound_selector_;
    // Sum of specificity from root to this node.
    Specificity cumulative_specificity_;

    // Indexes for the children. This is a std::unique_ptr because the majority
    // of nodes do not contain any children.
    std::unique_ptr<SelectorTextToNodesMap> selector_nodes_map_;
    // Bit mask used to quickly reject a certain selector type and combinator
    // combination.
    uint32 selector_mask_;
    COMPILE_ASSERT(sizeof(uint32) * 8 >=
                       kSimpleSelectorTypeCount * kCombinatorCount,
                   selector_mask_does_not_have_enough_bits);
    PseudoClassNodes pseudo_class_nodes_;
    // Bit mask used to quickly reject a certain pseudo class and combinator
    // combination.
    uint32 pseudo_class_mask_;
    COMPILE_ASSERT(sizeof(uint32) * 8 >=
                       kPseudoClassTypeCount * kCombinatorCount,
                   pseudo_class_mask_does_not_have_enough_bits);

    // Rules that end with this node.
    Rules rules_;
  };

  SelectorTree() : has_sibling_combinators_(false) {
    root_nodes_.push_back(&root_node_);
  }

  const Node* root_node() const { return &root_node_; }
  const Nodes& root_nodes() const { return root_nodes_; }

  Nodes* scratchpad_nodes_1() { return &scratchpad_nodes_1_; }
  Nodes* scratchpad_nodes_2() { return &scratchpad_nodes_2_; }
  NodePairs* scratchpad_node_pairs() { return &scratchpad_node_pairs_; }

  bool has_sibling_combinators() const { return has_sibling_combinators_; }

  void AppendRule(CSSStyleRule* rule);
  void RemoveRule(CSSStyleRule* rule);

  // Used by unit tests only.
  const OwnedNodes& children(const Node* node, CombinatorType combinator);

#if defined(COBALT_ENABLE_VERSION_COMPATIBILITY_VALIDATIONS)
  // Validates the selector tree's compatibility against pre-selected versions
  // of Cobalt. Returns true if there are no version compatibility violations.
  bool ValidateVersionCompatibility() const;
#endif  // defined(COBALT_ENABLE_VERSION_COMPATIBILITY_VALIDATIONS)

 private:
  // Gets or creates node for complex selector, starting from root.
  Node* GetOrCreateNodeForComplexSelector(ComplexSelector* selector);

  // Gets or creates node for compound selector, starting from given node, using
  // given combiantor as edge.
  Node* GetOrCreateNodeForCompoundSelector(CompoundSelector* compound_selector,
                                           Node* parent_node,
                                           CombinatorType combinator);

  Node root_node_;
  Nodes root_nodes_;

  // These member variables are available for temporary operations, so that
  // Node vectors associated with the SelectorTree don't have to be repeatedly
  // re-allocated. This significantly speeds up rule matching.
  Nodes scratchpad_nodes_1_;
  Nodes scratchpad_nodes_2_;
  NodePairs scratchpad_node_pairs_;

  // This variable maps from a parent Node and a combinator type to child Nodes
  // under the particular parent Node corresponding to the particular combinator
  // type.  It is only used when modifying the SelectorTree and is not used
  // during rule matching.  So we store it externally to the Node to minimize
  // the size of Node structure.
  OwnedNodesMap owned_nodes_map_;

  bool has_sibling_combinators_;

  DISALLOW_COPY_AND_ASSIGN(SelectorTree);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_SELECTOR_TREE_H_

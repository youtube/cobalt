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

#ifndef COBALT_CSSOM_SELECTOR_TREE_H_
#define COBALT_CSSOM_SELECTOR_TREE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/small_map.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_vector.h"
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
  typedef std::vector<base::WeakPtr<CSSStyleRule> > Rules;

  class Node;

  // This class can be used to store Nodes.  It stores the Nodes in its
  // internal buffer whose size can be configured via template parameter.
  // After the internal buffer is used up the extra Nodes will be stored inside
  // the contained std::vector.
  // TODO: Move this off to its own file if this can also be used by
  // other code.
  template <size_t InternalCacheSize>
  class NodeSet {
   public:
    // Minimum interface for iterator.
    class const_iterator {
     public:
      const_iterator(const NodeSet* set, size_t index)
          : set_(set), index_(index) {}
      void operator++() { ++index_; }
      const Node* operator*() const { return set_->GetNode(index_); }
      bool operator!=(const const_iterator& that) const {
        return set_ != that.set_ || index_ != that.index_;
      }

     private:
      const NodeSet* set_;
      size_t index_;
    };

    NodeSet() : size_(0) {}
    void insert(const Node* node, bool check_for_duplicate = false) {
      // If |check_for_duplicate| is true, then check if the node is already
      // contained. In nearly all cases, this check is unnecessary because it is
      // already known that the node is not a duplicate. As a result, the caller
      // must explicitly request the check when needed.
      if (check_for_duplicate) {
        for (size_t i = 0; i < size_; ++i) {
          if (GetNode(i) == node) {
            return;
          }
        }
      }

      if (size_ < InternalCacheSize) {
        nodes_[size_] = node;
      } else {
        nodes_vector_.push_back(node);
      }
      ++size_;
    }
    template <class ConstIterator>
    void insert(ConstIterator begin, ConstIterator end,
                bool check_for_duplicate = false) {
      while (begin != end) {
        insert(*begin, check_for_duplicate);
        ++begin;
      }
    }

    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, size_); }
    size_t size() const { return size_; }
    void clear() {
      size_ = 0;
      nodes_vector_.clear();
    }
    const Node* GetNode(size_t index) const {
      if (index < InternalCacheSize) {
        return nodes_[index];
      }
      return nodes_vector_[index - InternalCacheSize];
    }

   private:
    size_t size_;
    const Node* nodes_[InternalCacheSize];
    std::vector<const Node*> nodes_vector_;
  };

  struct CompoundNodeLessThan {
    bool operator()(const CompoundSelector* lhs,
                    const CompoundSelector* rhs) const;
  };

  // This class holds references to Nodes allocated on the heap that are owned
  // by the same parent Node.  It deletes all contained Nodes on destruction.
  class OwnedNodes
      : public base::SmallMap<
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
  typedef base::SmallMap<base::hash_map<base::Token, SimpleSelectorNodes>, 4>
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

    // Indexes for the children. This is a scoped_ptr because the majority of
    // nodes do not contain any children.
    scoped_ptr<SelectorTextToNodesMap> selector_nodes_map_;
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

  SelectorTree() : has_sibling_combinators_(false) { root_set_.insert(&root_); }

  const Node* root() const { return &root_; }
  const NodeSet<1>& root_set() const { return root_set_; }

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

  Node root_;
  NodeSet<1> root_set_;

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

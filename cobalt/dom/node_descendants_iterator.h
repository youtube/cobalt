// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_NODE_DESCENDANTS_ITERATOR_H_
#define COBALT_DOM_NODE_DESCENDANTS_ITERATOR_H_

#include "cobalt/dom/node.h"

namespace cobalt {
namespace dom {

// Iterates over all descendants of the given node (excluding the node itself).
// The iterator uses depth-first pre-order traversal to visit the descendants.
class NodeDescendantsIterator {
 public:
  explicit NodeDescendantsIterator(const scoped_refptr<const Node>& base)
      : base_(base), current_(NULL) {}

  Node* First() {
    current_ = base_->first_child();
    return current_;
  }

  Node* Next() {
    if (!current_) {
      return NULL;
    }
    if (current_->first_child()) {
      // Walk down and use the first child.
      current_ = current_->first_child();
    } else {
      // Walk towards the next sibling if one exists.
      // If one doesn't exist, walk up and look for a node (that is not a root)
      // with a sibling and continue the iteration from there.
      while (!current_->next_sibling()) {
        current_ = current_->parent_node();
        if (current_ == base_) {
          current_ = NULL;
          return current_;
        }
      }
      current_ = current_->next_sibling();
    }
    return current_;
  }

 private:
  scoped_refptr<const Node> base_;
  // Scoped pointer is not used for optimization reasons.
  Node* current_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_NODE_DESCENDANTS_ITERATOR_H_

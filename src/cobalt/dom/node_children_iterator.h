// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_NODE_CHILDREN_ITERATOR_H_
#define COBALT_DOM_NODE_CHILDREN_ITERATOR_H_

#include "cobalt/dom/node.h"

namespace cobalt {
namespace dom {

// Iterates over the first-level children of the given node.
class NodeChildrenIterator {
 public:
  explicit NodeChildrenIterator(const scoped_refptr<const Node>& parent)
      : parent_(parent), current_(NULL) {}

  Node* First() {
    current_ = parent_->first_child();
    return current_;
  }

  Node* Next() {
    if (current_) {
      current_ = current_->next_sibling();
    }
    return current_;
  }

 private:
  scoped_refptr<const Node> parent_;
  // Scoped pointer is not used for optimization reasons.
  Node* current_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_NODE_CHILDREN_ITERATOR_H_

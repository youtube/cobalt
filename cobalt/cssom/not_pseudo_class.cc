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

#include "cobalt/cssom/not_pseudo_class.h"

#include "cobalt/cssom/compound_selector.h"
#include "cobalt/cssom/selector_visitor.h"

namespace cobalt {
namespace cssom {

void NotPseudoClass::Accept(SelectorVisitor* visitor) {
  visitor->VisitNotPseudoClass(this);
}

void NotPseudoClass::IndexSelectorTreeNode(SelectorTree::Node* parent_node,
                                           SelectorTree::Node* child_node,
                                           CombinatorType combinator) {
  parent_node->AppendPseudoClassNode(kNotPseudoClass, combinator, child_node);
}

CompoundSelector* NotPseudoClass::selector() const {
  return compound_selector_.get();
}

void NotPseudoClass::set_selector(
    scoped_ptr<CompoundSelector> compound_selector) {
  compound_selector_ = compound_selector.Pass();
}

}  // namespace cssom
}  // namespace cobalt

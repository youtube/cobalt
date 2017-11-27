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

#ifndef COBALT_CSSOM_EMPTY_PSEUDO_CLASS_H_
#define COBALT_CSSOM_EMPTY_PSEUDO_CLASS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/pseudo_class.h"
#include "cobalt/cssom/selector_tree.h"

namespace cobalt {
namespace cssom {

class SelectorVisitor;

// The :empty pseudo-class represents an element that has no children. In terms
// of the document tree, only element nodes and content nodes (such as DOM
// text nodes, CDATA nodes, and entity references) whose data has a non-zero
// length must be considered as affecting emptiness; comments, processing
// instructions, and other nodes must not affect whether an element is
// considered empty or not.
//   https://www.w3.org/TR/selectors4/#empty-pseudo
class EmptyPseudoClass : public PseudoClass {
 public:
  EmptyPseudoClass()
      : PseudoClass(base::Tokens::empty_pseudo_class_selector()) {}
  ~EmptyPseudoClass() override {}

  // From Selector.
  void Accept(SelectorVisitor* visitor) override;

  // From SimpleSelector.
  void IndexSelectorTreeNode(SelectorTree::Node* parent_node,
                             SelectorTree::Node* child_node,
                             CombinatorType combinator) override;

  // From PseudoClass.
  EmptyPseudoClass* AsEmptyPseudoClass() override { return this; }

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyPseudoClass);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_EMPTY_PSEUDO_CLASS_H_

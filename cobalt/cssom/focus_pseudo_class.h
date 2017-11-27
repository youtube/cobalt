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

#ifndef COBALT_CSSOM_FOCUS_PSEUDO_CLASS_H_
#define COBALT_CSSOM_FOCUS_PSEUDO_CLASS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/pseudo_class.h"
#include "cobalt/cssom/selector_tree.h"

namespace cobalt {
namespace cssom {

class SelectorVisitor;

// The :focus pseudo-class applies while an element has the focus (accepts
// keyboard or mouse events, or other forms of input).
//   https://www.w3.org/TR/selectors4/#focus-pseudo
class FocusPseudoClass : public PseudoClass {
 public:
  FocusPseudoClass()
      : PseudoClass(base::Tokens::focus_pseudo_class_selector()) {}
  ~FocusPseudoClass() override {}

  // From Selector.
  void Accept(SelectorVisitor* visitor) override;

  // From SimpleSelector.
  void IndexSelectorTreeNode(SelectorTree::Node* parent_node,
                             SelectorTree::Node* child_node,
                             CombinatorType combinator) override;

  // From PseudoClass.
  FocusPseudoClass* AsFocusPseudoClass() override { return this; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusPseudoClass);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_FOCUS_PSEUDO_CLASS_H_

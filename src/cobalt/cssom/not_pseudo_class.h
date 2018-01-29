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

#ifndef COBALT_CSSOM_NOT_PSEUDO_CLASS_H_
#define COBALT_CSSOM_NOT_PSEUDO_CLASS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/pseudo_class.h"
#include "cobalt/cssom/selector_tree.h"

namespace cobalt {
namespace cssom {

class CompoundSelector;
class SelectorVisitor;

// The negation pseudo-class, :not(), is a functional pseudo-class taking a
// selector list as an argument. It represents an element that is not
// represented by its argument.
//   https://www.w3.org/TR/selectors4/#negation-pseudo
class NotPseudoClass : public PseudoClass {
 public:
  NotPseudoClass()
      : PseudoClass(base::Tokens::pseudo_class_selector_prefix()) {}
  ~NotPseudoClass() override {}

  // From Selector.
  void Accept(SelectorVisitor* visitor) override;

  // From SimpleSelector.
  bool AlwaysRequiresRuleMatchingVerificationVisit() const override {
    return true;
  }

  CompoundSelector* GetContainedCompoundSelector() const override {
    return selector();
  }

  void IndexSelectorTreeNode(SelectorTree::Node* parent_node,
                             SelectorTree::Node* child_node,
                             CombinatorType combinator) override;

  // From PseudoClass.
  NotPseudoClass* AsNotPseudoClass() override { return this; }

  // The compound selector within the pseudo class.
  CompoundSelector* selector() const;
  void set_selector(scoped_ptr<CompoundSelector> compound_selector);

 private:
  scoped_ptr<CompoundSelector> compound_selector_;

  DISALLOW_COPY_AND_ASSIGN(NotPseudoClass);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_NOT_PSEUDO_CLASS_H_

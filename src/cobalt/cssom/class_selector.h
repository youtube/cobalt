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

#ifndef COBALT_CSSOM_CLASS_SELECTOR_H_
#define COBALT_CSSOM_CLASS_SELECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/selector_tree.h"
#include "cobalt/cssom/simple_selector.h"
#include "cobalt/cssom/specificity.h"

namespace cobalt {
namespace cssom {

class SelectorVisitor;

// The class selector represents an element belonging to the class identified by
// the identifier.
//   https://www.w3.org/TR/selectors4/#class-selector
class ClassSelector : public SimpleSelector {
 public:
  explicit ClassSelector(const std::string& class_name)
      : SimpleSelector(kClassSelector, base::Tokens::class_selector_prefix(),
                       base::Token(class_name)) {}
  ~ClassSelector() override {}

  // From Selector.
  void Accept(SelectorVisitor* visitor) override;
  Specificity GetSpecificity() const override { return Specificity(0, 1, 0); }

  // From SimpleSelector.
  void IndexSelectorTreeNode(SelectorTree::Node* parent_node,
                             SelectorTree::Node* child_node,
                             CombinatorType combinator) override;

  // Rest of public methods.
  base::Token class_name() const { return text(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ClassSelector);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CLASS_SELECTOR_H_

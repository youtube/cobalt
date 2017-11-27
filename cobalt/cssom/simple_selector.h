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

#ifndef COBALT_CSSOM_SIMPLE_SELECTOR_H_
#define COBALT_CSSOM_SIMPLE_SELECTOR_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/base/token.h"
#include "cobalt/cssom/combinator.h"
#include "cobalt/cssom/selector.h"
#include "cobalt/cssom/selector_tree.h"
#include "cobalt/cssom/simple_selector_type.h"

namespace cobalt {
namespace cssom {

class AttributeSelector;
class ClassSelector;
class IdSelector;
class PseudoClass;
class PseudoElement;
class TypeSelector;

// A simple selector is either a type selector, universal selector, attribute
// selector, class selector, ID selector, or pseudo-class.
//   https://www.w3.org/TR/selectors4/#simple
class SimpleSelector : public Selector {
 public:
  SimpleSelector(SimpleSelectorType type, base::Token prefix, base::Token text)
      : type_(type), prefix_(prefix), text_(text) {}
  ~SimpleSelector() override {}

  // From Selector.
  SimpleSelector* AsSimpleSelector() override { return this; }

  // Rest of public methods.
  virtual PseudoElement* AsPseudoElement() { return NULL; }

  virtual bool AlwaysRequiresRuleMatchingVerificationVisit() const {
    return false;
  }

  // Used to sort simple selectors when normalizing compound selector.
  SimpleSelectorType type() const { return type_; }

  // Used to index selector tree node's children.
  virtual void IndexSelectorTreeNode(SelectorTree::Node* parent_node,
                                     SelectorTree::Node* child_node,
                                     CombinatorType combinator) = 0;

  // Returns token representation of the selector.
  base::Token prefix() const { return prefix_; }
  base::Token text() const { return text_; }

 private:
  SimpleSelectorType type_;
  base::Token prefix_;
  base::Token text_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_SIMPLE_SELECTOR_H_

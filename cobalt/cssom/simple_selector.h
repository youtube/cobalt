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

#ifndef COBALT_CSSOM_SIMPLE_SELECTOR_H_
#define COBALT_CSSOM_SIMPLE_SELECTOR_H_

#include <string>

#include "base/compiler_specific.h"
#include "cobalt/base/token.h"
#include "cobalt/cssom/combinator.h"
#include "cobalt/cssom/selector.h"
#include "cobalt/cssom/selector_tree.h"
#include "cobalt/cssom/simple_selector_type.h"

namespace cobalt {
namespace cssom {

class AttributeSelector;
class ClassSelector;
class CompoundSelector;
class IdSelector;
class PseudoClass;
class PseudoElement;
class TypeSelector;
class UniversalSelector;

// A simple selector is either a type selector, universal selector, attribute
// selector, class selector, ID selector, or pseudo-class.
//   https://www.w3.org/TR/selectors4/#simple
class SimpleSelector : public Selector {
 public:
  SimpleSelector(SimpleSelectorType type, base_token::Token prefix,
                 base_token::Token text)
      : type_(type), prefix_(prefix), text_(text), value_("") {}
  SimpleSelector(SimpleSelectorType type, base_token::Token prefix,
                 base_token::Token text, const std::string& value)
      : type_(type), prefix_(prefix), text_(text), value_(value) {}
  ~SimpleSelector() override {}

  // From Selector.
  SimpleSelector* AsSimpleSelector() override { return this; }

  // Rest of public methods.

  // Used to sort simple selectors when normalizing compound selector.
  SimpleSelectorType type() const { return type_; }

  // Returns token representation of the selector.
  base_token::Token prefix() const { return prefix_; }
  base_token::Token text() const { return text_; }
  base_token::Token value() const { return base_token::Token(value_); }

  virtual PseudoElement* AsPseudoElement() { return NULL; }
  virtual UniversalSelector* AsUniversalSelector() { return NULL; }

  virtual bool AlwaysRequiresRuleMatchingVerificationVisit() const {
    return false;
  }

  virtual CompoundSelector* GetContainedCompoundSelector() const {
    return NULL;
  }

  // Used to index selector tree node's children.
  virtual void IndexSelectorTreeNode(SelectorTree::Node* parent_node,
                                     SelectorTree::Node* child_node,
                                     CombinatorType combinator) = 0;

 private:
  SimpleSelectorType type_;
  base_token::Token prefix_;
  base_token::Token text_;
  std::string value_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_SIMPLE_SELECTOR_H_

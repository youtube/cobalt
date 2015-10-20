/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CSSOM_SIMPLE_SELECTOR_H_
#define CSSOM_SIMPLE_SELECTOR_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/cssom/combinator.h"
#include "cobalt/cssom/selector.h"
#include "cobalt/cssom/selector_tree.h"

namespace cobalt {
namespace cssom {

class ClassSelector;
class IdSelector;
class PseudoClass;
class PseudoElement;
class TypeSelector;

// The order used in normalizing simple selectors in a compound selector.
enum SimpleSelectorRanks {
  kTypeSelectorRank,
  kClassSelectorRank,
  kIdSelectorRank,
  kPseudoClassRank,
  kPseudoElementRank,
};

// A simple selector is either a type selector, universal selector, attribute
// selector, class selector, ID selector, or pseudo-class.
//   http://www.w3.org/TR/selectors4/#simple
class SimpleSelector : public Selector {
 public:
  ~SimpleSelector() OVERRIDE {}

  // From Selector.
  SimpleSelector* AsSimpleSelector() OVERRIDE { return this; }

  // Rest of public methods.

  virtual TypeSelector* AsTypeSelector() { return NULL; }
  virtual ClassSelector* AsClassSelector() { return NULL; }
  virtual IdSelector* AsIdSelector() { return NULL; }
  virtual PseudoClass* AsPseudoClass() { return NULL; }
  virtual PseudoElement* AsPseudoElement() { return NULL; }

  // Used to sort simple selectors when normalizing compound selector.
  virtual int GetRank() const = 0;

  // Returns string representation of the selector.
  virtual std::string GetSelectorText() const = 0;

  // Used to index selector tree node's children.
  virtual void IndexSelectorTreeNode(SelectorTree::Node* parent_node,
                                     SelectorTree::Node* child_node,
                                     CombinatorType combinator) = 0;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_SIMPLE_SELECTOR_H_

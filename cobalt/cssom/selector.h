/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef CSSOM_SELECTOR_H_
#define CSSOM_SELECTOR_H_

#include <string>

#include "base/memory/scoped_vector.h"
#include "cobalt/cssom/combinator.h"
#include "cobalt/cssom/selector_tree.h"
#include "cobalt/cssom/specificity.h"

namespace cobalt {
namespace cssom {

class AfterPseudoElement;
class BeforePseudoElement;
class ClassSelector;
class ComplexSelector;
class CompoundSelector;
class EmptyPseudoClass;
class IdSelector;
class SelectorVisitor;
class TypeSelector;

// Selectors are patterns that match against elements in a tree, and as such
// form one of several technologies that can be used to select nodes in an XML
// document.
//   http://www.w3.org/TR/selectors4/
class Selector {
 public:
  virtual ~Selector() {}

  virtual void Accept(SelectorVisitor* visitor) = 0;

  // A selector's specificity is calculated as follows:
  // Count the number of ID selectors in the selector (A)
  // Count the number of class selectors, attributes selectors, and
  // pseudo-classes in the selector (B)
  // Count the number of type selectors and pseudo-elements in the selector (C)
  //   http://www.w3.org/TR/selectors4/#specificity
  virtual Specificity GetSpecificity() const = 0;

  virtual ComplexSelector* AsComplexSelector() { return NULL; }
  virtual CompoundSelector* AsCompoundSelector() { return NULL; }
  virtual TypeSelector* AsTypeSelector() { return NULL; }
  virtual ClassSelector* AsClassSelector() { return NULL; }
  virtual IdSelector* AsIdSelector() { return NULL; }
  virtual EmptyPseudoClass* AsEmptyPseudoClass() { return NULL; }
  virtual AfterPseudoElement* AsAfterPseudoElement() { return NULL; }
  virtual BeforePseudoElement* AsBeforePseudoElement() { return NULL; }

  // TODO(***REMOVED***, b/24988156): Move the following to SimpleSelector.

  // Used to sort simple selectors when normalizing compound selector.
  virtual int GetRank() const { return 0; }

  // Returns string representation of the selector.
  virtual std::string GetSelectorText() const { return ""; }

  // Used to index selector tree node's children.
  virtual void IndexSelectorTreeNode(SelectorTree::Node* parent_node,
                                     SelectorTree::Node* child_node,
                                     CombinatorType combinator) {
    UNREFERENCED_PARAMETER(parent_node);
    UNREFERENCED_PARAMETER(child_node);
    UNREFERENCED_PARAMETER(combinator);
  }
};

typedef ScopedVector<Selector> Selectors;

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_SELECTOR_H_

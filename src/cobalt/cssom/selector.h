// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_SELECTOR_H_
#define COBALT_CSSOM_SELECTOR_H_

#include <memory>

#include "cobalt/cssom/specificity.h"

namespace cobalt {
namespace cssom {

class ComplexSelector;
class CompoundSelector;
class SelectorVisitor;
class SimpleSelector;

// Selectors are patterns that match against elements in a tree, and as such
// form one of several technologies that can be used to select nodes in an XML
// document.
//   https://www.w3.org/TR/selectors4/
class Selector {
 public:
  virtual ~Selector() {}

  virtual void Accept(SelectorVisitor* visitor) = 0;

  virtual ComplexSelector* AsComplexSelector() { return NULL; }
  virtual CompoundSelector* AsCompoundSelector() { return NULL; }
  virtual SimpleSelector* AsSimpleSelector() { return NULL; }

  // A selector's specificity is calculated as follows:
  // Count the number of ID selectors in the selector (A)
  // Count the number of class selectors, attributes selectors, and
  // pseudo-classes in the selector (B)
  // Count the number of type selectors and pseudo-elements in the selector (C)
  //   https://www.w3.org/TR/selectors4/#specificity
  virtual Specificity GetSpecificity() const = 0;
};

typedef std::vector<std::unique_ptr<Selector>> Selectors;

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_SELECTOR_H_

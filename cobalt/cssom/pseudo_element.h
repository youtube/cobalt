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

#ifndef COBALT_CSSOM_PSEUDO_ELEMENT_H_
#define COBALT_CSSOM_PSEUDO_ELEMENT_H_

#include "base/compiler_specific.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/selector_tree.h"
#include "cobalt/cssom/simple_selector.h"
#include "cobalt/cssom/specificity.h"

namespace cobalt {
namespace cssom {

class AfterPseudoElement;
class BeforePseudoElement;

// Pseudo-elements create abstractions about the document tree beyond those
// specified by the document language. For instance, document languages do not
// offer mechanisms to access the first letter or first line of an element's
// content. Pseudo-elements allow authors to refer to this otherwise
// inaccessible information. Pseudo-elements may also provide authors a way to
// refer to content that does not exist in the source document (e.g., the
// ::before and ::after pseudo-elements give access to generated content in
// CSS).
//   https://www.w3.org/TR/selectors4/#pseudo-elements
class PseudoElement : public SimpleSelector {
 public:
  explicit PseudoElement(base::Token text)
      : SimpleSelector(kPseudoElement,
                       base::Tokens::pseudo_element_selector_prefix(), text) {}
  ~PseudoElement() override {}

  // From Selector.
  Specificity GetSpecificity() const override { return Specificity(0, 0, 1); }

  // From SimpleSelector.
  PseudoElement* AsPseudoElement() override { return this; }
  void IndexSelectorTreeNode(SelectorTree::Node* /* parent_node */,
                             SelectorTree::Node* /* child_node */,
                             CombinatorType /* combinator */) override {}

  // Rest of public methods.
  virtual AfterPseudoElement* AsAfterPseudoElement() { return NULL; }
  virtual BeforePseudoElement* AsBeforePseudoElement() { return NULL; }
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_PSEUDO_ELEMENT_H_

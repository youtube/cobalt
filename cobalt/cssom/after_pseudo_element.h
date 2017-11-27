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

#ifndef COBALT_CSSOM_AFTER_PSEUDO_ELEMENT_H_
#define COBALT_CSSOM_AFTER_PSEUDO_ELEMENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/pseudo_element.h"

namespace cobalt {
namespace cssom {

class SelectorVisitor;

// Authors specify the style and location of generated content with the :before
// and :after pseudo-elements. As their names indicate, the :before and :after
// pseudo-elements specify the location of content before and after an element's
// document tree content. The 'content' property, in conjunction with these
// pseudo-elements, specifies what is inserted.
//   https://www.w3.org/TR/CSS21/generate.html#before-after-content
class AfterPseudoElement : public PseudoElement {
 public:
  AfterPseudoElement()
      : PseudoElement(base::Tokens::after_pseudo_element_selector()) {}
  ~AfterPseudoElement() override {}

  // From Selector.
  void Accept(SelectorVisitor* visitor) override;

  // From PseudoElement.
  AfterPseudoElement* AsAfterPseudoElement() override { return this; }

 private:
  DISALLOW_COPY_AND_ASSIGN(AfterPseudoElement);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_AFTER_PSEUDO_ELEMENT_H_

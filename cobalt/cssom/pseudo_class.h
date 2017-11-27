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

#ifndef COBALT_CSSOM_PSEUDO_CLASS_H_
#define COBALT_CSSOM_PSEUDO_CLASS_H_

#include "base/compiler_specific.h"
#include "cobalt/cssom/simple_selector.h"
#include "cobalt/cssom/specificity.h"

namespace cobalt {
namespace cssom {

class ActivePseudoClass;
class EmptyPseudoClass;
class FocusPseudoClass;
class HoverPseudoClass;
class NotPseudoClass;

// The pseudo-class concept is introduced to permit selection based on
// information that lies outside of the document tree or that can be awkward or
// impossible to express using the other simple selectors.
//   https://www.w3.org/TR/selectors4/#pseudo-classes
class PseudoClass : public SimpleSelector {
 public:
  explicit PseudoClass(base::Token text)
      : SimpleSelector(kPseudoClass,
                       base::Tokens::pseudo_class_selector_prefix(), text) {}
  ~PseudoClass() override {}

  // From Selector.
  Specificity GetSpecificity() const override { return Specificity(0, 1, 0); }

  // Rest of public methods.
  virtual ActivePseudoClass* AsActivePseudoClass() { return NULL; }
  virtual EmptyPseudoClass* AsEmptyPseudoClass() { return NULL; }
  virtual FocusPseudoClass* AsFocusPseudoClass() { return NULL; }
  virtual HoverPseudoClass* AsHoverPseudoClass() { return NULL; }
  virtual NotPseudoClass* AsNotPseudoClass() { return NULL; }
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_PSEUDO_CLASS_H_

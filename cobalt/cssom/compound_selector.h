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

#ifndef CSSOM_COMPOUND_SELECTOR_H_
#define CSSOM_COMPOUND_SELECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/cssom/selector.h"
#include "cobalt/cssom/specificity.h"

namespace cobalt {
namespace cssom {

class SelectorVisitor;
class SimpleSelector;
class PseudoElement;

// A compound selector is a chain of simple selectors that are not separated by
// a combinator.
//   http://www.w3.org/TR/selectors4/#compound
class CompoundSelector : public Selector {
 public:
  typedef ScopedVector<SimpleSelector> SimpleSelectors;

  CompoundSelector();
  ~CompoundSelector() OVERRIDE;

  // From Selector.
  void Accept(SelectorVisitor* visitor) OVERRIDE;
  CompoundSelector* AsCompoundSelector() OVERRIDE { return this; }
  Specificity GetSpecificity() const OVERRIDE { return specificity_; }

  // Rest of public methods.

  void AppendSelector(scoped_ptr<SimpleSelector> selector);
  const SimpleSelectors& simple_selectors() { return simple_selectors_; }
  PseudoElement* pseudo_element() { return pseudo_element_; }
  const std::string& GetNormalizedSelectorText();

 private:
  bool should_normalize_;
  PseudoElement* pseudo_element_;

  SimpleSelectors simple_selectors_;
  Specificity specificity_;
  std::string normalized_selector_text_;

  DISALLOW_COPY_AND_ASSIGN(CompoundSelector);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_COMPOUND_SELECTOR_H_

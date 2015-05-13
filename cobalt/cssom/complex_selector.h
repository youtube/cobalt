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

#ifndef CSSOM_COMPLEX_SELECTOR_H_
#define CSSOM_COMPLEX_SELECTOR_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/cssom/adjacent_selector.h"
#include "cobalt/cssom/combinator.h"
#include "cobalt/cssom/compound_selector.h"
#include "cobalt/cssom/selector.h"

namespace cobalt {
namespace cssom {

// A complex selector is a chain of one or more adjacent selectors separated by
// adjacent combinators.
//   http://www.w3.org/TR/selectors4/#complex
class ComplexSelector : public Selector {
 public:
  ComplexSelector() {}
  ~ComplexSelector() OVERRIDE {}

  Specificity GetSpecificity() const OVERRIDE { return specificity_; }

  void Accept(SelectorVisitor* visitor) OVERRIDE;

  AdjacentSelector* last_selector() { return last_selector_.get(); }

  const Combinators& combinators() { return combinators_; }

  // For a chain of selectors separated by combinators, AppendSelector should be
  // first called with the left most selector, then AppendCombinatorAndSelector
  // should be called with each (combinator, selector) pair that follows.
  void AppendSelector(scoped_ptr<CompoundSelector> selector);
  void AppendCombinatorAndSelector(scoped_ptr<Combinator> combinator,
                                   scoped_ptr<CompoundSelector> selector);

 private:
  scoped_ptr<AdjacentSelector> last_selector_;
  Combinators combinators_;
  Specificity specificity_;

  DISALLOW_COPY_AND_ASSIGN(ComplexSelector);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_COMPLEX_SELECTOR_H_

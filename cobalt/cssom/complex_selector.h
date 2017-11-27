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

#ifndef COBALT_CSSOM_COMPLEX_SELECTOR_H_
#define COBALT_CSSOM_COMPLEX_SELECTOR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/cssom/selector.h"
#include "cobalt/cssom/specificity.h"

namespace cobalt {
namespace cssom {

class Combinator;
class CompoundSelector;
class SelectorVisitor;

// A complex selector is a chain of one or more compound selectors separated by
// combinators.
//   https://www.w3.org/TR/selectors4/#complex
class ComplexSelector : public Selector {
 public:
  static const int kCombinatorLimit;

  ComplexSelector()
      : last_selector_(NULL),
        combinator_count_(0),
        combinator_limit_exceeded_(false) {}
  ~ComplexSelector() override {}

  // From Selector.
  void Accept(SelectorVisitor* visitor) override;
  Specificity GetSpecificity() const override { return specificity_; }
  ComplexSelector* AsComplexSelector() override { return this; }

  // Rest of public methods.

  CompoundSelector* first_selector() { return first_selector_.get(); }
  CompoundSelector* last_selector() { return last_selector_; }

  int combinator_count() {
    return combinator_count_;
  }

  // For a chain of compound selectors separated by combinators, AppendSelector
  // should be first called with the left most compound selector, then
  // AppendCombinatorAndSelector should be called with each (combinator,
  // compound selector) pair that follows.
  void AppendSelector(scoped_ptr<CompoundSelector> compound_selector);
  void AppendCombinatorAndSelector(
      scoped_ptr<Combinator> combinator,
      scoped_ptr<CompoundSelector> compound_selector);

 private:
  CompoundSelector* last_selector_;

  scoped_ptr<CompoundSelector> first_selector_;
  Specificity specificity_;

  int combinator_count_;
  bool combinator_limit_exceeded_;

  DISALLOW_COPY_AND_ASSIGN(ComplexSelector);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_COMPLEX_SELECTOR_H_

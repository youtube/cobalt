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

#include "cobalt/cssom/complex_selector.h"

#include "cobalt/cssom/combinator.h"
#include "cobalt/cssom/compound_selector.h"
#include "cobalt/cssom/selector_visitor.h"

namespace cobalt {
namespace cssom {

void ComplexSelector::Accept(SelectorVisitor* visitor) {
  visitor->VisitComplexSelector(this);
}

void ComplexSelector::AppendSelector(
    scoped_ptr<CompoundSelector> compound_selector) {
  DCHECK(!first_selector_);
  specificity_.AddFrom(compound_selector->GetSpecificity());
  first_selector_ = compound_selector.Pass();
  last_selector_ = first_selector_.get();
}

void ComplexSelector::AppendCombinatorAndSelector(
    scoped_ptr<Combinator> combinator,
    scoped_ptr<CompoundSelector> compound_selector) {
  DCHECK(first_selector_);
  specificity_.AddFrom(compound_selector->GetSpecificity());

  combinator->set_left_selector(last_selector_);
  compound_selector->set_left_combinator(combinator.get());

  combinator->set_right_selector(compound_selector.Pass());
  last_selector_->set_right_combinator(combinator.Pass());

  last_selector_ = last_selector_->right_combinator()->right_selector();
}

}  // namespace cssom
}  // namespace cobalt

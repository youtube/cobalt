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

#include "cobalt/cssom/selector_visitor.h"

namespace cobalt {
namespace cssom {

void ComplexSelector::Accept(SelectorVisitor* visitor) {
  visitor->VisitComplexSelector(this);
}

void ComplexSelector::AppendSelector(scoped_ptr<CompoundSelector> selector) {
  last_selector_.reset(new AdjacentSelector());
  last_selector_->AppendSelector(selector.Pass());
}

void ComplexSelector::AppendCombinatorAndSelector(
    scoped_ptr<Combinator> combinator, scoped_ptr<CompoundSelector> selector) {
  if (combinator->IsAdjacentCombinator()) {
    last_selector_->AppendCombinatorAndSelector(combinator.Pass(),
                                                selector.Pass());
  } else {
    combinator->set_selector(last_selector_.PassAs<Selector>());
    combinators_.push_back(combinator.release());
    last_selector_.reset(new AdjacentSelector());
    last_selector_->AppendSelector(selector.Pass());
  }
}

}  // namespace cssom
}  // namespace cobalt

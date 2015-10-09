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

#include "cobalt/cssom/compound_selector.h"

#include <algorithm>

#include "cobalt/cssom/after_pseudo_element.h"
#include "cobalt/cssom/before_pseudo_element.h"
#include "cobalt/cssom/selector_visitor.h"

namespace cobalt {
namespace cssom {
namespace {

bool CompareSelectorRank(const Selector* lhs, const Selector* rhs) {
  return (lhs->GetRank() < rhs->GetRank()) ||
         (lhs->GetRank() == rhs->GetRank() &&
          lhs->GetSelectorText() < rhs->GetSelectorText());
}

}  // namespace

void CompoundSelector::Accept(SelectorVisitor* visitor) {
  visitor->VisitCompoundSelector(this);
}

void CompoundSelector::AppendSelector(scoped_ptr<Selector> selector) {
  specificity_.AddFrom(selector->GetSpecificity());
  if (selector->AsAfterPseudoElement() || selector->AsBeforePseudoElement()) {
    DCHECK(!pseudo_element_);
    pseudo_element_ = selector.get();
  }
  selectors_.push_back(selector.release());
  should_normalize_ = true;
}

const std::string& CompoundSelector::GetNormalizedSelectorText() {
  if (should_normalize_) {
    should_normalize_ = false;
    std::sort(selectors_.begin(), selectors_.end(), CompareSelectorRank);
    normalized_selector_text_ = "";
    for (Selectors::iterator it = selectors_.begin(); it != selectors_.end();
         ++it) {
      normalized_selector_text_ += (*it)->GetSelectorText();
    }
  }
  return normalized_selector_text_;
}

}  // namespace cssom
}  // namespace cobalt

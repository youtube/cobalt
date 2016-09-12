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

#include "cobalt/cssom/pseudo_element.h"
#include "cobalt/cssom/selector_visitor.h"

namespace cobalt {
namespace cssom {
namespace {

bool SimpleSelectorsLessThan(const SimpleSelector* lhs,
                             const SimpleSelector* rhs) {
  if (lhs->type() < rhs->type()) {
    return true;
  }
  if (lhs->type() > rhs->type()) {
    return false;
  }

  return (lhs->prefix() < rhs->prefix()) ||
         (lhs->prefix() == rhs->prefix() && lhs->text() < rhs->text());
}

}  // namespace

CompoundSelector::CompoundSelector()
    : left_combinator_(NULL),
      has_pseudo_element_(false),
      requires_rule_matching_verification_visit_(false) {}

CompoundSelector::~CompoundSelector() {}

void CompoundSelector::Accept(SelectorVisitor* visitor) {
  visitor->VisitCompoundSelector(this);
}

void CompoundSelector::AppendSelector(
    scoped_ptr<SimpleSelector> simple_selector) {
  specificity_.AddFrom(simple_selector->GetSpecificity());
  has_pseudo_element_ =
      has_pseudo_element_ || simple_selector->AsPseudoElement() != NULL;

  // There are two cases where the selectors require a visit:
  // 1. There are multiple selectors. Gathering only tests against the first
  //    selector and the later selectors must also be verified to match.
  // 2. The single selector's AlwaysRequiresRuleMatchingVerificationVisit() call
  //    returns true. This indicates that being gathered as a candidate is not
  //    sufficient to prove a match and that additional verification checks are
  //    required.
  requires_rule_matching_verification_visit_ =
      requires_rule_matching_verification_visit_ ||
      !simple_selectors_.empty() ||
      simple_selector->AlwaysRequiresRuleMatchingVerificationVisit();

  bool should_sort =
      !simple_selectors_.empty() &&
      SimpleSelectorsLessThan(simple_selector.get(), simple_selectors_.back());
  simple_selectors_.push_back(simple_selector.release());
  if (should_sort) {
    std::sort(simple_selectors_.begin(), simple_selectors_.end(),
              SimpleSelectorsLessThan);
  }
}

void CompoundSelector::set_right_combinator(scoped_ptr<Combinator> combinator) {
  right_combinator_ = combinator.Pass();
}

}  // namespace cssom
}  // namespace cobalt

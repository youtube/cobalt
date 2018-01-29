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

#ifndef COBALT_CSSOM_COMPOUND_SELECTOR_H_
#define COBALT_CSSOM_COMPOUND_SELECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/cssom/selector.h"
#include "cobalt/cssom/simple_selector.h"
#include "cobalt/cssom/specificity.h"

namespace cobalt {
namespace cssom {

class Combinator;
class PseudoElement;
class SelectorVisitor;

// A compound selector is a chain of simple selectors that are not separated by
// a combinator.
//   https://www.w3.org/TR/selectors4/#compound
class CompoundSelector : public Selector {
 public:
  typedef ScopedVector<SimpleSelector> SimpleSelectors;

  CompoundSelector();
  ~CompoundSelector() override;

  bool operator<(const CompoundSelector& that) const;

  // From Selector.
  void Accept(SelectorVisitor* visitor) override;
  CompoundSelector* AsCompoundSelector() override { return this; }
  Specificity GetSpecificity() const override { return specificity_; }

  // Rest of public methods.

  void AppendSelector(scoped_ptr<SimpleSelector> selector);
  const SimpleSelectors& simple_selectors() { return simple_selectors_; }
  PseudoElement* pseudo_element() {
    if (has_pseudo_element_) {
      for (SimpleSelectors::iterator iter = simple_selectors_.begin();
           iter != simple_selectors_.end(); ++iter) {
        if ((*iter)->AsPseudoElement()) {
          return (*iter)->AsPseudoElement();
        }
      }
    }
    return NULL;
  }

  Combinator* left_combinator() { return left_combinator_; }
  void set_left_combinator(Combinator* combinator) {
    left_combinator_ = combinator;
  }

  Combinator* right_combinator() { return right_combinator_.get(); }
  void set_right_combinator(scoped_ptr<Combinator> combinator);

  bool requires_rule_matching_verification_visit() const {
    return requires_rule_matching_verification_visit_;
  }

 private:
  Combinator* left_combinator_;
  scoped_ptr<Combinator> right_combinator_;
  SimpleSelectors simple_selectors_;
  Specificity specificity_;
  bool has_pseudo_element_;
  // This flag tracks whether or not during rule matching, after the initial
  // candidate gathering phase, the simple selectors require additional checks
  // during the verification phase to determine a match; otherwise, the act of
  // being gathered itself proves the match.
  // There are two cases where the selectors require a visit:
  // 1. There are multiple selectors. Gathering only tests against the first
  //    selector and the later selectors must also be verified to match.
  // 2. The single selector's AlwaysRequiresRuleMatchingVerificationVisit() call
  //    returns true. This indicates that being gathered as a candidate is not
  //    sufficient to prove a match and that additional verification checks are
  //    required.
  bool requires_rule_matching_verification_visit_;

  DISALLOW_COPY_AND_ASSIGN(CompoundSelector);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_COMPOUND_SELECTOR_H_

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

#ifndef DOM_PSEUDO_ELEMENT_H_
#define DOM_PSEUDO_ELEMENT_H_

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/computed_style_state.h"
#include "cobalt/cssom/css_style_rule.h"

namespace cobalt {
namespace dom {

// Pseudo-elements create abstractions about the document tree beyond those
// specified by the document language.
// Pseudo elements are css selectors that can be applied to a DOM element and
// then cause sub-elements to be created that have their own style and/or
// content, but that do not exist in the DOM tree.
// Examples are generated content elements with '::after' and '::before', or
// styles that affect parts of the DOM element they are attached to, such as
// '::first-line'.
//   http://www.w3.org/TR/selectors4/#pseudo-elements
//   http://www.w3.org/TR/CSS21/generate.html#before-after-content
// This class adds a container for the DOM state needed for pseudo elements.
class PseudoElement {
 public:
  PseudoElement() : computed_style_state_(new cssom::ComputedStyleState()) {}
  ~PseudoElement() {}

  // Used by layout engine to cache the computed values.
  // See http://www.w3.org/TR/css-cascade-3/#computed for the definition of
  // computed value.
  scoped_refptr<cssom::ComputedStyleState>& computed_style_state() {
    return computed_style_state_;
  }

  scoped_refptr<const cssom::CSSStyleDeclarationData> computed_style() const {
    return computed_style_state_->style();
  }
  void set_computed_style(
      scoped_refptr<cssom::CSSStyleDeclarationData> computed_style) {
    computed_style_state_->set_style(computed_style);
  }

  cssom::RulesWithCascadePriority* matching_rules() { return &matching_rules_; }

  cssom::TransitionSet* transitions() const {
    return computed_style_state_->transitions();
  }

  void ClearMatchingRules() { matching_rules_.clear(); }

 private:
  scoped_refptr<cssom::ComputedStyleState> computed_style_state_;
  cssom::RulesWithCascadePriority matching_rules_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_PSEUDO_ELEMENT_H_

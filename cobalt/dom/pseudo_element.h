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

#ifndef COBALT_DOM_PSEUDO_ELEMENT_H_
#define COBALT_DOM_PSEUDO_ELEMENT_H_

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/cssom/animation_set.h"
#include "cobalt/cssom/css_computed_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_transition_set.h"
#include "cobalt/dom/css_animations_adapter.h"
#include "cobalt/dom/css_transitions_adapter.h"

namespace cobalt {
namespace dom {

class HTMLElement;

// Pseudo-elements create abstractions about the document tree beyond those
// specified by the document language.
// Pseudo elements are css selectors that can be applied to a DOM element and
// then cause sub-elements to be created that have their own style and/or
// content, but that do not exist in the DOM tree.
// Examples are generated content elements with '::after' and '::before', or
// styles that affect parts of the DOM element they are attached to, such as
// '::first-line'.
//   https://www.w3.org/TR/selectors4/#pseudo-elements
//   https://www.w3.org/TR/CSS21/generate.html#before-after-content
// This class adds a container for the DOM state needed for pseudo elements.
class PseudoElement {
 public:
  explicit PseudoElement(HTMLElement* parent_element);
  ~PseudoElement() {}

  // Used by layout engine to cache the computed values.
  // See https://www.w3.org/TR/css-cascade-3/#computed for the definition of
  // computed value.
  scoped_refptr<cssom::CSSComputedStyleDeclaration>&
  css_computed_style_declaration() {
    return css_computed_style_declaration_;
  }

  scoped_refptr<const cssom::CSSComputedStyleData> computed_style() const {
    return css_computed_style_declaration_->data();
  }

  cssom::RulesWithCascadePrecedence* matching_rules() {
    return &matching_rules_;
  }

  cssom::TransitionSet* css_transitions() { return &css_transitions_.value(); }
  cssom::AnimationSet* css_animations() { return &css_animations_.value(); }

  const scoped_refptr<web_animations::AnimationSet>& animations() {
    return animations_;
  }

  HTMLElement* parent_element() { return parent_element_; }
  void ClearMatchingRules() { matching_rules_.clear(); }

  void set_layout_boxes(scoped_ptr<LayoutBoxes> layout_boxes) {
    layout_boxes_ = layout_boxes.Pass();
  }
  LayoutBoxes* layout_boxes() const { return layout_boxes_.get(); }
  void reset_layout_boxes() { layout_boxes_.reset(); }

 private:
  HTMLElement* parent_element_;

  scoped_refptr<web_animations::AnimationSet> animations_;
  scoped_refptr<cssom::CSSComputedStyleDeclaration>
      css_computed_style_declaration_;

  base::optional<CSSTransitionsAdapter> transitions_adapter_;
  base::optional<cssom::TransitionSet> css_transitions_;

  base::optional<CSSAnimationsAdapter> animations_adapter_;
  base::optional<cssom::AnimationSet> css_animations_;

  cssom::RulesWithCascadePrecedence matching_rules_;

  // This contains information about the boxes generated from the element.
  scoped_ptr<LayoutBoxes> layout_boxes_;

  // PseudoElement is a friend of Animatable so that animatable can insert and
  // remove animations into PseudoElement's set of animations.
  friend class DOMAnimatable;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PSEUDO_ELEMENT_H_

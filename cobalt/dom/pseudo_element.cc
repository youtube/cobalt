// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "base/logging.h"
#include "cobalt/dom/dom_animatable.h"
#include "cobalt/dom/pseudo_element.h"

namespace cobalt {
namespace dom {

PseudoElement::PseudoElement(HTMLElement* parent_element)
    : parent_element_(parent_element),
      animations_(new web_animations::AnimationSet()),
      css_computed_style_declaration_(new cssom::CSSComputedStyleDeclaration()),
      computed_style_invalid_(true) {
  DCHECK(parent_element_);
  css_computed_style_declaration_->set_animations(animations_);

  transitions_adapter_.emplace(new DOMAnimatable(this));
  css_transitions_.emplace(&transitions_adapter_.value());

  animations_adapter_.emplace(new DOMAnimatable(this));
  css_animations_.emplace(&animations_adapter_.value());
}

}  // namespace dom
}  // namespace cobalt

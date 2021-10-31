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

#include "cobalt/dom/dom_animatable.h"

namespace cobalt {
namespace dom {

scoped_refptr<web_animations::AnimationTimeline>
DOMAnimatable::GetDefaultTimeline() const {
  if (element_) {
    return element_->node_document()->timeline();
  } else if (pseudo_element_) {
    return pseudo_element_->parent_element()->node_document()->timeline();
  } else {
    return scoped_refptr<web_animations::AnimationTimeline>();
  }
}

void DOMAnimatable::Register(web_animations::Animation* animation) {
  if (element_) {
    element_->animations()->AddAnimation(animation);
  } else if (pseudo_element_) {
    pseudo_element_->animations()->AddAnimation(animation);
  }
}

void DOMAnimatable::Deregister(web_animations::Animation* animation) {
  if (element_) {
    element_->animations()->RemoveAnimation(animation);
  } else if (pseudo_element_) {
    pseudo_element_->animations()->RemoveAnimation(animation);
  }
}

Element* DOMAnimatable::GetEventTarget() {
  if (element_) {
    return element_;
  } else if (pseudo_element_) {
    return pseudo_element_->parent_element();
  }
  NOTREACHED();
  return NULL;
}

scoped_refptr<const cssom::CSSComputedStyleData>
DOMAnimatable::GetComputedStyle() const {
  if (pseudo_element_) {
    return pseudo_element_->computed_style();
  }

  DCHECK(element_);
  DCHECK(element_->AsHTMLElement());
  return element_->AsHTMLElement()->computed_style();
}

const cssom::CSSKeyframesRule::NameMap& DOMAnimatable::GetKeyframesMap() const {
  if (pseudo_element_) {
    DCHECK(pseudo_element_->parent_element());
    DCHECK(pseudo_element_->parent_element()->node_document());
    return pseudo_element_->parent_element()->node_document()->keyframes_map();
  }

  DCHECK(element_);
  DCHECK(element_->node_document());
  return element_->node_document()->keyframes_map();
}

}  // namespace dom
}  // namespace cobalt

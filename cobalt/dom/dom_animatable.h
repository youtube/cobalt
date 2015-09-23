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

#ifndef DOM_DOM_ANIMATABLE_H_
#define DOM_DOM_ANIMATABLE_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/pseudo_element.h"
#include "cobalt/web_animations/animatable.h"

namespace cobalt {
namespace dom {

// In order to keep the Web Animations module not dependent on DOM, Animatable
// is left abstract, and is subclassed within DOM here in order to connect
// it to DOM.  Note that this informally implements the specification statement
// that Element and PseudoElement should "implement" Animatable, as indicated
// in the IDL Index:
// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#idl-index
class DOMAnimatable : public web_animations::Animatable {
 public:
  explicit DOMAnimatable(Element* element)
      : element_(element), pseudo_element_(NULL) {}

  explicit DOMAnimatable(PseudoElement* pseudo_element)
      : pseudo_element_(pseudo_element) {}

  // Returns the default timeline associated with the document that is
  // associated with the underlying Element or PseudoElement.
  scoped_refptr<web_animations::AnimationTimeline> GetDefaultTimeline()
      const OVERRIDE;

  // Registers an animation with this Animatable (which may internally be
  // either an HTMLElement or a PseudoElement).
  void Register(web_animations::Animation* animation) OVERRIDE;
  void Deregister(web_animations::Animation* animation) OVERRIDE;

 private:
  Element* element_;
  PseudoElement* pseudo_element_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_DOM_ANIMATABLE_H_

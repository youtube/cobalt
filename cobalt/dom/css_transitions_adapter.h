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

#ifndef DOM_CSS_TRANSITIONS_ADAPTER_H_
#define DOM_CSS_TRANSITIONS_ADAPTER_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/cssom/css_property_definitions.h"
#include "cobalt/cssom/css_transition.h"
#include "cobalt/cssom/css_transition_set.h"
#include "cobalt/web_animations/animatable.h"
#include "cobalt/web_animations/animation.h"
#include "cobalt/web_animations/animation_timeline.h"

namespace cobalt {
namespace dom {

// This class contains logic to adapt CSS Transitions to the Web Animations
// framework.  It watches for CSS Transition events, and handles them by
// appropriately creating and destroying Animation objects on the target element
// and timeline.
class CSSTransitionsAdapter : public cssom::TransitionSet::EventHandler {
 public:
  CSSTransitionsAdapter(
      const scoped_refptr<web_animations::Animatable>& target);

  void OnTransitionStarted(const cssom::Transition& transition) OVERRIDE;
  void OnTransitionRemoved(const cssom::Transition& transition) OVERRIDE;

 private:
  typedef std::map<cssom::PropertyKey,
                   scoped_refptr<web_animations::Animation> >
      PropertyValueAnimationMap;

  scoped_refptr<web_animations::Animatable> animatable_;

  // The animation map tracks animations that were created by transitions.
  // As transitions are created and removed, we update this animation map
  // per property value.
  PropertyValueAnimationMap animation_map_;

  DISALLOW_COPY_AND_ASSIGN(CSSTransitionsAdapter);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_CSS_TRANSITIONS_ADAPTER_H_

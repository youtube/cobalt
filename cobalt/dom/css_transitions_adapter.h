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

#ifndef COBALT_DOM_CSS_TRANSITIONS_ADAPTER_H_
#define COBALT_DOM_CSS_TRANSITIONS_ADAPTER_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/cssom/css_transition.h"
#include "cobalt/cssom/css_transition_set.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/web_animations/animation.h"
#include "cobalt/web_animations/animation_timeline.h"

namespace cobalt {
namespace dom {

class DOMAnimatable;

// This class contains logic to adapt CSS Transitions to the Web Animations
// framework.  It watches for CSS Transition events, and handles them by
// appropriately creating and destroying Animation objects on the target element
// and timeline.
class CSSTransitionsAdapter : public cssom::TransitionSet::EventHandler {
 public:
  explicit CSSTransitionsAdapter(
      const scoped_refptr<dom::DOMAnimatable>& target);
  ~CSSTransitionsAdapter();

  void OnTransitionStarted(const cssom::Transition& transition) override;
  void OnTransitionRemoved(const cssom::Transition& transition) override;

 private:
  // The AnimationWithEventHandler struct maintains a reference to the Animation
  // object and also owns an Animation::EventHandler that connects animation
  // events to this CSSTransitionsAdapter object.
  struct AnimationWithEventHandler {
    AnimationWithEventHandler(
        const scoped_refptr<web_animations::Animation>& animation,
        scoped_ptr<web_animations::Animation::EventHandler> event_handler)
        : animation(animation), event_handler(event_handler.Pass()) {}
    ~AnimationWithEventHandler() {}

    scoped_refptr<web_animations::Animation> animation;
    scoped_ptr<web_animations::Animation::EventHandler> event_handler;
  };
  typedef std::map<cssom::PropertyKey, AnimationWithEventHandler*>
      PropertyValueAnimationMap;

  // Called to handle Animation events.  When a transition's corresponding
  // animation enters the after phase, we fire the transitionend event.
  void HandleAnimationEnterAfterPhase(const cssom::Transition& transition);

  scoped_refptr<dom::DOMAnimatable> animatable_;

  // The animation map tracks animations that were created by transitions.
  // As transitions are created and removed, we update this animation map
  // per property value.
  PropertyValueAnimationMap animation_map_;

  DISALLOW_COPY_AND_ASSIGN(CSSTransitionsAdapter);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CSS_TRANSITIONS_ADAPTER_H_

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

#include "cobalt/dom/css_transitions_adapter.h"

#include <memory>
#include <vector>

#include "cobalt/base/tokens.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_animatable.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/pseudo_element.h"
#include "cobalt/dom/transition_event.h"
#include "cobalt/web_animations/keyframe.h"
#include "cobalt/web_animations/keyframe_effect_read_only.h"

namespace cobalt {
namespace dom {

CSSTransitionsAdapter::CSSTransitionsAdapter(
    const scoped_refptr<dom::DOMAnimatable>& target)
    : animatable_(target) {}

CSSTransitionsAdapter::~CSSTransitionsAdapter() {
  for (PropertyValueAnimationMap::iterator iter = animation_map_.begin();
       iter != animation_map_.end(); ++iter) {
    delete iter->second;
  }
}

void CSSTransitionsAdapter::OnTransitionStarted(
    const cssom::Transition& transition, cssom::TransitionSet* transition_set) {
  // The process for constructing web animations from CSS transitions is
  // described here:
  //   https://drafts.csswg.org/date/2015-03-02/web-animations-css-integration/#css-transitions

  // Create our timing structure using the timing information from the
  // transition.
  scoped_refptr<web_animations::AnimationEffectTimingReadOnly> timing_input(
      new web_animations::AnimationEffectTimingReadOnly(
          transition.delay(), base::TimeDelta(),
          web_animations::AnimationEffectTimingReadOnly::kBoth, 0.0, 1.0,
          transition.duration(),
          web_animations::AnimationEffectTimingReadOnly::kNormal,
          cssom::TimingFunction::GetLinear()));

  // Setup a KeyframeEffect with 2 keyframes, a start and end frame that hold
  // the start and end transition property values.  In general keyframes can
  // contain values for many properties, but for transitions they will only
  // ever have the transition's target property.
  std::vector<web_animations::Keyframe::Data> frames;

  web_animations::Keyframe::Data start_frame(0.0);
  start_frame.set_easing(transition.timing_function());
  start_frame.AddPropertyValue(transition.target_property(),
                               transition.start_value());
  frames.push_back(start_frame);

  web_animations::Keyframe::Data end_frame(1.0);
  end_frame.AddPropertyValue(transition.target_property(),
                             transition.end_value());
  frames.push_back(end_frame);

  scoped_refptr<web_animations::KeyframeEffectReadOnly> keyframe_effect(
      new web_animations::KeyframeEffectReadOnly(timing_input, animatable_,
                                                 frames));

  // Finally setup and play our animation.
  scoped_refptr<web_animations::Animation> animation(
      new web_animations::Animation(keyframe_effect,
                                    animatable_->GetDefaultTimeline()));

  // Setup an event handler on the animation so we can watch for when it enters
  // the after phase, allowing us to then trigger the transitionend event.
  std::unique_ptr<web_animations::Animation::EventHandler> event_handler =
      animation->AttachEventHandler(
          base::Bind(&CSSTransitionsAdapter::HandleAnimationEnterAfterPhase,
                     base::Unretained(this), transition_set));

  // Track the animation in our map of all CSS Transitions-created animations.
  DCHECK(animation_map_.find(transition.target_property()) ==
         animation_map_.end());

  animation_map_.insert(std::make_pair(
      transition.target_property(),
      new AnimationWithEventHandler(animation, std::move(event_handler))));

  animation->Play();
}

void CSSTransitionsAdapter::OnTransitionRemoved(
    const cssom::Transition& transition,
    cssom::Transition::IsCanceled is_canceled) {
  // As described in the process for cancelling/stopping a transition here:
  //    https://dr.csswg.org/date/2015-03-02/web-animations-css-integration/Overview.html#css-transitions
  // We find the animation corresponding to the removed transition, cancel it,
  // and then remove it from our record of animations.
  if (is_canceled == cssom::Transition::kIsNotCanceled) {
    // An animation corresponding to a transition has entered the "after phase",
    // so we should correspondingly fire the transitionend event.
    //   https://drafts.csswg.org/date/2015-03-02/web-animations-css-integration/#css-transitions-events
    // Cobalt assumes that the CSSOM does not change during computed style
    // calculation. Post to dispatch the event asynchronously here to avoid
    // calling event handlers during computed style calculation, which in turn
    // may modify CSSOM and require restart of the computed style calculation.
    animatable_->GetEventTarget()->PostToDispatchEvent(
        FROM_HERE,
        new TransitionEvent(
            base::Tokens::transitionend(), transition.target_property(),
            static_cast<float>(transition.duration().InMillisecondsF())));
  }

  PropertyValueAnimationMap::iterator found =
      animation_map_.find(transition.target_property());
  DCHECK(animation_map_.end() != found);

  found->second->animation->Cancel();
  delete found->second;
  animation_map_.erase(found);
}

void CSSTransitionsAdapter::HandleAnimationEnterAfterPhase(
    cssom::TransitionSet* transition_set) {
  DCHECK(animatable_->GetDefaultTimeline()->current_time());
  base::TimeDelta current_time = base::TimeDelta::FromMillisecondsD(
      animatable_->GetDefaultTimeline()->current_time().value_or(0));
  transition_set->UpdateTransitions(current_time,
                                    *animatable_->GetComputedStyle(),
                                    *animatable_->GetComputedStyle());
}

}  // namespace dom
}  // namespace cobalt

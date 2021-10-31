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

#include "cobalt/dom/css_animations_adapter.h"

#include <memory>
#include <vector>

#include "cobalt/base/tokens.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/css_keyframes_rule.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/timing_function_list_value.h"
#include "cobalt/dom/animation_event.h"
#include "cobalt/dom/dom_animatable.h"
#include "cobalt/web_animations/keyframe.h"
#include "cobalt/web_animations/keyframe_effect_read_only.h"

namespace cobalt {
namespace dom {

CSSAnimationsAdapter::CSSAnimationsAdapter(
    const scoped_refptr<dom::DOMAnimatable>& target)
    : animatable_(target) {}

CSSAnimationsAdapter::~CSSAnimationsAdapter() {
  for (AnimationMap::iterator iter = animation_map_.begin();
       iter != animation_map_.end(); ++iter) {
    delete iter->second;
  }
}

namespace {
// Convert a cssom::Animation::FillMode enum value to a
// web_animations::AnimationEffectTimingReadOnly::FillMode value.
web_animations::AnimationEffectTimingReadOnly::FillMode CSSToWebFillMode(
    cssom::Animation::FillMode css_fill_mode) {
  switch (css_fill_mode) {
    case cssom::Animation::kNone:
      return web_animations::AnimationEffectTimingReadOnly::kNone;
    case cssom::Animation::kForwards:
      return web_animations::AnimationEffectTimingReadOnly::kForwards;
    case cssom::Animation::kBackwards:
      return web_animations::AnimationEffectTimingReadOnly::kBackwards;
    case cssom::Animation::kBoth:
      return web_animations::AnimationEffectTimingReadOnly::kBoth;
  }

  NOTREACHED();
  return web_animations::AnimationEffectTimingReadOnly::kNone;
}

// Convert a cssom::Animation::PlaybackDirection enum value to a
// web_animations::AnimationEffectTimingReadOnly::PlaybackDirection value.
web_animations::AnimationEffectTimingReadOnly::PlaybackDirection
CSSToWebDirection(cssom::Animation::PlaybackDirection css_direction) {
  switch (css_direction) {
    case cssom::Animation::kNormal:
      return web_animations::AnimationEffectTimingReadOnly::kNormal;
    case cssom::Animation::kReverse:
      return web_animations::AnimationEffectTimingReadOnly::kReverse;
    case cssom::Animation::kAlternate:
      return web_animations::AnimationEffectTimingReadOnly::kAlternate;
    case cssom::Animation::kAlternateReverse:
      return web_animations::AnimationEffectTimingReadOnly::kAlternateReverse;
  }

  NOTREACHED();
  return web_animations::AnimationEffectTimingReadOnly::kNormal;
}

scoped_refptr<cssom::TimingFunction> GetTimingFunctionFromKeyframePropertyValue(
    const scoped_refptr<cssom::PropertyValue>& value) {
  // An 'animation-timing-function' specified as a property value in a keyframe
  // means that we should use the first value (since animation-timing-function
  // is a list) as the keyframe's timing function.
  return base::polymorphic_downcast<cssom::TimingFunctionListValue*>(
             value.get())
      ->value()[0];
}

std::vector<web_animations::Keyframe::Data>
ConvertCSSKeyframesToWebAnimationsKeyframes(
    const cssom::CSSKeyframesRule& keyframes,
    const scoped_refptr<cssom::TimingFunction>& default_timing_function) {
  // https://drafts.csswg.org/date/2015-03-02/web-animations-css-integration/#conversion-of-css-keyframes
  const std::vector<cssom::CSSKeyframesRule::KeyframeInfo>&
      sorted_css_keyframes = keyframes.sorted_keyframes();

  // 1. Initialize an empty list keyframeList.
  std::vector<web_animations::Keyframe::Data> keyframe_list;
  if (sorted_css_keyframes.empty()) {
    return keyframe_list;
  }
  keyframe_list.reserve(sorted_css_keyframes.size());

  // 2. For each keyframe in keyframes:
  for (size_t i = 0; i < sorted_css_keyframes.size(); ++i) {
    const cssom::CSSKeyframesRule::KeyframeInfo& keyframe(
        sorted_css_keyframes[i]);

    // 2.1. Create a new dictionary newKeyframe.
    // 2.2. Set newKeyframe's offset to the offset defined by keyframe.
    web_animations::Keyframe::Data new_keyframe(keyframe.offset);
    bool easing_was_set = false;
    // 2.4. For each property defined in keyframe:
    for (cssom::CSSDeclaredStyleData::PropertyValues::const_iterator iter =
             keyframe.style->declared_property_values().begin();
         iter != keyframe.style->declared_property_values().end(); ++iter) {
      // 2.3. If keyframe defines an animation-timing-function, set
      //      newKeyframe's easing to the defined animation-timing-function.
      if (iter->first == cssom::kAnimationTimingFunctionProperty) {
        new_keyframe.set_easing(
            GetTimingFunctionFromKeyframePropertyValue(iter->second));
        easing_was_set = true;
      } else {
        // 2.4.1. Set newKeyframe[property] to the value associated with that
        //        property in keyframe
        new_keyframe.AddPropertyValue(iter->first, iter->second);
      }
    }

    if (!easing_was_set) {
      // If no keyframe-specific easing was specified, use the animation's
      // default easing value for this keyframe.
      new_keyframe.set_easing(default_timing_function);
    }

    // 3. Add newKeyframe to keyframeList.
    keyframe_list.push_back(new_keyframe);
  }

  return keyframe_list;
}
}  // namespace

void CSSAnimationsAdapter::OnAnimationStarted(
    const cssom::Animation& css_animation, cssom::AnimationSet* animation_set) {
  // The process for constructing web animations from CSS animations is
  // specified here:
  //   https://drafts.csswg.org/date/2015-03-02/web-animations-css-integration/#css-animations

  // Transfer the CSS Animation timing information into a Web Animations
  // AnimationEffectTimingReadOnly object.
  // https://drafts.csswg.org/date/2015-03-02/web-animations-css-integration/#web-animations-instantiation
  scoped_refptr<web_animations::AnimationEffectTimingReadOnly> timing_input(
      new web_animations::AnimationEffectTimingReadOnly(
          css_animation.delay(), base::TimeDelta(),
          CSSToWebFillMode(css_animation.fill_mode()), 0.0,
          css_animation.iteration_count(), css_animation.duration(),
          CSSToWebDirection(css_animation.direction()),
          cssom::TimingFunction::GetLinear()));

  // Construct the web_animations keyframe data from the CSS Animations keyframe
  // data.
  scoped_refptr<web_animations::KeyframeEffectReadOnly> keyframe_effect(
      new web_animations::KeyframeEffectReadOnly(
          timing_input, animatable_,
          ConvertCSSKeyframesToWebAnimationsKeyframes(
              *css_animation.keyframes(), css_animation.timing_function())));

  // Finally setup and play our animation.
  scoped_refptr<web_animations::Animation> web_animation(
      new web_animations::Animation(keyframe_effect,
                                    animatable_->GetDefaultTimeline()));

  // Setup an event handler on the animation so we can watch for when it enters
  // the after phase, allowing us to then trigger the animation events.
  std::unique_ptr<web_animations::Animation::EventHandler> event_handler =
      web_animation->AttachEventHandler(
          base::Bind(&CSSAnimationsAdapter::HandleAnimationEnterAfterPhase,
                     base::Unretained(this), animation_set));

  // Track the animation in our map of all CSS Animations-created animations.
  DCHECK(animation_map_.find(css_animation.name()) == animation_map_.end());

  animation_map_.insert(std::make_pair(
      css_animation.name(),
      new AnimationWithEventHandler(web_animation, std::move(event_handler))));

  web_animation->Play();
}

void CSSAnimationsAdapter::OnAnimationEnded(
    const cssom::Animation& css_animation) {
  // An animation has entered the "after phase", so we should correspondingly
  // fire the animationend event.
  //   https://drafts.csswg.org/date/2015-03-02/web-animations-css-integration/#css-animations-events
  // Cobalt assumes that the CSSOM does not change during computed style
  // calculation. Post to dispatch the event asynchronously here to avoid
  // calling event handlers during computed style calculation, which in turn
  // may modify CSSOM and require restart of the computed style calculation.
  animatable_->GetEventTarget()->PostToDispatchEvent(
      FROM_HERE,
      new AnimationEvent(
          base::Tokens::animationend(), css_animation.name(),
          static_cast<float>(css_animation.duration().InMillisecondsF() *
                             css_animation.iteration_count())));
}

void CSSAnimationsAdapter::OnAnimationRemoved(
    const cssom::Animation& css_animation) {
  // If a CSS Animation is removed from an element, its corresponding
  // Web Animations animation should be stopped and removed also.
  AnimationMap::iterator found = animation_map_.find(css_animation.name());
  DCHECK(animation_map_.end() != found);

  found->second->animation->Cancel();
  delete found->second;
  animation_map_.erase(found);
}

void CSSAnimationsAdapter::HandleAnimationEnterAfterPhase(
    cssom::AnimationSet* animation_set) {
  // The update processing handles signalling when an animation ends.
  DCHECK(animatable_->GetDefaultTimeline()->current_time());
  base::TimeDelta current_time = base::TimeDelta::FromMillisecondsD(
      animatable_->GetDefaultTimeline()->current_time().value_or(0));
  animation_set->Update(current_time, *animatable_->GetComputedStyle(),
                        animatable_->GetKeyframesMap());
}

}  // namespace dom
}  // namespace cobalt

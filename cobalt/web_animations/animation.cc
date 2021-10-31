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

#include <memory>

#include "cobalt/web_animations/animation.h"

#include "base/logging.h"
#include "cobalt/web_animations/animation_set.h"
#include "cobalt/web_animations/keyframe_effect_read_only.h"

namespace cobalt {
namespace web_animations {

Animation::Animation(const scoped_refptr<AnimationEffectReadOnly>& effect,
                     const scoped_refptr<AnimationTimeline>& timeline)
    : effect_(effect) {
  // Register with the keyframe effect's target so that this Animation can
  // be referenced from the target, as per the web animations specification:
  //   https://www.w3.org/TR/web-animations-1/#dom-animatable-getanimations
  const KeyframeEffectReadOnly* keyframe_effect =
      base::polymorphic_downcast<const KeyframeEffectReadOnly*>(effect.get());
  if (keyframe_effect) {
    keyframe_effect->target()->Register(this);
  }
  set_timeline(timeline);
}

void Animation::set_timeline(const scoped_refptr<AnimationTimeline>& timeline) {
  // Deregister from the timeline we were previously registered to.
  if (timeline_) {
    timeline_->Deregister(this);
  }

  // Register with the timeline so that this Animation can be referenced from
  // that timeline, as per the web animations specifications:
  //   https://www.w3.org/TR/web-animations-1/#dom-document-getanimations
  if (timeline) {
    timeline->Register(this);
  }

  timeline_ = timeline;
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#playing-an-animation-section
void Animation::Play() {
  // This is currently a simplified version of steps 8.2.
  if (timeline_) {
    DCHECK(timeline_->current_time());
    if (!data_.start_time()) {
      set_start_time(timeline_->current_time());
    }
  }

  UpdatePendingTasks();
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#cancel-an-animation
void Animation::Cancel() {
  // 5.  Make animation's start time unresolved.
  data_.set_start_time(base::nullopt);

  UpdatePendingTasks();
}

base::Optional<base::TimeDelta> Animation::current_time_as_time_delta() const {
  if (!timeline_) {
    return base::nullopt;
  }

  return data_.ComputeLocalTimeFromTimelineTime(
      timeline_->current_time_as_time_delta());
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-current-time-of-an-animation
base::Optional<double> Animation::current_time() const {
  base::Optional<base::TimeDelta> current_time = current_time_as_time_delta();
  return current_time ? base::Optional<double>(current_time->InMillisecondsF())
                      : base::nullopt;
}

namespace {
base::TimeDelta ScaleTime(const base::TimeDelta& time, double scale) {
  return base::TimeDelta::FromMillisecondsD(time.InMillisecondsF() * scale);
}
}  // namespace

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-current-time-of-an-animation
base::Optional<base::TimeDelta>
Animation::Data::ComputeLocalTimeFromTimelineTime(
    const base::Optional<base::TimeDelta>& timeline_time) const {
  // TODO: Take into account the hold time.
  if (!timeline_time || !start_time_) {
    return base::nullopt;
  }

  return ScaleTime(*timeline_time - *start_time_, playback_rate_);
}

base::Optional<base::TimeDelta>
Animation::Data::ComputeTimelineTimeFromLocalTime(
    const base::Optional<base::TimeDelta>& local_time) const {
  if (!start_time_ || !local_time) {
    return base::nullopt;
  }

  if (local_time == base::TimeDelta::Max()) {
    return base::TimeDelta::Max();
  }

  return ScaleTime(*local_time, 1.0 / playback_rate_) + *start_time_;
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#setting-the-current-time-of-an-animation
void Animation::set_current_time(const base::Optional<double>& current_time) {
  NOTIMPLEMENTED();
}

void Animation::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(effect_);
  tracer->Trace(timeline_);
}

Animation::~Animation() {
  const KeyframeEffectReadOnly* keyframe_effect =
      base::polymorphic_downcast<const KeyframeEffectReadOnly*>(effect_.get());
  if (timeline_) {
    timeline_->Deregister(this);
  }
  if (keyframe_effect) {
    keyframe_effect->target()->Deregister(this);
  }

  // Make a copy of the animation set so that our loop iteration is unaffected
  // by the animation removals as we make them.
  std::set<scoped_refptr<AnimationSet>> contained_in_animation_sets =
      contained_in_animation_sets_;
  for (const auto& animation_set : contained_in_animation_sets) {
    animation_set->RemoveAnimation(this);
  }
}

void Animation::UpdatePendingTasks() {
  if (!effect_) {
    return;
  }

  base::TimeDelta end_time_local =
      effect_->timing()->data().time_until_after_phase(base::TimeDelta());

  base::Optional<base::TimeDelta> end_time_timeline =
      data_.ComputeTimelineTimeFromLocalTime(end_time_local);

  // If the local time is unresolved, then we cannot know when we will enter
  // the "after phase".
  if (end_time_timeline &&
      *end_time_timeline >= *timeline_->current_time_as_time_delta() &&
      *end_time_timeline != base::TimeDelta::Max()) {
    // Setup the "upon entering the after phase" event to fire at the
    // specified timeline time.
    on_enter_after_phase_ = timeline_->QueueTask(
        *end_time_timeline,
        base::Bind(&Animation::OnEnterAfterPhase, base::Unretained(this)));
  } else {
    // We are already in the after phase, so clear this task.
    on_enter_after_phase_.reset();
  }
}

void Animation::OnEnterAfterPhase() {
  if (event_handlers_.empty()) return;

  // Since this method is called via callback, and since one of the
  // event handlers it calls may result in the destruction of the
  // animation, the following line ensures the animation stays constructed
  // until we are done firing the callbacks.
  scoped_refptr<Animation> this_animation(this);

  // We start by making a copy of the set of event handlers we will call, in
  // case this set is modified while we are calling them.
  event_handlers_being_called_ = event_handlers_;

  // When we enter the after phase, let all registered event handlers know.
  while (!event_handlers_being_called_.empty()) {
    EventHandler* event_handler = *event_handlers_being_called_.begin();
    event_handlers_being_called_.erase(event_handlers_being_called_.begin());

    if (!event_handler->on_enter_after_phase().is_null()) {
      event_handler->on_enter_after_phase().Run();
    }
  }
}

std::unique_ptr<Animation::EventHandler> Animation::AttachEventHandler(
    const base::Closure& on_enter_after_phase) {
  // Attaches an event handler to this animation and returns a handle to it.
  std::unique_ptr<Animation::EventHandler> event_handler(
      new Animation::EventHandler(this, on_enter_after_phase));

  event_handlers_.insert(event_handler.get());

  return event_handler;
}

void Animation::OnAddedToAnimationSet(const scoped_refptr<AnimationSet>& set) {
  contained_in_animation_sets_.insert(set);
}

void Animation::OnRemovedFromAnimationSet(
    const scoped_refptr<AnimationSet>& set) {
  contained_in_animation_sets_.erase(set);
}

void Animation::RemoveEventHandler(EventHandler* handler) {
  // Called when the EventHandler object is destructed, this "deregisters"
  // the event handler from the Animation's event handler set.
  std::set<EventHandler*>::iterator found = event_handlers_.find(handler);
  DCHECK(found != event_handlers_.end());

  event_handlers_.erase(found);

  // If the event handler is while we are dispatching a list of event handlers,
  // then remove it from that list as well so that we don't call it.
  event_handlers_being_called_.erase(handler);
}

}  // namespace web_animations
}  // namespace cobalt

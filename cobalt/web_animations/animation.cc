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

#include "base/logging.h"
#include "cobalt/web_animations/animation.h"
#include "cobalt/web_animations/keyframe_effect_read_only.h"

namespace cobalt {
namespace web_animations {

Animation::Animation(const scoped_refptr<AnimationEffectReadOnly>& effect,
                     const scoped_refptr<AnimationTimeline>& timeline)
    : effect_(effect) {
  const KeyframeEffectReadOnly* keyframe_effect =
      base::polymorphic_downcast<const KeyframeEffectReadOnly*>(effect.get());
  keyframe_effect->target()->Register(this);
  set_timeline(timeline);
}

void Animation::set_timeline(const scoped_refptr<AnimationTimeline>& timeline) {
  if (timeline_) {
    timeline_->Deregister(this);
  }
  timeline->Register(this);
  timeline_ = timeline;
}

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#playing-an-animation-section
void Animation::Play() {
  // This is currently a simplified version of steps 8.2.
  DCHECK(timeline_);
  DCHECK(timeline_->current_time());
  if (!data_.start_time()) {
    set_start_time(timeline_->current_time());
  }
}

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#cancel-an-animation
void Animation::Cancel() {
  // 5.  Make animation's start time unresolved.
  data_.set_start_time(base::nullopt);
}

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-current-time-of-an-animation
base::optional<double> Animation::current_time() const {
  if (!timeline_) {
    return base::nullopt;
  }

  base::optional<base::TimeDelta> current_time =
      data_.ComputeLocalTimeFromTimelineTime(timeline_->current_time());
  return current_time ? base::optional<double>(current_time->InMillisecondsF())
                      : base::nullopt;
}

namespace {
base::TimeDelta ScaleTime(const base::TimeDelta& time, double scale) {
  return base::TimeDelta::FromMillisecondsD(time.InMillisecondsF() * scale);
}
}  // namespace

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-current-time-of-an-animation
base::optional<base::TimeDelta>
Animation::Data::ComputeLocalTimeFromTimelineTime(
    const base::optional<double>& timeline_time_in_milliseconds) const {
  // TODO(***REMOVED***): Take into account the hold time.
  if (!timeline_time_in_milliseconds || !start_time_) {
    return base::nullopt;
  }

  return ScaleTime(
      base::TimeDelta::FromMillisecondsD(*timeline_time_in_milliseconds) -
          *start_time_,
      playback_rate_);
}

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#setting-the-current-time-of-an-animation
void Animation::set_current_time(const base::optional<double>& current_time) {
  UNREFERENCED_PARAMETER(current_time);
  NOTIMPLEMENTED();
}

Animation::~Animation() {
  const KeyframeEffectReadOnly* keyframe_effect =
      base::polymorphic_downcast<const KeyframeEffectReadOnly*>(effect_.get());
  if (timeline_) {
    timeline_->Deregister(this);
  }
  keyframe_effect->target()->Deregister(this);
}

}  // namespace web_animations
}  // namespace cobalt

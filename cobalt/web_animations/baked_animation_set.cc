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

#include "cobalt/web_animations/baked_animation_set.h"

#include <algorithm>

#include "cobalt/web_animations/animation_effect_read_only.h"

namespace cobalt {
namespace web_animations {

// Extract Data objects from the Animation script objects.
BakedAnimation::BakedAnimation(const Animation& animation)
    : animation_data_(animation.data()),
      effect_timing_data_(animation.effect()->timing()->data()),
      keyframe_data_(base::polymorphic_downcast<const KeyframeEffectReadOnly*>(
                         animation.effect().get())
                         ->data()) {}

void BakedAnimation::Apply(
    const base::TimeDelta& timeline_time,
    cssom::MutableCSSComputedStyleData* in_out_style) const {
  // Get the animation's local time from the Animation::Data object.
  base::Optional<base::TimeDelta> local_time =
      animation_data_.ComputeLocalTimeFromTimelineTime(timeline_time);

  // Obtain the iteration progress from the AnimationEffectTimingReadOnly::Data
  // object.
  AnimationEffectTimingReadOnly::Data::IterationProgress iteration_progress =
      effect_timing_data_.ComputeIterationProgressFromLocalTime(local_time);

  if (!iteration_progress.iteration_progress) {
    return;
  }

  // Use the iteration progress to animate the CSS style properties using
  // keyframe data stored in KeyframeEffectReadOnly::Data.
  keyframe_data_.ApplyAnimation(in_out_style,
                                *iteration_progress.iteration_progress,
                                *iteration_progress.current_iteration);
}

base::TimeDelta BakedAnimation::end_time() const {
  base::TimeDelta end_time_local =
      effect_timing_data_.time_until_after_phase(base::TimeDelta());

  return *animation_data_.ComputeTimelineTimeFromLocalTime(end_time_local);
}

BakedAnimationSet::BakedAnimationSet(const AnimationSet& animation_set) {
  for (AnimationSet::InternalSet::const_iterator iter =
           animation_set.animations().begin();
       iter != animation_set.animations().end(); ++iter) {
    animations_.emplace_back(new BakedAnimation(**iter));
  }
}

BakedAnimationSet::BakedAnimationSet(const BakedAnimationSet& rhs) {
  for (AnimationList::const_iterator iter = rhs.animations_.begin();
       iter != rhs.animations_.end(); ++iter) {
    animations_.emplace_back(new BakedAnimation(**iter));
  }
}

void BakedAnimationSet::Apply(
    const base::TimeDelta& timeline_time,
    cssom::MutableCSSComputedStyleData* in_out_style) const {
  // TODO: Follow the procedure for combining effects.
  //   https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#combining-effects
  for (AnimationList::const_iterator iter = animations_.begin();
       iter != animations_.end(); ++iter) {
    (*iter)->Apply(timeline_time, in_out_style);
  }
}

base::TimeDelta BakedAnimationSet::end_time() const {
  base::TimeDelta max_end_time = -base::TimeDelta::Max();
  for (AnimationList::const_iterator iter = animations_.begin();
       iter != animations_.end(); ++iter) {
    base::TimeDelta animation_end_time = (*iter)->end_time();
    max_end_time = std::max(animation_end_time, max_end_time);
  }
  return max_end_time;
}

}  // namespace web_animations
}  // namespace cobalt

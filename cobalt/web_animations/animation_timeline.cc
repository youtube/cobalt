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
#include "cobalt/web_animations/animation_set.h"
#include "cobalt/web_animations/animation_timeline.h"

namespace cobalt {
namespace web_animations {

AnimationTimeline::AnimationTimeline(
    const scoped_refptr<base::BasicClock>& clock)
    : clock_(clock), animations_(new AnimationSet()) {}

AnimationTimeline::~AnimationTimeline() { DCHECK(animations_->IsEmpty()); }

void AnimationTimeline::Register(Animation* animation) {
  animations_->AddAnimation(animation);
}

void AnimationTimeline::Deregister(Animation* animation) {
  animations_->RemoveAnimation(animation);
}

unsigned int AnimationTimeline::num_animations() const {
  return static_cast<unsigned int>(animations_->GetSize());
}

// Returns the current time for the document.  This is based off of the last
// sampled time.
// https://www.w3.org/TR/web-animations-1/#the-animationtimeline-interface
base::Optional<double> AnimationTimeline::current_time() const {
  if (sampled_clock_time_) {
    return sampled_clock_time_->InMillisecondsF();
  } else {
    return base::nullopt;
  }
}

void AnimationTimeline::Sample() {
  if (clock_) {
    sampled_clock_time_ = clock_->Now();
    event_queue_.UpdateTime(*sampled_clock_time_);
  } else {
    sampled_clock_time_ = base::nullopt;
  }
  UpdateNextEventTimer();
}

std::unique_ptr<TimedTaskQueue::Task> AnimationTimeline::QueueTask(
    base::TimeDelta fire_time, const base::Closure& closure) {
  std::unique_ptr<TimedTaskQueue::Task> task =
      event_queue_.QueueTask(fire_time, closure);

  // Update our next-event timer since we may have modified the time of the
  // next-to-fire event by inserting this new event.
  UpdateNextEventTimer();

  return task;
}

void AnimationTimeline::UpdateNextEventTimer() {
  if (event_queue_.empty() || !sampled_clock_time_ || !clock_) {
    next_event_timer_.Stop();
  } else {
    base::TimeDelta delay = event_queue_.next_fire_time() - clock_->Now();
    next_event_timer_.Start(
        FROM_HERE, delay < base::TimeDelta() ? base::TimeDelta() : delay,
        base::Bind(&AnimationTimeline::Sample, base::Unretained(this)));
  }
}

}  // namespace web_animations
}  // namespace cobalt

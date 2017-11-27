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

#ifndef COBALT_WEB_ANIMATIONS_ANIMATION_TIMELINE_H_
#define COBALT_WEB_ANIMATIONS_ANIMATION_TIMELINE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/base/clock.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web_animations/timed_task_queue.h"

namespace cobalt {
namespace web_animations {

class Animation;
class AnimationSet;

// Implements the AnimationTimeline IDL interface.
//   https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-animationtimeline-interface
class AnimationTimeline : public script::Wrappable {
 public:
  explicit AnimationTimeline(const scoped_refptr<base::Clock>& clock);

  // Returns the current sample time of the timeline, in milliseconds.  If the
  // returned optional is not engaged, this timeline is 'unresolved'.
  base::optional<double> current_time() const;

  // Custom, not in any spec.

  // Helper class to return the current time as a base::TimeDelta instead of a
  // double.
  const base::optional<base::TimeDelta>& current_time_as_time_delta() const {
    return sampled_clock_time_;
  }

  // The owner of this timeline should call Sample() each time a new sample
  // time is ready.
  void Sample();

  // Returns the number of animations registered to this timeline.  This is not
  // in the spec, but serves as a poor-man's replacement for getAnimations()
  // until we have support for that.
  unsigned int num_animations() const;

  DEFINE_WRAPPABLE_TYPE(AnimationTimeline);

 protected:
  ~AnimationTimeline() override;

 private:
  // Registers and deregisters an animation with this timeline so that we are
  // able to track all animations currently associated with this timeline.
  // This will be required in order to implement getAnimations().
  void Register(Animation* animation);
  void Deregister(Animation* animation);

  // Called by Animation objects to queue their events into this timeline's
  // TimedTaskQueue.
  scoped_ptr<TimedTaskQueue::Task> QueueTask(base::TimeDelta fire_time,
                                             const base::Closure& closure);

  void UpdateNextEventTimer();

  scoped_refptr<base::Clock> clock_;
  scoped_refptr<AnimationSet> animations_;
  TimedTaskQueue event_queue_;
  base::optional<base::TimeDelta> sampled_clock_time_;
  base::OneShotTimer<AnimationTimeline> next_event_timer_;

  // So that Animation objects can register themselves.
  friend class Animation;

  DISALLOW_COPY_AND_ASSIGN(AnimationTimeline);
};

}  // namespace web_animations
}  // namespace cobalt

#endif  // COBALT_WEB_ANIMATIONS_ANIMATION_TIMELINE_H_

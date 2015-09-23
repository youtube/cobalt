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

#ifndef WEB_ANIMATIONS_ANIMATION_H_
#define WEB_ANIMATIONS_ANIMATION_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web_animations/animation_effect_read_only.h"
#include "cobalt/web_animations/animation_timeline.h"

namespace cobalt {
namespace web_animations {

// Animations are represented in the Web Animations API by the Animation
// interface.
//   http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-animation-interface
//
// This class introduces levers to pause/play/reverse an animation.  The
// Animation object provides functionality for converting from timeline time
// to the animation's local time, given its current state (e.g. start time,
// is the animation paused, etc...).  Additionally, it also links to the
// underlying effect which contains the rest of the data needed to fully
// specify an effect animation.
class Animation : public script::Wrappable {
 public:
  // Most of the internal Animation information is stored here in
  // Animation::Data.  This separate Data class exists to make it possible to
  // copy Animation information out of the non-thread-safe ref-counted
  // script-wrappable Animation object.  The class BakedAnimation takes
  // advantage of the Data object by storing an independent immutable instance
  // of the animation data that does not reference script objects and can be
  // passed to other threads.
  class Data {
   public:
    Data() : playback_rate_(1.0) {}

    const base::optional<base::TimeDelta>& start_time() const {
      return start_time_;
    }
    void set_start_time(const base::optional<base::TimeDelta>& start_time) {
      start_time_ = start_time;
    }

    double playback_rate() const { return playback_rate_; }
    void set_playback_rate(double playback_rate) {
      playback_rate_ = playback_rate;
    }

    // Converts the animation's timeline's time into the animation's local
    // time, which takes into account this animation's start_time().
    base::optional<base::TimeDelta> ComputeLocalTimeFromTimelineTime(
        const base::optional<double>& timeline_time_in_milliseconds) const;

   private:
    base::optional<base::TimeDelta> start_time_;
    double playback_rate_;
  };

  Animation(const scoped_refptr<AnimationEffectReadOnly>& effect,
            const scoped_refptr<AnimationTimeline>& timeline);

  // Web API: Animation
  const scoped_refptr<AnimationEffectReadOnly>& effect() const {
    return effect_;
  }
  void set_effect(const scoped_refptr<AnimationEffectReadOnly>& effect) {
    effect_ = effect;
  }

  const scoped_refptr<AnimationTimeline>& timeline() const { return timeline_; }
  void set_timeline(const scoped_refptr<AnimationTimeline>& timeline);

  base::optional<double> start_time() const {
    return data_.start_time()
               ? base::optional<double>(data_.start_time()->InMillisecondsF())
               : base::nullopt;
  }
  void set_start_time(const base::optional<double>& start_time) {
    if (!start_time) {
      data_.set_start_time(base::nullopt);
    } else {
      data_.set_start_time(base::TimeDelta::FromMillisecondsD(*start_time));
    }
  }

  base::optional<double> current_time() const;
  void set_current_time(const base::optional<double>& current_time);

  double playback_rate() const { return data_.playback_rate(); }
  void set_playback_rate(double playback_rate) {
    data_.set_playback_rate(playback_rate);
  }

  void Cancel();
  void Finish() {}
  void Play();
  void Pause() {}
  void Reverse() {}

  // Custom, not in any spec.
  const Data& data() const { return data_; }

  DEFINE_WRAPPABLE_TYPE(Animation);

 private:
  ~Animation() OVERRIDE;

  scoped_refptr<AnimationEffectReadOnly> effect_;
  scoped_refptr<AnimationTimeline> timeline_;
  Data data_;

  DISALLOW_COPY_AND_ASSIGN(Animation);
};

}  // namespace web_animations
}  // namespace cobalt

#endif  // WEB_ANIMATIONS_ANIMATION_H_

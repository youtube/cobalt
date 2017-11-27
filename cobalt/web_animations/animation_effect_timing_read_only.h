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

#ifndef COBALT_WEB_ANIMATIONS_ANIMATION_EFFECT_TIMING_READ_ONLY_H_
#define COBALT_WEB_ANIMATIONS_ANIMATION_EFFECT_TIMING_READ_ONLY_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time.h"
#include "cobalt/cssom/timing_function.h"
#include "cobalt/script/union_type.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace web_animations {

// Implements the Web Animations API AnimationEffectTimingReadOnly interface.
//   https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-animationeffecttimingreadonly-interface
// Specifies animation timing details, such as delay, number of iterations,
// duration, etc...
class AnimationEffectTimingReadOnly : public script::Wrappable {
 public:
  enum FillMode { kNone, kForwards, kBackwards, kBoth };
  enum PlaybackDirection { kNormal, kReverse, kAlternate, kAlternateReverse };

  AnimationEffectTimingReadOnly(
      const base::TimeDelta& delay, const base::TimeDelta& end_delay,
      FillMode fill, double iteration_start, double iterations,
      const base::TimeDelta& duration, PlaybackDirection direction,
      const scoped_refptr<cssom::TimingFunction>& easing)
      : data_(delay, end_delay, fill, iteration_start, iterations, duration,
              direction, easing) {}

  double delay() const { return data_.delay().InMillisecondsF(); }

  double end_delay() const { return data_.end_delay().InMillisecondsF(); }

  FillMode fill() const { return data_.fill(); }

  double iteration_start() const { return data_.iteration_start(); }

  double iterations() const { return data_.iterations(); }

  double duration() const { return data_.duration().InMillisecondsF(); }

  PlaybackDirection direction() const { return data_.direction(); }

  std::string easing() const {
    NOTIMPLEMENTED();
    return std::string("linear");
  }

  // Custom, not in any spec.

  // Package all the core data members provided by this class so that they can
  // easily be copied out of AnimationEffectTimingReadOnly and used elsewhere.
  class Data {
   public:
    Data()
        : iteration_start_(0.0),
          iterations_(1.0),
          easing_(cssom::TimingFunction::GetLinear()) {}
    Data(const base::TimeDelta& delay, const base::TimeDelta& end_delay,
         AnimationEffectTimingReadOnly::FillMode fill, double iteration_start,
         double iterations, const base::TimeDelta& duration,
         PlaybackDirection direction,
         const scoped_refptr<cssom::TimingFunction>& easing)
        : delay_(delay),
          end_delay_(end_delay),
          fill_(fill),
          iteration_start_(iteration_start),
          iterations_(iterations),
          duration_(duration),
          direction_(direction),
          easing_(easing) {}

    base::TimeDelta delay() const { return delay_; }
    base::TimeDelta end_delay() const { return end_delay_; }
    AnimationEffectTimingReadOnly::FillMode fill() const { return fill_; }
    double iteration_start() const { return iteration_start_; }
    double iterations() const { return iterations_; }
    base::TimeDelta duration() const { return duration_; }
    PlaybackDirection direction() const { return direction_; }
    const scoped_refptr<cssom::TimingFunction> easing() const {
      return easing_;
    }

    // Given a local time (computed from an Animation object), it can be passed
    // to ComputeIterationProgressFromLocalTime() in order to produce an
    // iteration progress and current iteration, which can be subsequently
    // passed into KeyframeEffectReadOnly::Data::ComputeAnimatedPropertyValue()
    // to animate property values.
    struct IterationProgress {
      base::optional<double> iteration_progress;
      base::optional<double> current_iteration;
    };
    IterationProgress ComputeIterationProgressFromLocalTime(
        const base::optional<base::TimeDelta>& local_time) const;

    // These methods are all based off of functionality defined by the Web
    // Animations specification.  They are all called internally by
    // ComputeIterationProgressFromLocalTime(), and are public primarily so that
    // they can be tested.
    base::optional<base::TimeDelta> ComputeActiveTimeFromLocalTime(
        const base::optional<base::TimeDelta>& local_time) const;
    base::optional<base::TimeDelta> ComputeScaledActiveTimeFromActiveTime(
        const base::optional<base::TimeDelta>& active_time) const;
    base::optional<base::TimeDelta> ComputeIterationTimeFromScaledActiveTime(
        const base::optional<base::TimeDelta>& scaled_active_time) const;
    base::optional<base::TimeDelta> ComputeDirectedTimeFromIterationTime(
        const base::optional<base::TimeDelta>& iteration_time,
        const base::optional<double>& current_iteration) const;
    base::optional<base::TimeDelta> ComputeTransformedTimeFromDirectedTime(
        const base::optional<base::TimeDelta>& directed_time) const;
    base::optional<double> ComputeIterationProgressFromTransformedTime(
        const base::optional<base::TimeDelta>& transformed_time) const;

    base::optional<double> ComputeCurrentIteration(
        const base::optional<base::TimeDelta>& active_time,
        const base::optional<base::TimeDelta>& scaled_active_time,
        const base::optional<base::TimeDelta>& iteration_time) const;

    base::TimeDelta time_until_after_phase(base::TimeDelta local_time) const;

   private:
    enum Phase {
      kBeforePhase,
      kActivePhase,
      kAfterPhase,
      kNoPhase,
    };
    Phase GetPhase(const base::optional<base::TimeDelta>& local_time) const;
    base::TimeDelta active_duration() const;
    base::TimeDelta start_offset() const;

    // delay_ here is sometimes referred to in the specs as "start delay".
    base::TimeDelta delay_;
    base::TimeDelta end_delay_;
    AnimationEffectTimingReadOnly::FillMode fill_;
    double iteration_start_;
    // iterations_ here is sometimes referred to in the specs as
    // "iteration count".
    double iterations_;
    // Note that the specs refer to duration here as "iteration duration".
    base::TimeDelta duration_;
    PlaybackDirection direction_;
    scoped_refptr<cssom::TimingFunction> easing_;
  };

  const Data& data() const { return data_; }

  DEFINE_WRAPPABLE_TYPE(AnimationEffectTimingReadOnly);

 protected:
  // While this is a read-only class, the data is not const because the
  // "ReadOnly" applies to the interface only, and non-read-only subclasses
  // may exist which extend the interface and allow writing.
  Data data_;

 private:
  ~AnimationEffectTimingReadOnly() override {}

  DISALLOW_COPY_AND_ASSIGN(AnimationEffectTimingReadOnly);
};

}  // namespace web_animations
}  // namespace cobalt

#endif  // COBALT_WEB_ANIMATIONS_ANIMATION_EFFECT_TIMING_READ_ONLY_H_

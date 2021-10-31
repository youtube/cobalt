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

#include "cobalt/web_animations/animation_effect_timing_read_only.h"

#include <cmath>
#include <limits>

namespace cobalt {
namespace web_animations {

AnimationEffectTimingReadOnly::Data::IterationProgress
AnimationEffectTimingReadOnly::Data::ComputeIterationProgressFromLocalTime(
    const base::Optional<base::TimeDelta>& local_time) const {
  // Calculating the iteration progress from the local time is summarized nicely
  // here: https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#overview
  // Note that the flowchart from that links concludes with the production of
  // "transformed time", which must then have this algorithm:
  //   https://w3c.github.io/web-animations/#calculating-the-iteration-progress
  // applied to obtain the iteration progress.
  IterationProgress ret;

  base::Optional<base::TimeDelta> active_time =
      ComputeActiveTimeFromLocalTime(local_time);
  base::Optional<base::TimeDelta> scaled_active_time =
      ComputeScaledActiveTimeFromActiveTime(active_time);
  base::Optional<base::TimeDelta> iteration_time =
      ComputeIterationTimeFromScaledActiveTime(scaled_active_time);

  ret.current_iteration =
      ComputeCurrentIteration(active_time, scaled_active_time, iteration_time);

  ret.iteration_progress = ComputeIterationProgressFromTransformedTime(
      ComputeTransformedTimeFromDirectedTime(
          ComputeDirectedTimeFromIterationTime(iteration_time,
                                               ret.current_iteration)));

  return ret;
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#animation-effect-phases-and-states
AnimationEffectTimingReadOnly::Data::Phase
AnimationEffectTimingReadOnly::Data::GetPhase(
    const base::Optional<base::TimeDelta>& local_time) const {
  if (!local_time) {
    return kNoPhase;
  }

  if (*local_time < delay_) {
    return kBeforePhase;
  }

  if (iterations_ == std::numeric_limits<double>::infinity() ||
      *local_time < delay_ + active_duration()) {
    return kActivePhase;
  }

  return kAfterPhase;
}

base::TimeDelta AnimationEffectTimingReadOnly::Data::time_until_after_phase(
    base::TimeDelta local_time) const {
  if (iterations_ == std::numeric_limits<double>::infinity()) {
    return base::TimeDelta::Max();
  }
  return (delay_ + active_duration()) - local_time;
}

namespace {
base::TimeDelta ScaleTime(const base::TimeDelta& time, double scale) {
  return base::TimeDelta::FromMillisecondsD(time.InMillisecondsF() * scale);
}
}  // namespace

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-active-duration
base::TimeDelta AnimationEffectTimingReadOnly::Data::active_duration() const {
  return ScaleTime(duration_, iterations_);
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-scaled-active-time
base::TimeDelta AnimationEffectTimingReadOnly::Data::start_offset() const {
  return ScaleTime(duration_, iteration_start_);
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-active-time
base::Optional<base::TimeDelta>
AnimationEffectTimingReadOnly::Data::ComputeActiveTimeFromLocalTime(
    const base::Optional<base::TimeDelta>& local_time) const {
  Phase phase = GetPhase(local_time);

  switch (phase) {
    case kBeforePhase:
      if (fill_ == AnimationEffectTimingReadOnly::kBackwards ||
          fill_ == AnimationEffectTimingReadOnly::kBoth) {
        return base::TimeDelta();
      } else {
        return base::nullopt;
      }
    case kActivePhase:
      return *local_time - delay_;
    case kAfterPhase:
      if (fill_ == AnimationEffectTimingReadOnly::kForwards ||
          fill_ == AnimationEffectTimingReadOnly::kBoth) {
        return active_duration();
      } else {
        return base::nullopt;
      }
    case kNoPhase:
      return base::nullopt;
  }

  NOTREACHED();
  return base::nullopt;
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-scaled-active-time
base::Optional<base::TimeDelta>
AnimationEffectTimingReadOnly::Data::ComputeScaledActiveTimeFromActiveTime(
    const base::Optional<base::TimeDelta>& active_time) const {
  if (!active_time) return base::nullopt;

  return *active_time + start_offset();
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-iteration-time
base::Optional<base::TimeDelta>
AnimationEffectTimingReadOnly::Data::ComputeIterationTimeFromScaledActiveTime(
    const base::Optional<base::TimeDelta>& scaled_active_time) const {
  if (!scaled_active_time) return base::nullopt;

  if (duration_ == base::TimeDelta()) return base::TimeDelta();

  double iteration_count_plus_start = iterations_ + iteration_start_;
  double iteration_count_plus_start_fraction =
      iteration_count_plus_start - std::floor(iteration_count_plus_start);
  if (iteration_count_plus_start_fraction == 0 && iterations_ != 0 &&
      *scaled_active_time - start_offset() == active_duration()) {
    return duration_;
  }

  return base::TimeDelta::FromMicroseconds(
      scaled_active_time->InMicroseconds() % duration_.InMicroseconds());
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-current-iteration
base::Optional<double>
AnimationEffectTimingReadOnly::Data::ComputeCurrentIteration(
    const base::Optional<base::TimeDelta>& active_time,
    const base::Optional<base::TimeDelta>& scaled_active_time,
    const base::Optional<base::TimeDelta>& iteration_time) const {
  // 1.  If the active time is unresolved, return unresolved.
  if (!active_time) return base::nullopt;
  // 2.  If the active time is zero, return floor(iteration start).
  if (*active_time == base::TimeDelta()) {
    return std::floor(iteration_start_);
  }
  // 3.  If the iteration duration is zero,
  //       If the iteration count is infinity, return infinity.
  //       Otherwise, return ceil(iteration start + iteration count) - 1.
  if (duration_ == base::TimeDelta()) {
    if (iterations_ == std::numeric_limits<double>::infinity()) {
      return std::numeric_limits<double>::infinity();
    } else {
      return std::ceil(iteration_start_ + iterations_) - 1;
    }
  }
  // 4.  If the iteration time equals the iteration duration, return
  //     iteration start + iteration count - 1.
  if (*iteration_time == duration_) {
    return iteration_start_ + iterations_ - 1;
  }
  // 5.  Return floor(scaled active time / iteration duration).
  return std::floor(scaled_active_time->InMillisecondsF() /
                    duration_.InMillisecondsF());
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-directed-time
base::Optional<base::TimeDelta>
AnimationEffectTimingReadOnly::Data::ComputeDirectedTimeFromIterationTime(
    const base::Optional<base::TimeDelta>& iteration_time,
    const base::Optional<double>& current_iteration) const {
  // 1.  If the iteration time is unresolved, return unresolved.
  if (!iteration_time) return base::nullopt;
  DCHECK(current_iteration);

  enum SimpleDirection { kForwards, kReverse };
  SimpleDirection current_direction;

  // 2.  Calculate the current direction using the first matching condition from
  //     the following list:
  if (direction_ == AnimationEffectTimingReadOnly::kNormal) {
    // If playback direction is normal,
    current_direction = kForwards;
  } else if (direction_ == AnimationEffectTimingReadOnly::kReverse) {
    // If playback direction is reverse,
    current_direction = kReverse;
  } else {
    // Otherwise,
    // 2.1.  Let d be the current iteration.
    double d = *current_iteration;
    if (d == std::numeric_limits<double>::infinity()) {
      current_direction = kForwards;
    } else {
      // 2.2.  If playback direction is alternate-reverse increment d by 1.
      if (direction_ == AnimationEffectTimingReadOnly::kAlternateReverse) {
        d += 1;
      }
      // 2.4.  If d % 2 == 0, let the current direction be forwards, otherwise
      //       let the current direction be reverse.
      if (static_cast<int>(d) % 2 == 0) {
        current_direction = kForwards;
      } else {
        current_direction = kReverse;
      }
    }
  }

  // 3.  If the current direction is forwards then return the iteration time.
  //     Otherwise, return the iteration duration - iteration time.
  return current_direction == kForwards ? *iteration_time
                                        : duration_ - *iteration_time;
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-transformed-time
base::Optional<base::TimeDelta>
AnimationEffectTimingReadOnly::Data::ComputeTransformedTimeFromDirectedTime(
    const base::Optional<base::TimeDelta>& directed_time) const {
  if (!directed_time) return base::nullopt;
  if (duration_ == base::TimeDelta::Max()) {
    return directed_time;
  }
  double unscaled_progress = 0;
  if (duration_ != base::TimeDelta()) {
    unscaled_progress =
        directed_time->InMillisecondsF() / duration_.InMillisecondsF();
  }

  DCHECK(easing_);
  double scaled_progress = easing_ != cssom::TimingFunction::GetLinear()
                               ? static_cast<double>(easing_->Evaluate(
                                     static_cast<float>(unscaled_progress)))
                               : unscaled_progress;

  return ScaleTime(duration_, scaled_progress);
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-iteration-progress
base::Optional<double> AnimationEffectTimingReadOnly::Data::
    ComputeIterationProgressFromTransformedTime(
        const base::Optional<base::TimeDelta>& transformed_time) const {
  if (!transformed_time) return base::nullopt;
  if (duration_ == base::TimeDelta()) {
    // TODO: Support animations with iteration duration set to 0.
    NOTIMPLEMENTED();
    return 0.0;
  } else {
    return transformed_time->InMillisecondsF() / duration_.InMillisecondsF();
  }
}

}  // namespace web_animations
}  // namespace cobalt

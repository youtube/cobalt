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

#include "cobalt/web_animations/animation_effect_timing_read_only.h"

#include <cmath>
#include <limits>

namespace cobalt {
namespace web_animations {

AnimationEffectTimingReadOnly::Data::IterationProgress
AnimationEffectTimingReadOnly::Data::ComputeIterationProgressFromLocalTime(
    const base::optional<base::TimeDelta>& local_time) const {
  // Calculating the iteration progress from the local time is summarized nicely
  // here: http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#overview
  // Note that the flowchart from that links concludes with the production of
  // "transformed time", which must then have this algorithm:
  //   https://w3c.github.io/web-animations/#calculating-the-iteration-progress
  // applied to obtain the iteration progress.
  IterationProgress ret;

  base::optional<base::TimeDelta> active_time =
      ComputeActiveTimeFromLocalTime(local_time);
  base::optional<base::TimeDelta> scaled_active_time =
      ComputeScaledActiveTimeFromActiveTime(active_time);
  base::optional<base::TimeDelta> iteration_time =
      ComputeIterationTimeFromScaledActiveTime(scaled_active_time);

  ret.current_iteration =
      ComputeCurrentIteration(active_time, scaled_active_time, iteration_time);

  ret.iteration_progress = ComputeIterationProgressFromTransformedTime(
      ComputeTransformedTimeFromDirectedTime(
          ComputeDirectedTimeFromIterationTime(iteration_time,
                                               ret.current_iteration)));

  return ret;
}

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#animation-effect-phases-and-states
AnimationEffectTimingReadOnly::Data::Phase
AnimationEffectTimingReadOnly::Data::GetPhase(
    const base::optional<base::TimeDelta>& local_time) const {
  if (!local_time) {
    return kNoPhase;
  }

  if (*local_time < delay_) {
    return kBeforePhase;
  }

  if (*local_time < delay_ + duration_) {
    return kActivePhase;
  }

  return kAfterPhase;
}

namespace {
base::TimeDelta ScaleTime(const base::TimeDelta& time, double scale) {
  return base::TimeDelta::FromMillisecondsD(time.InMillisecondsF() * scale);
}
}  // namespace

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-active-duration
base::TimeDelta AnimationEffectTimingReadOnly::Data::active_duration() const {
  return ScaleTime(duration_, iterations_);
}

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-scaled-active-time
base::TimeDelta AnimationEffectTimingReadOnly::Data::start_offset() const {
  return ScaleTime(duration_, iteration_start_);
}

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-active-time
base::optional<base::TimeDelta>
AnimationEffectTimingReadOnly::Data::ComputeActiveTimeFromLocalTime(
    const base::optional<base::TimeDelta>& local_time) const {
  Phase phase = GetPhase(local_time);

  // TODO(***REMOVED***): Implement fill mode.  As-is, a fill mode of 'both' is
  //               assumed.
  switch (phase) {
    case kBeforePhase:
      return base::TimeDelta();
    case kActivePhase:
      return *local_time - delay_;
    case kAfterPhase:
      return active_duration();
    case kNoPhase:
      return base::nullopt;
  }

  NOTREACHED();
  return base::nullopt;
}

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-scaled-active-time
base::optional<base::TimeDelta>
AnimationEffectTimingReadOnly::Data::ComputeScaledActiveTimeFromActiveTime(
    const base::optional<base::TimeDelta>& active_time) const {
  if (!active_time) return base::nullopt;

  return *active_time + start_offset();
}

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-iteration-time
base::optional<base::TimeDelta>
AnimationEffectTimingReadOnly::Data::ComputeIterationTimeFromScaledActiveTime(
    const base::optional<base::TimeDelta>& scaled_active_time) const {
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

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-current-iteration
base::optional<double>
AnimationEffectTimingReadOnly::Data::ComputeCurrentIteration(
    const base::optional<base::TimeDelta>& active_time,
    const base::optional<base::TimeDelta>& scaled_active_time,
    const base::optional<base::TimeDelta>& iteration_time) const {
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

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-directed-time
base::optional<base::TimeDelta>
AnimationEffectTimingReadOnly::Data::ComputeDirectedTimeFromIterationTime(
    const base::optional<base::TimeDelta>& iteration_time,
    const base::optional<double>& current_iteration) const {
  // TODO(***REMOVED***): Support playback direction:
  //   http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#direction-control
  UNREFERENCED_PARAMETER(current_iteration);

  return iteration_time;
}

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-transformed-time
base::optional<base::TimeDelta>
AnimationEffectTimingReadOnly::Data::ComputeTransformedTimeFromDirectedTime(
    const base::optional<base::TimeDelta>& directed_time) const {
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
  double scaled_progress = static_cast<double>(
      easing_->Evaluate(static_cast<float>(unscaled_progress)));

  return ScaleTime(duration_, scaled_progress);
}

// http://www.w3.org/TR/2015/WD-web-animations-1-20150707/#calculating-the-iteration-progress
base::optional<double> AnimationEffectTimingReadOnly::Data::
    ComputeIterationProgressFromTransformedTime(
        const base::optional<base::TimeDelta>& transformed_time) const {
  if (!transformed_time) return base::nullopt;
  if (duration_ == base::TimeDelta()) {
    // TODO(***REMOVED***): Support animations with iteration duration set to 0.
    NOTIMPLEMENTED();
    return 0.0;
  } else {
    return transformed_time->InMillisecondsF() / duration_.InMillisecondsF();
  }
}

}  // namespace web_animations
}  // namespace cobalt

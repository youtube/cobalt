// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/smoothed_value.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cobalt {
namespace renderer {

SmoothedValue::SmoothedValue(base::TimeDelta time_to_converge,
                             base::Optional<double> max_slope_magnitude)
    : time_to_converge_(time_to_converge),
      previous_derivative_(0),
      max_slope_magnitude_(max_slope_magnitude) {
  DCHECK(base::TimeDelta() < time_to_converge_);
  DCHECK(!max_slope_magnitude_ || *max_slope_magnitude_ > 0);
}

void SmoothedValue::SetTarget(double target, const base::TimeTicks& time) {
  // Determine the current derivative and value.
  double current_derivative = GetCurrentDerivative(time);
  base::Optional<double> current_value;
  if (target_) {
    current_value = GetValueAtTime(time);
  }

  // Set the previous derivative and value to the current derivative and value.
  previous_derivative_ = current_derivative;
  previous_value_ = current_value;

  target_ = target;
  target_set_time_ = time;
}

void SmoothedValue::SnapToTarget() {
  previous_value_ = base::nullopt;
  previous_derivative_ = 0;
}

double SmoothedValue::GetValueAtTime(const base::TimeTicks& time) const {
  if (!previous_value_) {
    // If only one target has ever been set, simply return it.
    return *target_;
  }

  // Compute the current value based off of a cubic bezier curve.
  double t = SmoothedValue::t(time);
  double one_minus_t = 1 - t;
  double P0 = SmoothedValue::P0();
  double P1 = SmoothedValue::P1();
  double P2 = SmoothedValue::P2();
  double P3 = SmoothedValue::P3();

  return one_minus_t * one_minus_t * one_minus_t * P0 +
         3 * one_minus_t * one_minus_t * t * P1 + 3 * one_minus_t * t * t * P2 +
         t * t * t * P3;
}

double SmoothedValue::t(const base::TimeTicks& time) const {
  DCHECK(target_) << "SetTarget() must have been called previously.";

  base::TimeDelta time_diff = time - target_set_time_;
  double time_to_converge_in_seconds = time_to_converge_.InSecondsF();

  // Enforce any maximum slope constraints (which can result in overriding the
  // time to converge).
  if (max_slope_magnitude_) {
    double largest_slope = GetDerivativeWithLargestMagnitude();
    if (largest_slope == std::numeric_limits<double>::infinity() ||
        largest_slope == -std::numeric_limits<double>::infinity()) {
      // If we can have a slope of infinity, then just don't move.
      return 0;
    }

    // If we find that our smoothing curve's maximum slope would result in a
    // slope greater than the maximum slope constraint, stretch the time to
    // converge in order to meet the slope constraint.  This can result in
    // overriding the user-provided time to converge.
    double unconstrained_largest_slope =
        largest_slope / time_to_converge_in_seconds;
    if (unconstrained_largest_slope < -*max_slope_magnitude_) {
      time_to_converge_in_seconds = -largest_slope / *max_slope_magnitude_;
    } else if (unconstrained_largest_slope > *max_slope_magnitude_) {
      time_to_converge_in_seconds = largest_slope / *max_slope_magnitude_;
    }
  }

  double t = time_diff.InSecondsF() / time_to_converge_in_seconds;

  DCHECK_LE(0, t);

  return std::max(std::min(t, 1.0), 0.0);
}

double SmoothedValue::P1() const {
  // See comments in header for why P1() is calculated this way.
  return *previous_value_ + previous_derivative_ / 3.0f;
}

double SmoothedValue::P2() const {
  // See comments in header for why P2() is calculated this way.
  return P3();
}

namespace {
double EvaluateCubicBezierDerivative(double P0, double P1, double P2, double P3,
                                     double t) {
  double one_minus_t = 1 - t;
  return 3 * one_minus_t * one_minus_t * (P1 - P0) +
         6 * one_minus_t * t * (P2 - P1) + 3 * t * t * (P3 - P2);
}
}  // namespace

double SmoothedValue::GetCurrentDerivative(const base::TimeTicks& time) const {
  if (!previous_value_) {
    // If only one target has ever been set, return 0 as our derivative.
    return 0;
  }

  double t = SmoothedValue::t(time);
  double P0 = SmoothedValue::P0();
  double P1 = SmoothedValue::P1();
  double P2 = SmoothedValue::P2();
  double P3 = SmoothedValue::P3();

  return EvaluateCubicBezierDerivative(P0, P1, P2, P3, t);
}

double SmoothedValue::GetDerivativeWithLargestMagnitude() const {
  double P0 = SmoothedValue::P0();
  double P1 = SmoothedValue::P1();
  double P2 = SmoothedValue::P2();
  double P3 = SmoothedValue::P3();

  // Since our spline is a cubic function, it will have a single inflection
  // point where its derivative is 0 (or infinite if
  // numerator = denominator = 0).  This function finds that single inflection
  // point and stores the value in |t|.  We then evaluate the derivative at
  // that inflection point, and at the beginning and end of the [0, 1] segment.
  // We then compare the results and return the derivative with the largest
  // magnitude.

  // Compute the location of the inflection point by setting the second
  // derivative to zero and solving.
  double numerator = (P2 - 2 * P1 + P0);
  double denominator = (-P3 + 3 * P2 - 3 * P1 + P0);
  double t;
  if (numerator == 0) {
    t = 0;
  } else if (denominator == 0.0) {
    double numerator_sign = (numerator >= 0 ? 1.0 : -1.0);
    return numerator_sign * std::numeric_limits<double>::infinity();
  } else {
    t = numerator / denominator;
  }

  // Evaluate the value of the derivative at each critical point.
  double at_inflection_point = EvaluateCubicBezierDerivative(P0, P1, P2, P3, t);
  double at_start = EvaluateCubicBezierDerivative(P0, P1, P2, P3, 0);
  double at_end = EvaluateCubicBezierDerivative(P0, P1, P2, P3, 1);

  if (std::abs(at_inflection_point) > std::abs(at_start)) {
    if (std::abs(at_inflection_point) > std::abs(at_end)) {
      return at_inflection_point;
    } else {
      return at_end;
    }
  } else {
    if (std::abs(at_start) > std::abs(at_end)) {
      return at_start;
    } else {
      return at_end;
    }
  }
}

}  // namespace renderer
}  // namespace cobalt

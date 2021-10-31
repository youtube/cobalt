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

#ifndef COBALT_RENDERER_SMOOTHED_VALUE_H_
#define COBALT_RENDERER_SMOOTHED_VALUE_H_

#include "base/optional.h"
#include "base/time/time.h"

namespace cobalt {
namespace renderer {

// Smooths a value over time.  Currently implemented using bezier curve.
class SmoothedValue {
 public:
  // |time_to_converge| indicates how long it takes for the current value
  // to converge to a newly set target value.  A |max_slope_magnitude| can
  // be provided to dictate the maximum slope the value will move by as it
  // transitions from one value to another.  It must be greater than 0, if
  // provided, and it can result in convergence times larger than
  // |time_to_converge|.
  SmoothedValue(
      base::TimeDelta time_to_converge,
      base::Optional<double> max_slope_magnitude = base::Optional<double>());

  // Sets the target value that GetCurrentValue() will smoothly converge
  // towards.
  void SetTarget(double target, const base::TimeTicks& time);

  // Snaps GetCurrentValue() to the last set target value.
  void SnapToTarget();

  // Returns the current value, which is always converging slowly towards
  // the last set target value.
  double GetValueAtTime(const base::TimeTicks& time) const;

 private:
  // The following methods return the parameters for the cubic bezier equation.
  //   https://en.wikipedia.org/wiki/B%C3%A9zier_curve#Cubic_B.C3.A9zier_curves

  // Returns the value of t to be used in cubic bezier equations.
  double t(const base::TimeTicks& time) const;

  double P0() const { return *previous_value_; }

  // Returns the value of P1 to be used in cubic bezier equations.
  // Here, we calculate it from |previous_derivative_| and |previous_value_|
  // in such a way that it results in a curve that at t = 0 has a derivative
  // equal to |previous_derivative_|.
  double P1() const;

  // Returns the value of P2 to be used in cubic bezier equations.
  // For us, we set it in such a way that the derivative at t = 1 is 0.
  double P2() const;

  double P3() const { return *target_; }

  // Returns the current derivative of GetCurrentValue() over time.
  double GetCurrentDerivative(const base::TimeTicks& time) const;

  // Returns the derivative of the function that has the highest magnitude
  // between 0 and 1.
  double GetDerivativeWithLargestMagnitude() const;

  const base::TimeDelta time_to_converge_;

  // The current target value that we are converging towards.
  base::Optional<double> target_;

  // Tracks when |target_| was last set.
  base::TimeTicks target_set_time_;

  // The value returned by GetCurrentValue() at the time that the target was
  // last set.
  base::Optional<double> previous_value_;

  // The derivative of GetCurrentValue() when target was last set.
  double previous_derivative_;

  base::Optional<double> max_slope_magnitude_;
};

}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_SMOOTHED_VALUE_H_

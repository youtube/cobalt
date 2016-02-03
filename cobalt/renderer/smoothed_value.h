/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_SMOOTHED_VALUE_H_
#define COBALT_RENDERER_SMOOTHED_VALUE_H_

#include "base/optional.h"
#include "base/time.h"

namespace cobalt {
namespace renderer {

// Smooths a value over time.  Currently implemented using bezier curve.
class SmoothedValue {
 public:
  // |time_to_converge| indicates how long it takes for the current value
  // to converge to a newly set target value.
  explicit SmoothedValue(base::TimeDelta time_to_converge);

  // Sets the target value that GetCurrentValue() will smoothly converge
  // towards.
  void SetTarget(float target);

  // Snaps GetCurrentValue() to the last set target value.
  void SnapToTarget();

  // Returns the current value, which is always converging slowly towards
  // the last set target value.
  float GetCurrentValue() const;

 private:
  // The following methods return the parameters for the cubic bezier equation.
  //   https://en.wikipedia.org/wiki/B%C3%A9zier_curve#Cubic_B.C3.A9zier_curves

  // Returns the value of t to be used in cubic bezier equations.
  float t() const;

  float P0() const { return *previous_target_; }

  // Returns the value of P1 to be used in cubic bezier equations.
  // Here, we calculate it from |previous_derivative_| and |previous_target_|
  // in such a way that it results in a curve that at t = 0 has a derivative
  // equal to |previous_deriviative_|.
  float P1() const;

  // Returns the value of P2 to be used in cubic bezier equations.
  // For us, we set it in such a way that the derivative at t = 1 is 0.
  float P2() const;

  float P3() const { return *target_; }

  // Returns the current derivative of GetCurrentValue() over time.
  float GetCurrentDerivative() const;

  const base::TimeDelta time_to_converge_;

  // The current target value that we are converging towards.
  base::optional<float> target_;

  // Tracks when |target_| was last set.
  base::TimeTicks target_set_time_;

  // The previous value of |target_|.
  base::optional<float> previous_target_;

  // The derivative of GetCurrentValue() when target was last set.
  float previous_derivative_;
};

}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_SMOOTHED_VALUE_H_

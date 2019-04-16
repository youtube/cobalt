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

#ifndef COBALT_MATH_EXPONENTIAL_MOVING_AVERAGE_H_
#define COBALT_MATH_EXPONENTIAL_MOVING_AVERAGE_H_

#include "base/logging.h"
#include "base/optional.h"

namespace cobalt {
namespace math {

// Models an exponential moving average.  Every value added will result in the
// average being adjusted towards the new value, where the new value is given
// the weight |new_value_weight| (passed into the constructor) out of a maximum
// of 1.0.
template <typename T>
class ExponentialMovingAverage {
 public:
  explicit ExponentialMovingAverage(float new_value_weight)
      : new_value_weight_(new_value_weight) {
    DCHECK_GE(1, new_value_weight_);
  }

  const base::Optional<T>& average() const { return average_; }

  void AddValue(T value) {
    if (!average_) {
      average_ = value;
    } else {
      *average_ =
          *average_ * (1 - new_value_weight_) + value * new_value_weight_;
    }
  }

 private:
  const float new_value_weight_;
  base::Optional<T> average_;
};

typedef ExponentialMovingAverage<float> ExponentialMovingAverageF;

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_EXPONENTIAL_MOVING_AVERAGE_H_

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

#include "cobalt/cssom/css_transition.h"

#include "cobalt/cssom/interpolate_property_value.h"

namespace cobalt {
namespace cssom {

Transition::Transition(
    PropertyKey target_property,
    const scoped_refptr<PropertyValue>& start_value,
    const scoped_refptr<PropertyValue>& end_value,
    const base::TimeDelta& start_time, const base::TimeDelta& duration,
    const base::TimeDelta& delay,
    const scoped_refptr<TimingFunction>& timing_function,
    scoped_refptr<PropertyValue> reversing_adjusted_start_value,
    float reversing_shortening_factor)
    : target_property_(target_property),
      start_value_(start_value),
      end_value_(end_value),
      start_time_(start_time),
      duration_(duration < base::TimeDelta() ? base::TimeDelta() : duration),
      delay_(delay),
      timing_function_(timing_function),
      reversing_adjusted_start_value_(reversing_adjusted_start_value),
      reversing_shortening_factor_(reversing_shortening_factor) {}

float Transition::Progress(const base::TimeDelta& time) const {
  base::TimeDelta since_start = time - start_time_;
  return timing_function_->Evaluate(static_cast<float>(
      (since_start - delay_).InSecondsF() / duration_.InSecondsF()));
}

const base::TimeDelta Transition::EndTime() const {
  return start_time_ +
         (duration_ < base::TimeDelta() ? base::TimeDelta() : duration_) +
         delay_;
}

scoped_refptr<PropertyValue> Transition::Evaluate(
    const base::TimeDelta& time) const {
  return InterpolatePropertyValue(Progress(time), start_value_, end_value_);
}

}  // namespace cssom
}  // namespace cobalt

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

#ifndef COBALT_CSSOM_CSS_TRANSITION_H_
#define COBALT_CSSOM_CSS_TRANSITION_H_

#include <algorithm>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/timing_function.h"

namespace cobalt {
namespace cssom {

class PropertyValue;

// A Transition object represents a persistent transition from one CSS style
// value to another.  Most of the data members of this class are defined with
// names based off of the concepts named here:
//   https://www.w3.org/TR/2013/WD-css3-transitions-20131119/#starting
class Transition {
 public:
  // The enum IsCanceled is used as a flag to indicate if a removed transition
  // is canceled or removed after finishing.
  enum IsCanceled { kIsNotCanceled, kIsCanceled };

  Transition(PropertyKey target_property,
             const scoped_refptr<PropertyValue>& start_value,
             const scoped_refptr<PropertyValue>& end_value,
             const base::TimeDelta& start_time, const base::TimeDelta& duration,
             const base::TimeDelta& delay,
             const scoped_refptr<TimingFunction>& timing_function,
             scoped_refptr<PropertyValue> reversing_adjusted_start_value,
             float reversing_shortening_factor);

  PropertyKey target_property() const { return target_property_; }
  const base::TimeDelta& start_time() const { return start_time_; }
  const base::TimeDelta& duration() const { return duration_; }
  const base::TimeDelta& delay() const { return delay_; }
  const scoped_refptr<TimingFunction>& timing_function() const {
    return timing_function_;
  }
  const scoped_refptr<PropertyValue>& start_value() const {
    return start_value_;
  }
  const scoped_refptr<PropertyValue>& end_value() const { return end_value_; }

  // The reversing shortening factor and reversing adjusted start value are
  // both used for dealing with the special case where a property that is
  // already transitioning is modified again to now transition back to where
  // it was transitioning from.  In this case, it would look weird if the
  // reversing transition took the full duration despite the original transition
  // being cancelled part-way through and so not taking the full duration.
  // These members help us know how to modify the reversing transition to have
  // the same duration as the cancelled initial transition.  They are described
  // here:
  //   https://www.w3.org/TR/2013/WD-css3-transitions-20131119/#reversing
  const scoped_refptr<PropertyValue>& reversing_adjusted_start_value() const {
    return reversing_adjusted_start_value_;
  }
  float reversing_shortening_factor() const {
    return reversing_shortening_factor_;
  }

  // Returns the animation progress at a specified time.  This will take into
  // account animation delay, duration, and timing function.
  float Progress(const base::TimeDelta& time) const;
  const base::TimeDelta EndTime() const;

  // Produces an animated property value for the specified time.
  scoped_refptr<PropertyValue> Evaluate(const base::TimeDelta& time) const;

 private:
  PropertyKey target_property_;
  scoped_refptr<PropertyValue> start_value_;
  scoped_refptr<PropertyValue> end_value_;
  base::TimeDelta start_time_;
  base::TimeDelta duration_;
  base::TimeDelta delay_;
  scoped_refptr<TimingFunction> timing_function_;

  // See the getter methods for these members for documentation.
  scoped_refptr<PropertyValue> reversing_adjusted_start_value_;
  float reversing_shortening_factor_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_TRANSITION_H_

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

#ifndef COBALT_CSSOM_ANIMATION_H_
#define COBALT_CSSOM_ANIMATION_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cobalt/cssom/css_keyframes_rule.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/timing_function.h"

namespace cobalt {
namespace cssom {

// The Animation class defines a descriptive object that defines a CSS
// Animation and contains a reference to the CSSKeyframesRule that the
// animation references.
class Animation {
 public:
  // Determines how the animation should interact with the style before the
  // animation starts and after it ends.
  enum FillMode {
    kNone,
    kForwards,
    kBackwards,
    kBoth,
  };

  // Determines what direction the animation should play in.
  enum PlaybackDirection {
    kNormal,
    kReverse,
    kAlternate,
    kAlternateReverse,
  };

  Animation(const std::string& name,
            const scoped_refptr<CSSKeyframesRule>& keyframes,
            base::TimeDelta start_time, base::TimeDelta delay,
            base::TimeDelta duration, FillMode fill_mode, float iteration_count,
            PlaybackDirection direction,
            const scoped_refptr<TimingFunction>& timing_function)
      : name_(name),
        keyframes_(keyframes),
        start_time_(start_time),
        delay_(delay),
        duration_(duration),
        fill_mode_(fill_mode),
        iteration_count_(iteration_count),
        direction_(direction),
        timing_function_(timing_function) {}

  // Returns the 'animation-name' property value.
  const std::string& name() const { return name_; }

  // Returns the looked up CSSKeyframesRule referenced by 'animation-name'.
  const scoped_refptr<CSSKeyframesRule>& keyframes() const {
    return keyframes_;
  }

  // Returns the animation's start time, provided to the constructor.
  base::TimeDelta start_time() const { return start_time_; }

  // Returns the 'animation-duration' property value.
  base::TimeDelta duration() const { return duration_; }

  // Returns the 'animation-delay' property value.
  base::TimeDelta delay() const { return delay_; }

  // Returns the 'animation-fill-mode' property value.
  FillMode fill_mode() const { return fill_mode_; }

  // Returns the 'animation-iteration-count' property value.
  float iteration_count() const { return iteration_count_; }

  // Returns the 'animation-direction' property value.
  PlaybackDirection direction() const { return direction_; }

  // Returns the 'animation-timing-function' property value.
  const scoped_refptr<TimingFunction>& timing_function() const {
    return timing_function_;
  }

 private:
  std::string name_;
  scoped_refptr<CSSKeyframesRule> keyframes_;
  base::TimeDelta start_time_;
  base::TimeDelta delay_;
  base::TimeDelta duration_;
  FillMode fill_mode_;
  float iteration_count_;
  PlaybackDirection direction_;
  scoped_refptr<TimingFunction> timing_function_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_ANIMATION_H_

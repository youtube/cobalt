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

#ifndef COBALT_CSSOM_TIMING_FUNCTION_H_
#define COBALT_CSSOM_TIMING_FUNCTION_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stringprintf.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/keyword_names.h"
#include "cobalt/math/cubic_bezier.h"

namespace cobalt {
namespace cssom {

// The TimingFunction class represents a timing function CSS property value
// that transforms time (from 0 to 1) into animation progress (where 0
// means the beginning and 1 means the end).  Since CSS provides keywords
// for common timing function parameters, these specific timing functions also
// have getter methods provided here.
// More information on CSS timing functions can be found in the specification:
//   https://www.w3.org/TR/css3-transitions/#transition-timing-function-property
class TimingFunction : public base::RefCountedThreadSafe<TimingFunction>,
                       public base::PolymorphicEquatable {
 public:
  // Transforms a time value (x) into an animation progress value (the return
  // value).
  virtual float Evaluate(float x) const = 0;

  // Functions to retreive specific timing functions that are named by CSS
  // keywords.
  static const scoped_refptr<TimingFunction>& GetEase();
  static const scoped_refptr<TimingFunction>& GetEaseIn();
  static const scoped_refptr<TimingFunction>& GetEaseInOut();
  static const scoped_refptr<TimingFunction>& GetEaseOut();
  static const scoped_refptr<TimingFunction>& GetLinear();
  static const scoped_refptr<TimingFunction>& GetStepEnd();
  static const scoped_refptr<TimingFunction>& GetStepStart();

  virtual std::string ToString() = 0;

 protected:
  virtual ~TimingFunction() {}

  friend class base::RefCountedThreadSafe<TimingFunction>;
};

// A cubic bezier timing function is parameterized by two 2D control points, one
// at position (0, 0) and the other at position (1, 1).
// Note that this is not a 1D bezier curve, rather it is the function defined by
// drawing a 2D bezier curve to act as the graph of a 1D function.  The only
// reason this function maps each x value to a *unique* y value is because of
// restrictions on the input control points.  The defined function is NOT a
// polynomial (it contains square-root and cube-root terms).
class CubicBezierTimingFunction : public TimingFunction {
 public:
  CubicBezierTimingFunction(float p1_x, float p1_y, float p2_x, float p2_y)
      : cubic_bezier_(p1_x, p1_y, p2_x, p2_y) {}

  float Evaluate(float x) const override;

  std::string ToString() override;

  bool operator==(const CubicBezierTimingFunction& other) const {
    return cubic_bezier_ == other.cubic_bezier_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(CubicBezierTimingFunction);

 protected:
  ~CubicBezierTimingFunction() override {}

 private:
  math::CubicBezier cubic_bezier_;
};

// A stepping timing function is a piecewise constant function that progress
// towards the result in discrete steps.
class SteppingTimingFunction : public TimingFunction {
 public:
  // When should the step in progress occur, at the starts of the step intervals
  // or the ends of the step intervals.
  enum ValueChangeLocation {
    kStart,
    kEnd,
  };

  SteppingTimingFunction(int number_of_steps,
                         ValueChangeLocation value_change_location)
      : number_of_steps_(number_of_steps),
        value_change_location_(value_change_location) {
    DCHECK_LT(0, number_of_steps_);
  }

  float Evaluate(float x) const override;

  std::string ToString() override;

  bool operator==(const SteppingTimingFunction& other) const {
    return number_of_steps_ == other.number_of_steps_ &&
           value_change_location_ == other.value_change_location_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(SteppingTimingFunction);

 protected:
  ~SteppingTimingFunction() override{};

 private:
  int number_of_steps_;
  ValueChangeLocation value_change_location_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_TIMING_FUNCTION_H_

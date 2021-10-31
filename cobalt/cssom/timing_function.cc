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

#include "cobalt/cssom/timing_function.h"

#include <algorithm>

#include "base/lazy_instance.h"
#include "base/logging.h"

namespace cobalt {
namespace cssom {

namespace {

struct NonTrivialStaticFields {
  // Values for keyword timing values are provided by the specification:
  //  https://www.w3.org/TR/css3-transitions/#transition-timing-function-property
  NonTrivialStaticFields()
      : ease(new CubicBezierTimingFunction(0.25f, 0.1f, 0.25f, 1.0f)),
        ease_in(new CubicBezierTimingFunction(0.42f, 0.0f, 1.0f, 1.0f)),
        ease_in_out(new CubicBezierTimingFunction(0.42f, 0.0f, 0.58f, 1.0f)),
        ease_out(new CubicBezierTimingFunction(0.0f, 0.0f, 0.58f, 1.0f)),
        linear(new CubicBezierTimingFunction(0.0f, 0.0f, 1.0f, 1.0f)),
        step_end(new SteppingTimingFunction(1, SteppingTimingFunction::kEnd)),
        step_start(
            new SteppingTimingFunction(1, SteppingTimingFunction::kStart)) {}

  const scoped_refptr<TimingFunction> ease;
  const scoped_refptr<TimingFunction> ease_in;
  const scoped_refptr<TimingFunction> ease_in_out;
  const scoped_refptr<TimingFunction> ease_out;
  const scoped_refptr<TimingFunction> linear;
  const scoped_refptr<TimingFunction> step_end;
  const scoped_refptr<TimingFunction> step_start;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

base::LazyInstance<NonTrivialStaticFields>::DestructorAtExit
    non_trivial_static_fields = LAZY_INSTANCE_INITIALIZER;

}  // namespace

const scoped_refptr<TimingFunction>& TimingFunction::GetEase() {
  return non_trivial_static_fields.Get().ease;
}

const scoped_refptr<TimingFunction>& TimingFunction::GetEaseIn() {
  return non_trivial_static_fields.Get().ease_in;
}

const scoped_refptr<TimingFunction>& TimingFunction::GetEaseInOut() {
  return non_trivial_static_fields.Get().ease_in_out;
}

const scoped_refptr<TimingFunction>& TimingFunction::GetEaseOut() {
  return non_trivial_static_fields.Get().ease_out;
}

const scoped_refptr<TimingFunction>& TimingFunction::GetLinear() {
  return non_trivial_static_fields.Get().linear;
}

const scoped_refptr<TimingFunction>& TimingFunction::GetStepEnd() {
  return non_trivial_static_fields.Get().step_end;
}

const scoped_refptr<TimingFunction>& TimingFunction::GetStepStart() {
  return non_trivial_static_fields.Get().step_start;
}

float CubicBezierTimingFunction::Evaluate(float x) const {
  return static_cast<float>(cubic_bezier_.Solve(x));
}

float SteppingTimingFunction::Evaluate(float x) const {
  // number_of_steps_ must be greater than 0.  This is DCHECK'd in the
  // constructor.
  int offset_amount = value_change_location_ == kStart ? 1 : 0;
  return std::min(static_cast<int>(x * number_of_steps_ + offset_amount) /
                      static_cast<float>(number_of_steps_),
                  1.0f);
}

std::string CubicBezierTimingFunction::ToString() {
  return base::StringPrintf("cubic-bezier(%.7g,%.7g,%.7g,%.7g)",
                            cubic_bezier_.x1(), cubic_bezier_.y1(),
                            cubic_bezier_.x2(), cubic_bezier_.y2());
}

std::string SteppingTimingFunction::ToString() {
  std::string result = base::StringPrintf("steps(%d, ", number_of_steps_);
  switch (value_change_location_) {
    case kStart: {
      result.append(kStartKeywordName);
      break;
    }
    case kEnd: {
      result.append(kEndKeywordName);
      break;
    }
  }
  result.push_back(')');
  return result;
}

}  // namespace cssom
}  // namespace cobalt

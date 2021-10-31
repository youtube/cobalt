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

#include "cobalt/css_parser/animation_shorthand_property_parse_structures.h"

#include "cobalt/cssom/css_declared_style_data.h"

namespace cobalt {
namespace css_parser {

namespace {

// While the initial values for animation_duration, animation_timing_function
// and animation_delay are well defined, they are lists of single objects,
// and right here what we really need are those single objects.  These wrapper
// functions extract the single element out of the initial list values for
// each different optional animation property.
base::TimeDelta GetInitialDelay() {
  return base::polymorphic_downcast<const cssom::TimeListValue*>(
             cssom::GetPropertyInitialValue(cssom::kAnimationDelayProperty)
                 .get())
      ->value()[0];
}

scoped_refptr<cssom::PropertyValue> GetInitialDirection() {
  return base::polymorphic_downcast<const cssom::PropertyListValue*>(
             cssom::GetPropertyInitialValue(cssom::kAnimationDirectionProperty)
                 .get())
      ->value()[0];
}

base::TimeDelta GetInitialDuration() {
  return base::polymorphic_downcast<const cssom::TimeListValue*>(
             cssom::GetPropertyInitialValue(cssom::kAnimationDurationProperty)
                 .get())
      ->value()[0];
}

scoped_refptr<cssom::PropertyValue> GetInitialFillMode() {
  return base::polymorphic_downcast<const cssom::PropertyListValue*>(
             cssom::GetPropertyInitialValue(cssom::kAnimationFillModeProperty)
                 .get())
      ->value()[0];
}

scoped_refptr<cssom::PropertyValue> GetInitialIterationCount() {
  return base::polymorphic_downcast<const cssom::PropertyListValue*>(
             cssom::GetPropertyInitialValue(
                 cssom::kAnimationIterationCountProperty)
                 .get())
      ->value()[0];
}

scoped_refptr<cssom::PropertyValue> GetInitialName() {
  return base::polymorphic_downcast<const cssom::PropertyListValue*>(
             cssom::GetPropertyInitialValue(cssom::kAnimationFillModeProperty)
                 .get())
      ->value()[0];
}

scoped_refptr<cssom::TimingFunction> GetInitialTimingFunction() {
  return base::polymorphic_downcast<const cssom::TimingFunctionListValue*>(
             cssom::GetPropertyInitialValue(
                 cssom::kAnimationTimingFunctionProperty)
                 .get())
      ->value()[0];
}

}  // namespace

void SingleAnimationShorthand::ReplaceNullWithInitialValues() {
  if (!delay) {
    delay = GetInitialDelay();
  }

  if (!direction) {
    direction = GetInitialDirection();
  }

  if (!duration) {
    duration = GetInitialDuration();
  }

  if (!fill_mode) {
    fill_mode = GetInitialFillMode();
  }

  if (!iteration_count) {
    iteration_count = GetInitialIterationCount();
  }

  if (!name) {
    name = GetInitialName();
  }

  if (!timing_function) {
    timing_function = GetInitialTimingFunction();
  }
}

}  // namespace css_parser
}  // namespace cobalt

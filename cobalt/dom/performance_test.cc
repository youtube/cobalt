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

#include "cobalt/dom/performance.h"
#include "cobalt/dom/performance_high_resolution_time.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

TEST(PerformanceTest, Now) {
  scoped_refptr<base::SystemMonotonicClock> clock(
      new base::SystemMonotonicClock());

  testing::StubEnvironmentSettings environment_settings;
  scoped_refptr<Performance> performance(
      new Performance(&environment_settings, clock));

  // Test that now returns a result that is within a correct range for the
  // current time.
  DOMHighResTimeStamp lower_limit =
      performance->MonotonicTimeToDOMHighResTimeStamp(base::TimeTicks::Now());

  DOMHighResTimeStamp current_time_in_milliseconds = performance->Now();

  DOMHighResTimeStamp upper_limit =
      performance->MonotonicTimeToDOMHighResTimeStamp(base::TimeTicks::Now());

  DCHECK_GE(current_time_in_milliseconds, lower_limit);
  DCHECK_LE(current_time_in_milliseconds, upper_limit);
}

TEST(PerformanceTest, MonotonicTimeToDOMHighResTimeStamp) {
  scoped_refptr<base::SystemMonotonicClock> clock(
      new base::SystemMonotonicClock());

  testing::StubEnvironmentSettings environment_settings;
  scoped_refptr<Performance> performance(
      new Performance(&environment_settings, clock));

  base::TimeTicks current_time_ticks = base::TimeTicks::Now();
  DOMHighResTimeStamp current_time = ClampTimeStampMinimumResolution(
      current_time_ticks,
      Performance::kPerformanceTimerMinResolutionInMicroseconds);
  DOMHighResTimeStamp current_time_respect_to_time_origin =
      performance->MonotonicTimeToDOMHighResTimeStamp(current_time_ticks);
  DOMHighResTimeStamp time_origin = ClampTimeStampMinimumResolution(
      performance->GetTimeOrigin(),
      Performance::kPerformanceTimerMinResolutionInMicroseconds);

  DCHECK_EQ(current_time_respect_to_time_origin, current_time - time_origin);
}

TEST(PerformanceTest, NavigationStart) {
  // navigationStart is supposed to return the time, in milliseconds, since
  // January 1st, 1970 (UTC):
  //   https://w3c.github.io/navigation-timing/#the-performancetiming-interface
  // We assume (and test) here though that navigationStart time will be set
  // relative to the time that the NavigationTiming object was created, since
  // the object will be created at the beginning of a new navigation.
  scoped_refptr<base::SystemMonotonicClock> clock(
      new base::SystemMonotonicClock());
  base::TimeTicks lower_limit = base::TimeTicks::Now();

  scoped_refptr<PerformanceTiming> performance_timing(
      new PerformanceTiming(clock, base::TimeTicks::Now()));

  base::TimeTicks upper_limit = base::TimeTicks::Now();

  DCHECK_GE(performance_timing->navigation_start(),
            static_cast<uint64>((lower_limit.ToInternalValue())));
  DCHECK_LE(performance_timing->navigation_start(),
            static_cast<uint64>((upper_limit.ToInternalValue())));
}

}  // namespace dom
}  // namespace cobalt

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
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

TEST(PerformanceTest, Now) {
  scoped_refptr<base::SystemMonotonicClock> clock(
      new base::SystemMonotonicClock());

  scoped_refptr<Performance> performance(new Performance(clock));

  // Test that now returns a result that is within a correct range for the
  // current time.
  base::TimeDelta lower_limit = clock->Now();

  double current_time_in_milliseconds = performance->Now();

  base::TimeDelta upper_limit = clock->Now();

  scoped_refptr<base::OffsetClock> navigation_start_clock =
      performance->timing()->GetNavigationStartClock();

  DCHECK_GE(current_time_in_milliseconds,
            (lower_limit - navigation_start_clock->origin()).InMillisecondsF());
  DCHECK_LE(current_time_in_milliseconds,
            (upper_limit - navigation_start_clock->origin()).InMillisecondsF());
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
  base::Time lower_limit = base::Time::Now();

  scoped_refptr<PerformanceTiming> performance_timing(
      new PerformanceTiming(clock));

  base::Time upper_limit = base::Time::Now();

  DCHECK_GE(performance_timing->navigation_start(),
            static_cast<uint64>(
                (lower_limit - base::Time::UnixEpoch()).InMilliseconds()));
  DCHECK_LE(performance_timing->navigation_start(),
            static_cast<uint64>(
                (upper_limit - base::Time::UnixEpoch()).InMilliseconds()));
}

}  // namespace dom
}  // namespace cobalt

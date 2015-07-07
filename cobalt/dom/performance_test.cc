/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/performance.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

TEST(PerformanceTest, Now) {
  scoped_refptr<Performance> performance(new Performance());

  // Test that now returns a result that is within a correct range for the
  // current time.
  base::Time lower_limit = base::Time::Now();

  double current_time_in_milliseconds = performance->Now();

  base::Time upper_limit = base::Time::Now();

  DCHECK_GE(current_time_in_milliseconds,
            (lower_limit - performance->timing()->navigation_start_time())
                .InMillisecondsF());
  DCHECK_LE(current_time_in_milliseconds,
            (upper_limit - performance->timing()->navigation_start_time())
                .InMillisecondsF());
}

TEST(PerformanceTest, NavigationStart) {
  // navigationStart is supposed to return the time, in milliseconds, since
  // the page was loaded, as described here:
  //   https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/NavigationTiming/Overview.html#sec-navigation-timing-interface
  // We assume (and test) here though that navigationStart time will be set
  // to the time that the NavigationTiming object was created, since it is
  // not yet clear how things will be setup to support navigating to new
  // web pages, and it is immediately useful to have the current functionality
  // in place.
  base::Time lower_limit = base::Time::Now();

  scoped_refptr<PerformanceTiming> performance_timing(new PerformanceTiming());

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

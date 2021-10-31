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

#ifndef COBALT_DOM_PERFORMANCE_TIMING_H_
#define COBALT_DOM_PERFORMANCE_TIMING_H_

#include "base/time/time.h"
#include "cobalt/base/clock.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// Implements the PerformanceTiming IDL interface, as described here:
//   https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/NavigationTiming/Overview.html#sec-navigation-timing-interface
class PerformanceTiming : public script::Wrappable {
  // If any new public fields are added here, handling logic must be added to
  // Performance::Mark and Performance::Measure.
 public:
  explicit PerformanceTiming(const scoped_refptr<base::BasicClock>& clock,
                             base::TimeTicks time_origin);

  // This attribute must return the time immediately after the user agent
  // finishes prompting to unload the previous document. If there is no previous
  // document, this attribute must return the time the current document is
  // created.
  uint64 navigation_start() const;

  // Custom, not in any spec.

  // Returns a clock that is relative to the navigation start time, and based
  // off of the clock passed into Performance (the one that navigation start
  // time was derived from).
  scoped_refptr<base::OffsetClock> GetNavigationStartClock();

  DEFINE_WRAPPABLE_TYPE(PerformanceTiming);

 private:
  ~PerformanceTiming();

  // The navigation start time relative to January 1, 1970.
  base::TimeTicks navigation_start_;
  scoped_refptr<base::OffsetClock> navigation_start_clock_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceTiming);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_TIMING_H_

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

#include "cobalt/dom/performance_timing.h"

namespace cobalt {
namespace dom {

PerformanceTiming::PerformanceTiming(const scoped_refptr<base::Clock>& clock)
    : navigation_start_clock_(new base::OffsetClock(clock, clock->Now())) {}

PerformanceTiming::~PerformanceTiming() {}

uint64 PerformanceTiming::navigation_start() const {
  return static_cast<uint64>(
      navigation_start_clock_->origin().InMilliseconds());
}

scoped_refptr<base::OffsetClock> PerformanceTiming::GetNavigationStartClock() {
  return navigation_start_clock_;
}

}  // namespace dom
}  // namespace cobalt

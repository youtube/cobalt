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

#ifndef DOM_PERFORMANCE_TIMING_H_
#define DOM_PERFORMANCE_TIMING_H_

#include "cobalt/script/wrappable.h"

#include "base/time.h"

namespace cobalt {
namespace dom {

// Implements the PerformanceTiming IDL interface, as described here:
//   https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/NavigationTiming/Overview.html#sec-navigation-timing-interface
class PerformanceTiming : public script::Wrappable {
 public:
  PerformanceTiming();

  // This attribute must return the time immediately after the user agent
  // finishes prompting to unload the previous document. If there is no previous
  // document, this attribute must return the time the current document is
  // created.
  uint64 navigation_start() const;

  // Custom, not in any spec.

  // The *_time() methods return the underlying base::Time structure from
  // which the integer millisecond times are returned.  Thus, more accurate
  // timing results can be obtained from these numbers.
  const base::Time& navigation_start_time() const;

  DEFINE_WRAPPABLE_TYPE(PerformanceTiming);

 private:
  ~PerformanceTiming();

  base::Time navigation_start_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceTiming);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_PERFORMANCE_TIMING_H_

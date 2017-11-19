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

#ifndef COBALT_DOM_PERFORMANCE_H_
#define COBALT_DOM_PERFORMANCE_H_

#include "cobalt/base/clock.h"
#include "cobalt/dom/performance_timing.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class MemoryInfo;

// Implements the Performance IDL interface, an instance of which is created
// and owned by the Window object.
//   https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/NavigationTiming/Overview.html#sec-window.performance-attribute
class Performance : public script::Wrappable {
 public:
  explicit Performance(const scoped_refptr<base::Clock>& clock);

  // Web API: Performance
  scoped_refptr<PerformanceTiming> timing() const;
  scoped_refptr<MemoryInfo> memory() const;

  // Custom, not in any spec.
  // Returns the time since timing()->navigation_start(), in milliseconds.
  double Now() const;

  DEFINE_WRAPPABLE_TYPE(Performance);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~Performance() override;

  scoped_refptr<PerformanceTiming> timing_;
  scoped_refptr<MemoryInfo> memory_;

  DISALLOW_COPY_AND_ASSIGN(Performance);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_H_

// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/performance_measure.h"

#include "cobalt/dom/performance.h"
#include "cobalt/dom/performance_entry.h"

namespace cobalt {
namespace dom {

PerformanceMeasure::PerformanceMeasure(const std::string& name,
                                       DOMHighResTimeStamp start_time,
                                       DOMHighResTimeStamp end_time)
    : PerformanceEntry(name, start_time, end_time) {}

}  // namespace dom
}  // namespace cobalt

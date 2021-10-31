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

#ifndef COBALT_DOM_PERFORMANCE_MARK_H_
#define COBALT_DOM_PERFORMANCE_MARK_H_

#include <string>

#include "cobalt/dom/performance_entry.h"
#include "cobalt/dom/performance_high_resolution_time.h"
#include "cobalt/script/wrappable.h"
#include "net/base/load_timing_info.h"

namespace cobalt {
namespace dom {
class Performance;

// Implements the PerformanceMart interface.
//   https://www.w3.org/TR/2019/REC-user-timing-2-20190226/#performancemark
class PerformanceMark : public PerformanceEntry {
 public:
  PerformanceMark(const std::string& name, DOMHighResTimeStamp start_time);

  std::string entry_type() const override { return "mark"; }
  PerformanceEntryType EntryTypeEnum() const override {
    return PerformanceEntry::kMark;
  }

  DEFINE_WRAPPABLE_TYPE(PerformanceMark);
};
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_MARK_H_

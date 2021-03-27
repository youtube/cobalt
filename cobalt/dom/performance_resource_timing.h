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

#ifndef COBALT_DOM_PERFORMANCE_RESOURCE_TIMING_H_
#define COBALT_DOM_PERFORMANCE_RESOURCE_TIMING_H_

#include <string>

#include "cobalt/dom/performance_entry.h"
#include "cobalt/dom/performance_high_resolution_time.h"

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// Implements the Performance Resource Timing IDL interface, as described here:
//   https://www.w3.org/TR/resource-timing-2/#sec-resource-timing
class PerformanceResourceTiming : public PerformanceEntry {
 public:
  explicit PerformanceResourceTiming(const std::string& name,
      DOMHighResTimeStamp start_time, DOMHighResTimeStamp end_time);

  // Web API.
  std::string initiator_type() const;
  DOMHighResTimeStamp fetch_start() const;
  DOMHighResTimeStamp connect_start() const;
  DOMHighResTimeStamp connect_end() const;
  DOMHighResTimeStamp secure_connection_start() const;
  DOMHighResTimeStamp request_start() const;
  DOMHighResTimeStamp response_start() const;
  DOMHighResTimeStamp response_end() const;
  unsigned long long transfer_size() const;

  std::string entry_type() const  override { return "resource"; }
  PerformanceEntryType EntryTypeEnum() const override {
    return PerformanceEntry::kResource;
  }

  DEFINE_WRAPPABLE_TYPE(PerformanceResourceTiming);

 private:
  std::string initiator_type_;
  uint64_t transfer_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PerformanceResourceTiming);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_RESOURCE_TIMING_H_

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

#ifndef COBALT_DOM_PERFORMANCE_LIFECYCLE_TIMING_H_
#define COBALT_DOM_PERFORMANCE_LIFECYCLE_TIMING_H_

#include <string>

#include "cobalt/dom/performance_entry.h"
#include "cobalt/dom/performance_high_resolution_time.h"

#include "cobalt/script/wrappable.h"
#include "net/base/load_timing_info.h"

namespace cobalt {
namespace dom {

class Performance;

class PerformanceLifecycleTiming : public PerformanceEntry {
 public:
  PerformanceLifecycleTiming(const std::string& name,
                            DOMHighResTimeStamp start_time,
                            DOMHighResTimeStamp end_time);

  // Web API.
  DOMHighResTimeStamp app_start() const;
  DOMHighResTimeStamp app_blur() const;
  DOMHighResTimeStamp app_focus() const;
  DOMHighResTimeStamp app_conceal() const;
  DOMHighResTimeStamp app_reveal() const;
  DOMHighResTimeStamp app_freeze() const;
  DOMHighResTimeStamp app_unfreeze() const;
  DOMHighResTimeStamp app_stop() const;
  std::string current_state() const;
  std::string last_state() const;

  std::string entry_type() const override { return "lifecycle"; }
  PerformanceEntryType EntryTypeEnum() const override {
    return PerformanceEntry::kLifecycle;
  }

  DEFINE_WRAPPABLE_TYPE(PerformanceLifecycleTiming);

 private:
  std::string current_state_;
  std::string last_state_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceLifecycleTiming);
};
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_LIFECYCLE_TIMING_H_
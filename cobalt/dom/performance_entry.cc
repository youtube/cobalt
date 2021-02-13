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

#include "performance_entry.h"

namespace cobalt {
namespace dom {

PerformanceEntry::PerformanceEntry(const std::string& name,
                                   DOMHighResTimeStamp start_time,
                                   DOMHighResTimeStamp finish_time)
    : duration_(finish_time - start_time),
    name_(name),
    start_time_(start_time) {}

DOMHighResTimeStamp PerformanceEntry::start_time() const {
  return start_time_;
}

DOMHighResTimeStamp PerformanceEntry::duration() const {
  return duration_;
}

PerformanceEntry::EntryType PerformanceEntry::ToEntryTypeEnum(
    const std::string& entry_type) {
  if (entry_type == "resource")
    return kResource;
  if (entry_type == "navigation")
    return kNavigation;
  return kInvalid;
}

}  // namespace dom
}  // namespace cobalt

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

#include "cobalt/dom/performance_resource_timing.h"

namespace cobalt {
namespace dom {

PerformanceResourceTiming::PerformanceResourceTiming(const std::string& name,
      DOMHighResTimeStamp start_time, DOMHighResTimeStamp end_time)
      : PerformanceEntry(name, start_time, end_time) {}

std::string PerformanceResourceTiming::initiator_type() const {
  return initiator_type_;
}

DOMHighResTimeStamp PerformanceResourceTiming::fetch_start() const {
  return 0;
}

DOMHighResTimeStamp PerformanceResourceTiming::connect_start() const {
  return 0;
}

DOMHighResTimeStamp PerformanceResourceTiming::connect_end() const {
  return 0;
}

DOMHighResTimeStamp PerformanceResourceTiming::secure_connection_start() const {
  return 0;
}

DOMHighResTimeStamp PerformanceResourceTiming::request_start() const {
  return 0;
}

DOMHighResTimeStamp PerformanceResourceTiming::response_start() const {
  return 0;
}

DOMHighResTimeStamp PerformanceResourceTiming::response_end() const {
  return 0;
}

unsigned long long PerformanceResourceTiming::transfer_size() const {
  return transfer_size_;
}

}  // namespace dom
}  // namespace cobalt

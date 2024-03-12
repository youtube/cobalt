// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_instrumentation_log.h"

#include <string>
#include <vector>

namespace cobalt {
namespace h5vcc {

CircularBuffer* InstrumentationLog::buffer_ = new CircularBuffer(128);

void InstrumentationLog::LogEvent(std::string event) {
  //  LOG(INFO) << "LogEvent";
  if (event != buffer_->GetLast()) {
    buffer_->Push(event);
  }
}

script::Sequence<std::string> InstrumentationLog::GetLogTrace() {
  script::Sequence<std::string> traceEvents;

  for (std::size_t i = 0; i < buffer_->GetValues().size(); ++i) {
    traceEvents.push_back(buffer_->GetValues()[i]);
  }

  return traceEvents;
}

std::string InstrumentationLog::GetLogTraceAsJson() {
  std::string s;
  s += "[";
  for (const auto& piece : buffer_->GetValues()) {
    std::string wrapped = "\"" + piece + "\", ";
    s += wrapped;
  }
  s.pop_back();
  s.pop_back();
  s += "]";

  return s;
}

}  // namespace h5vcc
}  // namespace cobalt

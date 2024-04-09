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

#include "cobalt/watchdog/instrumentation_log.h"

#include <string>
#include <vector>

namespace cobalt {
namespace watchdog {

bool InstrumentationLog::LogEvent(const std::string& event) {
  if (event.length() > kMaxEventLenBytes) {
    SB_DLOG(ERROR) << "[Watchdog] Log event exceeds max: " << kMaxEventLenBytes;
    return false;
  }

  starboard::ScopedLock scoped_lock(buffer_mutex_);
  buffer_.SaveToBuffer(event);

  return true;
}

std::vector<std::string> InstrumentationLog::GetLogTrace() {
  std::vector<std::string> traceEvents;

  starboard::ScopedLock scoped_lock(buffer_mutex_);
  for (auto it = buffer_.Begin(); it; ++it) {
    traceEvents.push_back(**it);
  }

  return traceEvents;
}

base::Value InstrumentationLog::GetLogTraceAsValue() {
  base::Value log_trace_value = base::Value(base::Value::Type::LIST);

  starboard::ScopedLock scoped_lock(buffer_mutex_);
  for (auto it = buffer_.Begin(); it; ++it) {
    log_trace_value.GetList().emplace_back(**it);
  }

  return log_trace_value;
}

void InstrumentationLog::ClearLog() {
  starboard::ScopedLock scoped_lock(buffer_mutex_);
  buffer_.Clear();
}

}  // namespace watchdog
}  // namespace cobalt

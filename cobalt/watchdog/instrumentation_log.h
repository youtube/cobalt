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

#ifndef COBALT_WATCHDOG_INSTRUMENTATION_LOG_H_
#define COBALT_WATCHDOG_INSTRUMENTATION_LOG_H_

#include <string>
#include <vector>

#include "base/containers/ring_buffer.h"
#include "base/values.h"
#include "starboard/common/mutex.h"

namespace cobalt {
namespace watchdog {

constexpr int kBufferSize = 128;
constexpr int kMaxEventLen = 256;

// Wrapper class on top of base::RingBuffer for tracking log events emitted
// through logEvent() h5vcc API. There's an optimization: identical sequential
// events added back to back are folded into single event.
class InstrumentationLog {
 public:
  // Append a single event to the end of the buffer.
  bool LogEvent(const std::string& event);

  // Returns a snapshot of the ring buffer.
  // Vector of kBufferSize strings at max.
  std::vector<std::string> GetLogTrace();

  // Same as GetLogTrace() but converted to base::Value for json serialization.
  base::Value GetLogTraceAsValue();

  // Empty the ring buffer.
  void ClearLog();

 private:
  base::RingBuffer<std::string, kBufferSize> buffer_;

  // Mutex to guard buffer operations. E.g. LogEvent() called from h5vcc API
  // handler and getLogTrace() called from watchdog monitoring thread.
  starboard::Mutex buffer_mutex_;
};

}  // namespace watchdog
}  // namespace cobalt
#endif  // COBALT_WATCHDOG_INSTRUMENTATION_LOG_H_

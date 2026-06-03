// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_SCOPED_TIMER_H_
#define STARBOARD_COMMON_SCOPED_TIMER_H_

#include <string_view>

#include "starboard/common/source_location.h"

namespace starboard {

// A ScopedTimer logs the time elapsed from its creation to its destruction.
// It can also be used to explicitly stop the timer and get the elapsed time.
class ScopedTimer {
 public:
  ScopedTimer(SourceLocation location = SourceLocation::current());
  ScopedTimer(std::string_view message,
              SourceLocation location = SourceLocation::current());
  ~ScopedTimer();

  // Disallow copy and assign.
  ScopedTimer(const ScopedTimer&) = delete;
  ScopedTimer& operator=(const ScopedTimer&) = delete;

  // Stops the timer, logs the elapsed time, and returns the duration in
  // microseconds.
  int64_t Stop();

 private:
  const std::string message_;
  const SourceLocation location_;
  const int64_t start_time_us_;
  bool stopped_ = false;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_SCOPED_TIMER_H_

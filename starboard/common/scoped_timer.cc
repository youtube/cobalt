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

#include "starboard/common/scoped_timer.h"

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"

namespace starboard {

ScopedTimer::ScopedTimer(SourceLocation location) : ScopedTimer("", location) {}

ScopedTimer::ScopedTimer(std::string_view message, SourceLocation location)
    : message_(message),
      location_(location),
      start_time_us_(CurrentMonotonicTime()) {}

ScopedTimer::~ScopedTimer() {
  if (!stopped_) {
    Stop();
  }
}

int64_t ScopedTimer::Stop() {
  SB_CHECK_EQ(stopped_, false);
  stopped_ = true;
  const int64_t duration_us = CurrentMonotonicTime() - start_time_us_;
  if (!message_.empty()) {
    LogMessage(location_.file(), location_.line(), SB_LOG_INFO).stream()
        << message_ << " completed: elapsed(usec)="
        << FormatWithDigitSeparators(duration_us);
  }
  return duration_us;
}

}  // namespace starboard

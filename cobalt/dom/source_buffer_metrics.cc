// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/source_buffer_metrics.h"

#include <algorithm>

#include "base/logging.h"
#include "starboard/common/string.h"

namespace cobalt {
namespace dom {

#if !defined(COBALT_BUILD_TYPE_GOLD)

namespace {

int GetBandwidth(std::size_t size, SbTimeMonotonic duration) {
  return duration == 0 ? 0 : size * kSbTimeSecond / duration;
}

}  // namespace

void SourceBufferMetrics::StartTracking() {
  DCHECK(!is_tracking_);
  is_tracking_ = true;
  wall_start_time_ = SbTimeGetMonotonicNow();
  thread_start_time_ =
      SbTimeIsTimeThreadNowSupported() ? SbTimeGetMonotonicThreadNow() : -1;
}

void SourceBufferMetrics::EndTracking(std::size_t size_appended) {
  DCHECK(is_tracking_);
  is_tracking_ = false;

  SbTimeMonotonic wall_duration = SbTimeGetMonotonicNow() - wall_start_time_;
  SbTimeMonotonic thread_duration =
      SbTimeIsTimeThreadNowSupported()
          ? SbTimeGetMonotonicThreadNow() - thread_start_time_
          : 0;
  total_wall_time_ += wall_duration;
  total_thread_time_ += thread_duration;

  total_size_ += size_appended;

  if (size_appended > 0) {
    int wall_bandwidth = GetBandwidth(size_appended, wall_duration);
    int thread_bandwidth = SbTimeIsTimeThreadNowSupported()
                               ? GetBandwidth(size_appended, thread_duration)
                               : 0;

    max_wall_bandwidth_ = std::max(max_wall_bandwidth_, wall_bandwidth);
    min_wall_bandwidth_ = std::min(min_wall_bandwidth_, wall_bandwidth);
    max_thread_bandwidth_ = std::max(max_thread_bandwidth_, thread_bandwidth);
    min_thread_bandwidth_ = std::min(min_thread_bandwidth_, thread_bandwidth);
  }
}

void SourceBufferMetrics::PrintMetrics() {
  LOG_IF(INFO, total_thread_time_ > total_wall_time_)
      << "Total thread time " << total_thread_time_
      << " should not be greater than total wall time " << total_wall_time_
      << ".";
  LOG(INFO) << starboard::FormatString(
      "AppendBuffer() metrics:\n\t%-30s%zu B\n\t%-30s%d "
      "us (%d B/s)\n\t\t%-28s%d "
      "B/s\n\t\t%-28s%d B/s\n\t%-30s%d us (%d B/s)\n\t\t%-28s%d "
      "B/s\n\t\t%-28s%d B/s",
      "Total size of appended data:", total_size_, "Total append wall time",
      total_wall_time_, GetBandwidth(total_size_, total_wall_time_),
      "Max wall bandwidth:", max_wall_bandwidth_,
      "Min wall bandwidth:", min_wall_bandwidth_,
      "Total append thread time:", total_thread_time_,
      GetBandwidth(total_size_, total_thread_time_),
      "Max thread bandwidth:", max_thread_bandwidth_,
      "Min thread bandwidth:", min_thread_bandwidth_);
}

#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

}  // namespace dom
}  // namespace cobalt

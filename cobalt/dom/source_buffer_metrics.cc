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
#include "cobalt/base/statistics.h"
#include "starboard/common/string.h"
#include "starboard/once.h"
#include "starboard/types.h"

namespace cobalt {
namespace dom {

#if !defined(COBALT_BUILD_TYPE_GOLD)

namespace {

int GetBandwidth(std::size_t size, SbTimeMonotonic duration) {
  return duration == 0 ? 0 : size * kSbTimeSecond / duration;
}

int64_t GetBandwidthForStatistics(int64_t size, int64_t duration) {
  return GetBandwidth(static_cast<std::size_t>(size), duration);
}

using BandwidthStatistics =
    base::Statistics<int64_t, SbTimeMonotonic, 1024, GetBandwidthForStatistics>;

class StatisticsWrapper {
 public:
  static StatisticsWrapper* GetInstance();
  BandwidthStatistics accumulated_wall_time_bandwidth_{
      "DOM.Performance.BandwidthStatsWallTime"};
  BandwidthStatistics accumulated_thread_time_bandwidth_{
      "DOM.Performance.BandwidthStatsThreadTime"};
};


double GetWallToThreadTimeRatio(int64_t wall_time, int64_t thread_time) {
  if (thread_time == 0) {
    thread_time = 1;
  }
  return static_cast<double>(wall_time) / thread_time;
}
}  // namespace
SB_ONCE_INITIALIZE_FUNCTION(StatisticsWrapper, StatisticsWrapper::GetInstance);

void SourceBufferMetrics::StartTracking() {
  if (!is_primary_video_) {
    return;
  }

  DCHECK(!is_tracking_);
  is_tracking_ = true;
  wall_start_time_ = SbTimeGetMonotonicNow();
  thread_start_time_ =
      SbTimeIsTimeThreadNowSupported() ? SbTimeGetMonotonicThreadNow() : -1;
}

void SourceBufferMetrics::EndTracking(std::size_t size_appended) {
  if (!is_primary_video_) {
    return;
  }

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

void SourceBufferMetrics::PrintCurrentMetricsAndUpdateAccumulatedMetrics() {
  if (!is_primary_video_) {
    return;
  }

  StatisticsWrapper::GetInstance()->accumulated_wall_time_bandwidth_.AddSample(
      total_size_, total_wall_time_);
  StatisticsWrapper::GetInstance()
      ->accumulated_thread_time_bandwidth_.AddSample(total_size_,
                                                     total_thread_time_);

  LOG_IF(INFO, total_thread_time_ > total_wall_time_)
      << "Total thread time " << total_thread_time_
      << " should not be greater than total wall time " << total_wall_time_
      << ".";

  // clang-format off
  LOG(INFO) << starboard::FormatString(
      "AppendBuffer() metrics:\n"
      "    Total size of appended data: %zu B, wall time / thread time = %02g\n"
      "    Total append wall time:      %" PRId64 " us (%d B/s)\n"
      "        Max wall bandwidth:      %d B/s\n"
      "        Min wall bandwidth:      %d B/s\n"
      "    Total append thread time:    %" PRId64 " us (%d B/s)\n"
      "        Max thread bandwidth:    %d B/s\n"
      "        Min thread bandwidth:    %d B/s\n",
      total_size_,
      GetWallToThreadTimeRatio(total_wall_time_, total_thread_time_),
      total_wall_time_, GetBandwidth(total_size_, total_wall_time_),
      max_wall_bandwidth_, min_wall_bandwidth_, total_thread_time_,
      GetBandwidth(total_size_, total_thread_time_), max_thread_bandwidth_,
      min_thread_bandwidth_);
  // clang-format on
}

void SourceBufferMetrics::PrintAccumulatedMetrics() {
  if (!is_primary_video_) {
    return;
  }

  LOG(INFO) << starboard::FormatString(
      "Accumulated AppendBuffer() metrics:\n"
      "    wall time / thread time = %02g\n"
      "    wall bandwidth statistics (B/s):\n"
      "        min %d, median %d, average %d, max %d\n"
      "    thread bandwidth statistics (B/s):\n"
      "        min %d, median %d, average %d, max %d",
      GetWallToThreadTimeRatio(
          StatisticsWrapper::GetInstance()
              ->accumulated_wall_time_bandwidth_.accumulated_divisor(),
          StatisticsWrapper::GetInstance()
              ->accumulated_thread_time_bandwidth_.accumulated_divisor()),
      static_cast<int>(StatisticsWrapper::GetInstance()
                           ->accumulated_wall_time_bandwidth_.min()),
      static_cast<int>(StatisticsWrapper::GetInstance()
                           ->accumulated_wall_time_bandwidth_.GetMedian()),
      static_cast<int>(StatisticsWrapper::GetInstance()
                           ->accumulated_wall_time_bandwidth_.average()),
      static_cast<int>(StatisticsWrapper::GetInstance()
                           ->accumulated_wall_time_bandwidth_.max()),
      static_cast<int>(StatisticsWrapper::GetInstance()
                           ->accumulated_thread_time_bandwidth_.min()),
      static_cast<int>(StatisticsWrapper::GetInstance()
                           ->accumulated_thread_time_bandwidth_.GetMedian()),
      static_cast<int>(StatisticsWrapper::GetInstance()
                           ->accumulated_thread_time_bandwidth_.average()),
      static_cast<int>(StatisticsWrapper::GetInstance()
                           ->accumulated_thread_time_bandwidth_.max()));
}

#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

}  // namespace dom
}  // namespace cobalt

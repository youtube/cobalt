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

#ifndef COBALT_DOM_SOURCE_BUFFER_METRICS_H_
#define COBALT_DOM_SOURCE_BUFFER_METRICS_H_

#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "starboard/types.h"

namespace cobalt {
namespace dom {

enum class SourceBufferMetricsAction : uint8_t {
  PREPARE_APPEND,
  APPEND_BUFFER,
};

class SourceBufferMetrics {
 public:
  SourceBufferMetrics(
      bool is_primary_video,
      const base::TickClock* clock = base::DefaultTickClock::GetInstance())
      : is_primary_video_(is_primary_video), clock_(clock) {}
  ~SourceBufferMetrics() = default;

  void StartTracking();
  void EndTracking(SourceBufferMetricsAction action, size_t size_appended);

#if defined(COBALT_BUILD_TYPE_GOLD)
  void PrintCurrentMetricsAndUpdateAccumulatedMetrics() {}
  void PrintAccumulatedMetrics() {}
#else   // defined(COBALT_BUILD_TYPE_GOLD)
  void PrintCurrentMetricsAndUpdateAccumulatedMetrics();
  void PrintAccumulatedMetrics();
#endif  // defined(COBALT_BUILD_TYPE_GOLD)

 private:
  SourceBufferMetrics(const SourceBufferMetrics&) = delete;
  SourceBufferMetrics& operator=(const SourceBufferMetrics&) = delete;

  void RecordTelemetry(SourceBufferMetricsAction action,
                       const base::TimeDelta& action_duration);

  base::TimeTicks wall_start_time_;
  int64_t thread_start_time_;

  const bool is_primary_video_;
  bool is_tracking_ = false;
  const base::TickClock* clock_;

#if !defined(COBALT_BUILD_TYPE_GOLD)
  size_t total_size_ = 0;
  int64_t total_thread_time_ = 0;
  int64_t total_wall_time_ = 0;
  int max_thread_bandwidth_ = 0;
  int min_thread_bandwidth_ = INT_MAX;
  int max_wall_bandwidth_ = 0;
  int min_wall_bandwidth_ = INT_MAX;
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_SOURCE_BUFFER_METRICS_H_

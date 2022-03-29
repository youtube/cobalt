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

#include "starboard/time.h"

namespace cobalt {
namespace dom {

#if defined(COBALT_BUILD_TYPE_GOLD)

class SourceBufferMetrics {
 public:
  SourceBufferMetrics() = default;
  ~SourceBufferMetrics() = default;

  void StartTracking() {}
  void EndTracking(size_t size_appended) {}
  void PrintMetrics() {}
};

#else  // defined(COBALT_BUILD_TYPE_GOLD)

class SourceBufferMetrics {
 public:
  SourceBufferMetrics() = default;
  ~SourceBufferMetrics() = default;

  void StartTracking();
  void EndTracking(size_t size_appended);
  void PrintMetrics();

 private:
  SbTimeMonotonic wall_start_time_ = 0;
  SbTimeMonotonic thread_start_time_ = 0;

  bool is_tracking_ = false;

  size_t total_size_ = 0;
  int total_thread_time_ = 0;
  int total_wall_time_ = 0;
  int max_thread_bandwidth_ = 0;
  int min_thread_bandwidth_ = INT_MAX;
  int max_wall_bandwidth_ = 0;
  int min_wall_bandwidth_ = INT_MAX;
};

#endif  // defined(COBALT_BUILD_TYPE_GOLD)

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_SOURCE_BUFFER_METRICS_H_

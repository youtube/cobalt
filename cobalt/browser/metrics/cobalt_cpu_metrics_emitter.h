// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_METRICS_COBALT_CPU_METRICS_EMITTER_H_
#define COBALT_BROWSER_METRICS_COBALT_CPU_METRICS_EMITTER_H_

#include <memory>

#include "base/process/process_metrics.h"
#include "cobalt/common/cobalt_thread_checker.h"

namespace cobalt {

// This class fetches CPU metrics for the current process and its threads,
// and emits UMA metrics. It maintains state to calculate deltas between
// emissions. This class is not thread-safe and must be called from the same
// TaskRunner it was created on.
class CobaltCpuMetricsEmitter {
 public:
  CobaltCpuMetricsEmitter();

  CobaltCpuMetricsEmitter(const CobaltCpuMetricsEmitter&) = delete;
  CobaltCpuMetricsEmitter& operator=(const CobaltCpuMetricsEmitter&) = delete;

  void FetchAndEmitCpuMetrics();
  virtual ~CobaltCpuMetricsEmitter();

 private:
  std::unique_ptr<base::ProcessMetrics> process_metrics_;

  COBALT_THREAD_CHECKER(thread_checker_);
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_CPU_METRICS_EMITTER_H_

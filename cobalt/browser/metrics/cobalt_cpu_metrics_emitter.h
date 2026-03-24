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

#include "base/memory/ref_counted.h"
#include "base/process/process_metrics.h"
#include "base/sequence_checker.h"

namespace cobalt {

// This class fetches CPU metrics for the current process and its threads and
// records the average per-core CPU usage as a percentage in a UMA histogram.
// It relies on a persistent base::ProcessMetrics object (tracked by
// CobaltMetricsServiceClient::CpuPollingState) to maintain stateful CPU usage
// tracking across multiple calls. This class is not thread-safe and must be
// called from the same Sequence it was created on.
class CobaltCpuMetricsEmitter
    : public base::RefCountedThreadSafe<CobaltCpuMetricsEmitter> {
 public:
  CobaltCpuMetricsEmitter();

  CobaltCpuMetricsEmitter(const CobaltCpuMetricsEmitter&) = delete;
  CobaltCpuMetricsEmitter& operator=(const CobaltCpuMetricsEmitter&) = delete;

  void FetchAndEmitCpuMetrics(base::ProcessMetrics* process_metrics);

 protected:
  // TODO(b/492251096): add tests for CPU metrics emitter class
  virtual ~CobaltCpuMetricsEmitter();

 private:
  friend class base::RefCountedThreadSafe<CobaltCpuMetricsEmitter>;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_CPU_METRICS_EMITTER_H_

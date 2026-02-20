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

#ifndef COBALT_BROWSER_METRICS_COBALT_MEMORY_METRICS_EMITTER_H_
#define COBALT_BROWSER_METRICS_COBALT_MEMORY_METRICS_EMITTER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"

namespace cobalt {

// This class asynchronously fetches memory metrics for each process, and then
// emits UMA metrics from those metrics.
// It's a Cobalt-specific, minimal version of Chromium's
// ProcessMemoryMetricsEmitter.
class CobaltMemoryMetricsEmitter
    : public base::RefCountedThreadSafe<CobaltMemoryMetricsEmitter> {
 public:
  // Prefer predefined ranges kLarge, kSmall and kTiny over custom ranges.
  enum class MetricSize {
    kPercentage,  // percentages, 0% - 100%
    kLarge,       // 1MiB - 64,000MiB
    kSmall,       // 10 - 500,000KiB
    kTiny,        // 1 - 500,000B
    kCustom,      // custom range, in bytes
  };

  enum class EmitTo {
    kCountsInUkmOnly,
    kCountsInUkmAndSizeInUma,
    kSizeInUkmAndUma,
    kSizeInUmaOnly,
    kIgnored
  };

  struct MetricRange {
    const int min;
    const int max;
  };

  struct Metric {
    const char* const dump_name;
    const char* const uma_name;
    const MetricSize metric_size;
    const char* const metric;
    const EmitTo target;
    const MetricRange range;
  };

  CobaltMemoryMetricsEmitter();

  CobaltMemoryMetricsEmitter(const CobaltMemoryMetricsEmitter&) = delete;
  CobaltMemoryMetricsEmitter& operator=(const CobaltMemoryMetricsEmitter&) =
      delete;

  virtual void FetchAndEmitProcessMemoryMetrics();

  void set_callback_for_testing(base::OnceClosure callback) {
    callback_for_testing_ = std::move(callback);
  }

 protected:
  virtual ~CobaltMemoryMetricsEmitter();

  virtual void ReceivedMemoryDump(
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);

 private:
  friend class base::RefCountedThreadSafe<CobaltMemoryMetricsEmitter>;

  static void EmitProcessUma(
      memory_instrumentation::HistogramProcessType process_type,
      const Metric& item,
      uint64_t value);

  void CollateResults();

  bool memory_dump_in_progress_ = false;
  std::unique_ptr<memory_instrumentation::GlobalMemoryDump> global_dump_;

  base::OnceClosure callback_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_MEMORY_METRICS_EMITTER_H_

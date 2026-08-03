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

#ifndef COBALT_BROWSER_METRICS_COBALT_MEMORY_METRICS_HELPER_H_
#define COBALT_BROWSER_METRICS_COBALT_MEMORY_METRICS_HELPER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cobalt {

struct MemoryBreakdownMetric {
  const char* name;
  uint64_t value_bytes;
};

// Helper class for extracting live Cobalt/Chromium memory breakdown metrics
// from base::StatisticsRecorder. Aligns with go/kimono-memory-metrics.
class CobaltMemoryMetricsHelper {
 public:
  // Standard memory breakdown metrics tracked in PLX / Kimono telemetry
  // configs.
  static constexpr const char* kMemoryBreakdownMetricNames[] = {
      "Memory.Browser.ResidentSet",
      "Memory.Browser.PrivateMemoryFootprint",
      "Memory.Experimental.Browser2.Malloc",
      "Memory.Experimental.Browser2.PartitionAlloc",
      "Memory.Experimental.Browser2.V8",
      "Memory.Experimental.Browser2.BlinkGC",
      "Memory.Experimental.Browser2.Skia",
      "Memory.Browser.LibChrobaltRss",
      "Memory.Experimental.Browser2.CodeOther",
      "Memory.Experimental.Browser2.Fonts",
      "Memory.Experimental.Browser2.Stacks",
      "Memory.GPU.PeakMemoryUsage2.PageLoad"};

  // Query a single metric by histogram name. Returns value in bytes if found.
  static std::optional<uint64_t> GetMetricValueBytes(
      const std::string& metric_name);

  // Queries all target memory breakdown metrics currently present in
  // StatisticsRecorder.
  static std::vector<MemoryBreakdownMetric> GetMemoryBreakdown();
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_MEMORY_METRICS_HELPER_H_

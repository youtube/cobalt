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

#include "cobalt/browser/metrics/cobalt_memory_metrics_helper.h"

#include <iterator>
#include <memory>

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"

namespace cobalt {

namespace {

// UMA Memory.Experimental.Browser2.* histograms record values in KiB
// (Kilobytes). Convert KiB to bytes for consistent DevTools byte reporting.
constexpr uint64_t kBytesPerKiB = 1024;

bool IsKiBHistogram(const std::string& metric_name) {
  return metric_name.rfind("Memory.Experimental.Browser2.", 0) == 0 ||
         metric_name == "Memory.Browser.ResidentSet" ||
         metric_name == "Memory.Browser.PrivateMemoryFootprint" ||
         metric_name == "Memory.Browser.LibChrobaltRss" ||
         metric_name == "Memory.GPU.PeakMemoryUsage2.PageLoad";
}

}  // namespace

constexpr const char* CobaltMemoryMetricsHelper::kMemoryBreakdownMetricNames[];

std::optional<uint64_t> CobaltMemoryMetricsHelper::GetMetricValueBytes(
    const std::string& metric_name) {
#if defined(OFFICIAL_BUILD)
  return std::nullopt;
#else
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(metric_name);
  if (!histogram) {
    return std::nullopt;
  }

  std::unique_ptr<base::HistogramSamples> samples =
      histogram->SnapshotSamples();
  if (!samples || samples->TotalCount() == 0) {
    return std::nullopt;
  }

  uint64_t raw_sum = static_cast<uint64_t>(samples->sum());
  if (IsKiBHistogram(metric_name)) {
    return raw_sum * kBytesPerKiB;
  }

  return raw_sum;
#endif
}

std::vector<MemoryBreakdownMetric>
CobaltMemoryMetricsHelper::GetMemoryBreakdown() {
  std::vector<MemoryBreakdownMetric> breakdown;
#if !defined(OFFICIAL_BUILD)
  breakdown.reserve(std::size(kMemoryBreakdownMetricNames));

  for (const char* metric_name : kMemoryBreakdownMetricNames) {
    auto value = GetMetricValueBytes(metric_name);
    if (value.has_value()) {
      breakdown.push_back({metric_name, value.value()});
    }
  }
#endif
  return breakdown;
}

}  // namespace cobalt

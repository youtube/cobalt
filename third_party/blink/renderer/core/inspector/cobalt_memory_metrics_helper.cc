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

#include "third_party/blink/renderer/core/inspector/cobalt_memory_metrics_helper.h"

#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_util.h"

namespace blink {

namespace {

// All Cobalt memory breakdown histograms (emitted in
// cobalt_memory_metrics_emitter.cc and peak_gpu_memory_callback.cc via
// base::UmaHistogramMemoryLargeMB) store values in MiB (Mebibytes). We convert
// MiB to bytes (1 MiB = 1,048,576 bytes) for consistent DevTools byte
// reporting.
constexpr uint64_t kBytesPerMiB = 1024 * 1024;

bool IsMiBHistogram(std::string_view metric_name) {
  return base::StartsWith(metric_name, "Memory.Experimental.Browser2.",
                          base::CompareCase::SENSITIVE) ||
         metric_name == "Memory.Browser.ResidentSet" ||
         metric_name == "Memory.Browser.PrivateMemoryFootprint" ||
         metric_name == "Memory.Browser.LibChrobaltRss" ||
         metric_name == "Memory.GPU.PeakMemoryUsage2.PageLoad";
}

}  // namespace

std::optional<uint64_t> GetMetricValueBytes(std::string_view metric_name) {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(metric_name);
  if (!histogram) {
    return std::nullopt;
  }

  std::unique_ptr<base::HistogramSamples> samples =
      histogram->SnapshotSamples();
  int64_t total_count = samples ? samples->TotalCount() : 0;
  if (!samples || total_count <= 0) {
    return std::nullopt;
  }

  int64_t target_count = (total_count + 1) / 2;
  int64_t accumulated_count = 0;
  uint64_t p50_value = 0;

  // Compute p50 (median) sample from accumulated bucket samples, mirroring
  // the field percentile calculation in
  // cobalt/tools/uma/interpret_uma_histogram.py (which uses the bucket upper
  // bound max/high).
  for (std::unique_ptr<base::SampleCountIterator> it = samples->Iterator();
       !it->Done(); it->Next()) {
    base::HistogramBase::Sample32 min;
    int64_t max;
    base::HistogramBase::Count32 count;
    it->Get(&min, &max, &count);
    accumulated_count += count;
    if (accumulated_count >= target_count) {
      p50_value = static_cast<uint64_t>(max);
      break;
    }
  }

  if (IsMiBHistogram(metric_name)) {
    return p50_value * kBytesPerMiB;
  }

  return p50_value;
}

std::optional<std::vector<MemoryBreakdownMetric>> GetMemoryBreakdown() {
  std::vector<MemoryBreakdownMetric> breakdown;
  breakdown.reserve(std::size(kMemoryBreakdownMetricNames));

  for (const char* metric_name : kMemoryBreakdownMetricNames) {
    auto value = GetMetricValueBytes(metric_name);
    if (value.has_value()) {
      breakdown.push_back({metric_name, value.value()});
    }
  }

  if (breakdown.empty()) {
    return std::nullopt;
  }

  return breakdown;
}

}  // namespace blink

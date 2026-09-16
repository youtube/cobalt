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

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/process/process_metrics.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// All Cobalt memory breakdown histograms (emitted via base::UmaHistogramMemoryLargeMB)
// store values in MiB. We convert MiB to bytes (1 MiB = 1,048,576 bytes) for
// consistent DevTools byte reporting.
constexpr uint64_t kBytesPerMiB = 1024 * 1024;

bool IsMiBHistogram(std::string_view metric_name) {
  for (const char* canonical_name : kCanonicalMemoryMetricNames) {
    if (canonical_name && metric_name == canonical_name) {
      return true;
    }
  }
  for (const char* guardrail_name : kPeakMemoryGuardrailMetricNames) {
    if (guardrail_name && metric_name == guardrail_name) {
      return true;
    }
  }
  return false;
}

// Extracts a representative sample value from a histogram bucket. Uses `max`
// (the upper bound) for normal buckets to represent the peak/median, but falls
// back to `min` (the inclusive lower bound) if the sample is in the overflow
// bucket where `max` is set to INT_MAX (which would otherwise report petabytes).
// Also guards with std::max(..., 0) against negative underflow buckets.
uint64_t GetSafeBucketValue(base::HistogramBase::Sample32 min, int64_t max) {
  if (max >= std::numeric_limits<base::HistogramBase::Sample32>::max()) {
    return static_cast<uint64_t>(
        std::max<base::HistogramBase::Sample32>(min, 0));
  }
  return static_cast<uint64_t>(std::max<int64_t>(max, 0));
}

}  // namespace

std::optional<uint64_t> GetP50MetricValueBytes(std::string_view metric_name) {
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

  std::unique_ptr<base::SampleCountIterator> it = samples->Iterator();
  if (!it) {
    return std::nullopt;
  }

  int64_t target_count = (total_count + 1) / 2;
  int64_t accumulated_count = 0;
  uint64_t p50_value = 0;

  // Compute p50 (median) sample from accumulated bucket samples, mirroring
  // the field percentile calculation in
  // cobalt/tools/uma/interpret_uma_histogram.py (which uses the bucket upper
  // bound max/high).
  for (; !it->Done(); it->Next()) {
    base::HistogramBase::Sample32 min;
    int64_t max;
    base::HistogramBase::Count32 count;
    it->Get(&min, &max, &count);
    accumulated_count += count;
    if (accumulated_count >= target_count) {
      p50_value = GetSafeBucketValue(min, max);
      break;
    }
  }

  if (IsMiBHistogram(metric_name)) {
    return p50_value * kBytesPerMiB;
  }

  return p50_value;
}

std::optional<std::vector<MemoryBreakdownMetric>> GetP50MemoryBreakdown() {
  std::vector<MemoryBreakdownMetric> breakdown;
  breakdown.reserve(std::size(kCanonicalMemoryMetricNames));

  for (const char* canonical_name : kCanonicalMemoryMetricNames) {
    if (!canonical_name) {
      continue;
    }
    auto value = GetP50MetricValueBytes(canonical_name);
    if (value.has_value()) {
      breakdown.push_back(
          {base::StrCat({canonical_name, kP50Suffix}), *value});
    }
  }

  if (breakdown.empty()) {
    return std::nullopt;
  }

  return breakdown;
}

std::optional<std::vector<MemoryBreakdownMetric>> GetLiveMemoryBreakdown() {
#if !BUILDFLAG(IS_STARBOARD) && !BUILDFLAG(IS_ANDROID)
  return std::nullopt;
#else
  std::vector<MemoryBreakdownMetric> metrics;
  metrics.reserve(std::size(kCanonicalMemoryMetricNames));

  // 1. Process OS Memory (ResidentSet and PrivateMemoryFootprint)
  std::unique_ptr<base::ProcessMetrics> process_metrics =
      base::ProcessMetrics::CreateCurrentProcessMetrics();
  if (process_metrics) {
    auto memory_info = process_metrics->GetMemoryInfo();
    if (memory_info.has_value()) {
      if (memory_info->resident_set_bytes > 0) {
        metrics.push_back(
            {base::StrCat(
                 {kCanonicalMemoryMetricNames[CobaltMemoryMetricId::kResidentSet],
                  kLiveSuffix}),
             memory_info->resident_set_bytes});
      }
      if (memory_info->rss_anon_bytes > 0) {
        metrics.push_back(
            {base::StrCat(
                 {kCanonicalMemoryMetricNames[
                      CobaltMemoryMetricId::kPrivateMemoryFootprint],
                  kLiveSuffix}),
             memory_info->rss_anon_bytes});
      }
    }
  }

  // 2. V8 Live Heap Memory
  if (v8::Isolate* isolate = v8::Isolate::TryGetCurrent()) {
    v8::HeapStatistics heap_stats;
    isolate->GetHeapStatistics(&heap_stats);
    if (heap_stats.used_heap_size() > 0) {
      metrics.push_back(
          {base::StrCat(
               {kCanonicalMemoryMetricNames[CobaltMemoryMetricId::kV8],
                kLiveSuffix}),
           heap_stats.used_heap_size()});
    }
  }

  if (metrics.empty()) {
    return std::nullopt;
  }

  return metrics;
#endif  // BUILDFLAG(IS_STARBOARD) || BUILDFLAG(IS_ANDROID)
}

std::optional<uint64_t> GetPeakHistogramValueBytes(
    std::string_view metric_name) {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(metric_name);
  if (!histogram) {
    return std::nullopt;
  }

  std::unique_ptr<base::HistogramSamples> samples =
      histogram->SnapshotSamples();
  if (!samples || samples->TotalCount() <= 0) {
    return std::nullopt;
  }

  std::unique_ptr<base::SampleCountIterator> it = samples->Iterator();
  if (!it) {
    return std::nullopt;
  }

  uint64_t peak_value = 0;
  for (; !it->Done(); it->Next()) {
    base::HistogramBase::Sample32 min;
    int64_t max;
    base::HistogramBase::Count32 count;
    it->Get(&min, &max, &count);
    uint64_t bucket_value = GetSafeBucketValue(min, max);
    if (count > 0 && bucket_value > peak_value) {
      peak_value = bucket_value;
    }
  }

  if (IsMiBHistogram(metric_name)) {
    return peak_value * kBytesPerMiB;
  }

  return peak_value;
}

std::optional<std::vector<MemoryBreakdownMetric>> GetPeakMemoryGuardrails() {
  std::vector<MemoryBreakdownMetric> guardrails;
  guardrails.reserve(std::size(kPeakMemoryGuardrailMetricNames));

  for (const char* metric_name : kPeakMemoryGuardrailMetricNames) {
    if (!metric_name) {
      continue;
    }
    auto value = GetPeakHistogramValueBytes(metric_name);
    if (value.has_value()) {
      guardrails.push_back({metric_name, *value});
    }
  }

  if (guardrails.empty()) {
    return std::nullopt;
  }

  return guardrails;
}

}  // namespace blink

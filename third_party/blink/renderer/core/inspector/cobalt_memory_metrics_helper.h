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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_COBALT_MEMORY_METRICS_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_COBALT_MEMORY_METRICS_HELPER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blink {

struct MemoryBreakdownMetric {
  std::string name;
  uint64_t value_bytes;
};

// Suffix identifiers for distinguishing statistical medians vs. live meters.
inline constexpr char kP50Suffix[] = ".P50";
inline constexpr char kLiveSuffix[] = ".Live";

// Metric indices for structured reference across helper functions.
enum CobaltMemoryMetricId {
  kResidentSet = 0,
  kPrivateMemoryFootprint,
  kMalloc,
  kPartitionAlloc,
  kV8,
  kBlinkGC,
  kSkia,
  kLibChrobaltRss,
  kCodeOther,
  kFonts,
  kStacks,
  kJavaHeap,
  kPeakMemoryUsagePageLoad,
  kNumCobaltMemoryMetrics
};

// Canonical UMA memory breakdown metric names tracked in PLX / Kimono telemetry configs.
inline constexpr const char* kCanonicalMemoryMetricNames[kNumCobaltMemoryMetrics] = {
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
    "Memory.Experimental.Browser2.JavaHeap",
    "Memory.GPU.PeakMemoryUsage2.PageLoad"};

// ============================================================================
// 1. Session Median (P50) Memory Breakdown Metrics (UMA Histograms)
// ============================================================================
// Helper functions for extracting session median (P50) Cobalt/Chromium memory
// breakdown metrics from base::StatisticsRecorder. Aligns with
// go/kimono-memory-metrics and mirrors the field p50 metric calculation in
// interpret_uma_histogram.py.

// Query a single P50 metric by base histogram name. Returns value in bytes if found.
std::optional<uint64_t> GetP50MetricValueBytes(std::string_view metric_name);

// Queries all target P50 memory breakdown metrics currently present in
// StatisticsRecorder. Returns std::nullopt if no metrics are recorded.
std::optional<std::vector<MemoryBreakdownMetric>> GetP50MemoryBreakdown();

// ============================================================================
// 2. Real-Time Instantaneous Live Memory Metrics
// ============================================================================
// Queries the active, instantaneous memory footprints from process metrics
// and runtime subsystem allocators (e.g. resident set, private footprint,
// malloc, v8).

// Queries all available live instantaneous memory breakdown metrics.
std::optional<std::vector<MemoryBreakdownMetric>> GetLiveMemoryBreakdown();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_COBALT_MEMORY_METRICS_HELPER_H_

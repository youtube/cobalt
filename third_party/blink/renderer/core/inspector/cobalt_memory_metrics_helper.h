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
#include <string_view>
#include <vector>

namespace blink {

struct MemoryBreakdownMetric {
  const char* name;
  uint64_t value_bytes;
};

// Helper functions for extracting live Cobalt/Chromium memory breakdown metrics
// from base::StatisticsRecorder. Aligns with go/kimono-memory-metrics and
// mirrors the field p50 (median) metric calculation in
// interpret_uma_histogram.py. Used by Blink CDP Performance domain
// (InspectorPerformanceAgent::getMetrics) and DevTools memory breakdown
// inspection.
//
// Threading Model:
// These non-member functions are thread-safe and can be called from any thread
// or TaskRunner, as they rely on the thread-safe base::StatisticsRecorder.

// Standard memory breakdown metrics tracked in PLX / Kimono telemetry
// configs.
constexpr const char* kMemoryBreakdownMetricNames[] = {
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

// Query a single metric by histogram name. Returns value in bytes if found.
std::optional<uint64_t> GetMetricValueBytes(std::string_view metric_name);

// Queries all target memory breakdown metrics currently present in
// StatisticsRecorder. Returns std::nullopt if no metrics are recorded.
std::optional<std::vector<MemoryBreakdownMetric>> GetMemoryBreakdown();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_COBALT_MEMORY_METRICS_HELPER_H_

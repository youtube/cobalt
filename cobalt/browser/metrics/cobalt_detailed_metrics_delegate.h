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

#ifndef COBALT_BROWSER_METRICS_COBALT_DETAILED_METRICS_DELEGATE_H_
#define COBALT_BROWSER_METRICS_COBALT_DETAILED_METRICS_DELEGATE_H_

#include <cstdint>
#include <string>

#include "base/containers/flat_map.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/detailed_metrics_delegate.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"

namespace cobalt {

// Detailed memory metric keys.
inline constexpr char kDetailedMetricPssPrefix[] = "pss:";
inline constexpr char kDetailedMetricRssPrefix[] = "rss:";

inline constexpr char kDetailedMetricCobaltCore[] = "cobalt_core";
inline constexpr char kDetailedMetricV8[] = "v8";
inline constexpr char kDetailedMetricPartitionAlloc[] = "partition_alloc";
inline constexpr char kDetailedMetricMalloc[] = "malloc";
inline constexpr char kDetailedMetricAndroidRuntime[] = "android_runtime";
inline constexpr char kDetailedMetricGraphics[] = "graphics";
inline constexpr char kDetailedMetricFonts[] = "fonts";
inline constexpr char kDetailedMetricAshmemJit[] = "ashmem_jit";
inline constexpr char kDetailedMetricStacks[] = "stacks";
inline constexpr char kDetailedMetricCodeOther[] = "code_other";
inline constexpr char kDetailedMetricAnonymousOther[] = "anonymous_other";
inline constexpr char kDetailedMetricOther[] = "other";

// Cobalt-specific implementation of the DetailedMetricsDelegate.
// Categorizes /proc/self/smaps mappings into Cobalt-specific categories.
class CobaltDetailedMetricsDelegate
    : public memory_instrumentation::DetailedMetricsDelegate {
 public:
  CobaltDetailedMetricsDelegate();
  ~CobaltDetailedMetricsDelegate() override;

  // DetailedMetricsDelegate implementation.
  void OnSmapsLine(absl::string_view line) override;
  base::flat_map<std::string, uint32_t> GetAndResetStats() override;

 private:
  enum class Category {
    kNone,
    kCobaltCore,
    kV8,
    kPartitionAlloc,
    kMalloc,
    kAndroidRuntime,
    kGraphics,
    kFonts,
    kAshmemJit,
    kStacks,
    kCodeOther,
    kAnonymousOther,
    kOther
  };

  struct Stats {
    uint64_t pss_kb = 0;
    uint64_t rss_kb = 0;
  };

  Category GetCategory(absl::string_view line);

  Category current_category_ = Category::kNone;

  Stats cobalt_core_;
  Stats v8_;
  Stats partition_alloc_;
  Stats malloc_;
  Stats android_runtime_;
  Stats graphics_;
  Stats fonts_;
  Stats ashmem_jit_;
  Stats stacks_;
  Stats code_other_;
  Stats anonymous_other_;
  Stats other_;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_DETAILED_METRICS_DELEGATE_H_

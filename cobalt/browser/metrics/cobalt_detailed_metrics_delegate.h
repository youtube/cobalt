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

namespace cobalt {

// Cobalt-specific implementation of the DetailedMetricsDelegate.
// Categorizes /proc/self/smaps mappings into Cobalt-specific categories.
class CobaltDetailedMetricsDelegate
    : public memory_instrumentation::DetailedMetricsDelegate {
 public:
  CobaltDetailedMetricsDelegate();
  ~CobaltDetailedMetricsDelegate() override;

  // DetailedMetricsDelegate implementation.
  bool OnSmapsBuffer(std::string_view line) override;
  memory_instrumentation::DetailedMetrics GetAndResetStats() override;

 private:
  enum class Category {
    kNone,
    kCobaltCore,
    kLibChrobalt,
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

  Category GetCategory(std::string_view line);

  Category current_category_ = Category::kNone;

  Stats cobalt_core_;
  Stats lib_chrobalt_;
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

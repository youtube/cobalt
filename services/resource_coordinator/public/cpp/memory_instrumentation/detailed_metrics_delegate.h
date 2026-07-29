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

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_DETAILED_METRICS_DELEGATE_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_DETAILED_METRICS_DELEGATE_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"

namespace memory_instrumentation {

// Optimized structure for smaps metrics.
struct COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION)
    SmapsMetrics {
  uint64_t rss_kb = 0;
  uint64_t pss_kb = 0;
  uint64_t private_dirty_kb = 0;
  uint64_t private_clean_kb = 0;
  uint64_t shared_dirty_kb = 0;
  uint64_t shared_clean_kb = 0;
  uint64_t swap_kb = 0;
  uint64_t swap_pss_kb = 0;
};

// Interface for project-specific detailed memory metrics collection.
class COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION)
    DetailedMetricsDelegate {
 public:
  virtual ~DetailedMetricsDelegate() = default;

  // Called for each parsed entry. |name| is transient and must be copied if
  // needed. Implementations must be thread-safe.
  virtual void OnSmapsEntry(std::string_view name,
                            const SmapsMetrics& metrics) = 0;

  // Swaps the internal results map with an empty one and returns it via |stats|.
  virtual void GetAndResetStats(
      base::flat_map<std::string, uint64_t>* stats) = 0;

  virtual base::WeakPtr<DetailedMetricsDelegate> GetWeakPtr() = 0;
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_DETAILED_METRICS_DELEGATE_H_


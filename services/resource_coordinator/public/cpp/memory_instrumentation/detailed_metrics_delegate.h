// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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


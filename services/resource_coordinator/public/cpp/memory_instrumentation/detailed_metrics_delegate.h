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

namespace memory_instrumentation {

// Results returned by the delegate after parsing.
struct COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION)
    DetailedMetrics {
  DetailedMetrics();
  ~DetailedMetrics();
  DetailedMetrics(DetailedMetrics&&);
  DetailedMetrics& operator=(DetailedMetrics&&);

  // Map of category names to accumulated metrics (e.g. RSS or PSS) in KB.
  // Mandate: Keys must be namespaced and PII-free.
  base::flat_map<std::string, uint32_t> categories_kb;

  // Reported totals for harness-level validation.
  uint32_t total_pss_kb = 0;
  uint32_t total_rss_kb = 0;
};

// Interface for project-specific detailed memory metrics collection.
// The harness reads /proc/self/smaps and passes each line to the registered
// delegate for categorization.
class COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION)
    DetailedMetricsDelegate {
 public:
  virtual ~DetailedMetricsDelegate() = default;

  // Called with a buffer containing a single line from /proc/self/smaps.
  // The |buffer| view is only valid for the duration of this call.
  // Returns true if parsing should continue, false if it should abort.
  virtual bool OnSmapsBuffer(std::string_view buffer) = 0;

  // Called after all lines have been processed to retrieve the aggregated
  // metrics and reset internal counters for the next dump.
  // TODO(cobalt): Implement project-specific library tracking (e.g. libchrobalt)
  // here to avoid redundant scans of /proc/self/smaps.
  virtual DetailedMetrics GetAndResetStats() = 0;
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_DETAILED_METRICS_DELEGATE_H_

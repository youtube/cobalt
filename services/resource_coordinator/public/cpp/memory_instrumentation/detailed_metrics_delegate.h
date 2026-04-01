// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_DETAILED_METRICS_DELEGATE_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_DETAILED_METRICS_DELEGATE_H_

#include <cstdint>
#include <string>

#include "base/containers/flat_map.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"

namespace memory_instrumentation {

// Interface for project-specific detailed memory metrics collection.
// The harness (SmapsCategorizer) reads /proc/self/smaps and passes each
// mapping line to the registered delegate for categorization.
class DetailedMetricsDelegate {
 public:
  virtual ~DetailedMetricsDelegate() = default;

  // Called for each line in /proc/self/smaps.
  virtual void OnSmapsLine(absl::string_view line) = 0;

  // Called after all lines have been processed to retrieve the aggregated
  // metrics and reset internal counters for the next dump.
  virtual base::flat_map<std::string, uint32_t> GetAndResetStats() = 0;
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_DETAILED_METRICS_DELEGATE_H_

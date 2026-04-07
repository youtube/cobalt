// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

class SmapsCategorizer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual mojom::CobaltMemoryCategory GetCategory(
        const std::string& mapped_file) const = 0;
  };

  // Parses /proc/self/smaps and aggregates PSS by category.
  // Address discarding and path scrubbing are handled by the Delegate and
  // the parser logic.
  static mojom::DetailedMemoryStats ParseSmaps(
      const base::FilePath& smaps_path,
      const Delegate& delegate);

  // For testing with string content.
  static mojom::DetailedMemoryStats ParseSmapsContent(
      const std::string& content,
      const Delegate& delegate);

  // Parses a specific metric (e.g. "Pss:") from a smaps-like file.
  static uint32_t ParseSmapsMetric(
      const base::FilePath& smaps_path,
      const std::string& metric_name);

  // For testing.
  static uint32_t ParseSmapsMetricContent(
      const std::string& content,
      const std::string& metric_name);
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_H_

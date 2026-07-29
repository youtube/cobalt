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

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/detailed_metrics_delegate.h"

namespace memory_instrumentation {

class ParsedSmapsEntry {
 public:
  std::string name;
  SmapsMetrics metrics;
};
using ParsedSmapsResults = std::vector<ParsedSmapsEntry>;

// Manages the lifecycle of /proc/self/smaps parsing.
//
// This class uses a sequence-affine front-end to handle request coalescing and
// protect internal state (e.g., `pending_callbacks_`), while
// offloading the heavy I/O and parsing to a background ThreadPool task.
//
// Although it may currently be accessed from a single sequence, maintaining
// sequence affinity for `RequestDump()` ensures that it can safely support
// requests from different sequences/TaskRunners in the future (especially for
// upstream compatibility) without introducing race conditions or requiring
// complex locking.
class COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION)
    SmapsCategorizer {
 public:
  explicit SmapsCategorizer(DetailedMetricsDelegate* delegate);
  ~SmapsCategorizer();

  SmapsCategorizer(const SmapsCategorizer&) = delete;
  SmapsCategorizer& operator=(const SmapsCategorizer&) = delete;

  // Requests a memory dump. If a scan is already in progress, the callback
  // will be queued and called when the current scan completes.
  // Must be called on the same sequence the categorizer was created on.
  void RequestDump(base::OnceClosure done);

  // Synchronously scans a smaps file.
  static std::optional<ParsedSmapsResults> ScanSmapsFile(
      const base::FilePath& path);

  // Synchronously scans an already opened smaps file.
  // TODO(b/494004530): Keep this public static as it will be used by OSMetrics
  // in a subsequent change (re-evaluate making it file-static once OSMetrics
  // integration is complete/decoupled).
  static std::optional<ParsedSmapsResults> ScanSmaps(base::File file);

 private:
  void OnScanComplete(std::optional<ParsedSmapsResults> results);

  bool isScanning() const { return !pending_callbacks_.empty(); }

  base::raw_ptr<DetailedMetricsDelegate> delegate_;
  std::vector<base::OnceClosure> pending_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SmapsCategorizer> weak_ptr_factory_{this};
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_H_

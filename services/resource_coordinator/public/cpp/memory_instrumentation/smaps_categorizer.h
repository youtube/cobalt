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

#include <vector>

#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/detailed_metrics_delegate.h"

namespace memory_instrumentation {

// Manages the lifecycle of /proc/self/smaps parsing.
// Use a sequence-affine front-end to handle request coalescing and a background
// ThreadPool task for I/O and parsing.
class COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION)
    SmapsCategorizer {
 public:
  explicit SmapsCategorizer(base::WeakPtr<DetailedMetricsDelegate> delegate);
  ~SmapsCategorizer();

  SmapsCategorizer(const SmapsCategorizer&) = delete;
  SmapsCategorizer& operator=(const SmapsCategorizer&) = delete;

  // Requests a memory dump. If a scan is already in progress, the callback
  // will be queued and called when the current scan completes.
  // Must be called on the same sequence the categorizer was created on.
  void RequestDump(base::OnceClosure callback);

  // Synchronously scans a smaps file.
  static bool ScanSmapsFile(const base::FilePath& path,
                            DetailedMetricsDelegate* delegate);

  // Synchronously scans an already opened smaps file.
  static bool ScanSmaps(base::File file,
                        DetailedMetricsDelegate* delegate);

 private:
  void OnScanComplete(bool success);

  // Background thread task.
  static bool PerformScanOnBackgroundThread(
      DetailedMetricsDelegate* delegate);

  base::WeakPtr<DetailedMetricsDelegate> delegate_;
  bool is_scanning_ = false;
  std::vector<base::OnceClosure> pending_callbacks_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SmapsCategorizer> weak_ptr_factory_{this};
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_H_

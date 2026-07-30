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

#ifndef COBALT_BROWSER_METRICS_COBALT_STABILITY_METRICS_HELPER_H_
#define COBALT_BROWSER_METRICS_COBALT_STABILITY_METRICS_HELPER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/process/process_handle.h"

namespace cobalt {

// Returns the total size in bytes of all .pma files in |metrics_dir|.
int64_t GetTotalStabilityMetricsPmaDirSizeBytes(
    const base::FilePath& metrics_dir);

// Ensures that the total size of .pma files in |metrics_dir| plus
// |bytes_to_add| does not exceed |max_total_bytes|. If the budget is exceeded,
// prunes the oldest .pma files (and any invalid .pma files) until the budget is
// satisfied. Returns true if the budget is satisfied, or false if it cannot be
// met.
bool EnsurePmaDirectoryBudget(const base::FilePath& metrics_dir,
                              const std::string& expected_allocator_name,
                              int64_t max_total_bytes,
                              int64_t bytes_to_add);

// Extracts process IDs of prior sessions from persistent memory allocator
// (.pma) files located in |metrics_dir| matching |expected_allocator_name|.
// Ignores non-.pma files, files where ParseFilePath fails, files with
// mismatched allocator names, PIDs <= 0, and |current_pid|. Returned PIDs are
// deduplicated.
std::vector<base::ProcessId> ExtractPriorSessionPids(
    const base::FilePath& metrics_dir,
    const std::string& expected_allocator_name,
    base::ProcessId current_pid);

// Reads all persistent memory allocator (.pma) files in |metrics_dir|
// matching |expected_allocator_name| (excluding |current_pid|) into memory by
// merging their histogram deltas into the global StatisticsRecorder, and then
// removes them from disk. Also removes any corrupt, invalid, or mismatched
// .pma files, while preserving active files matching |current_pid|.
void ClearOtherStabilityMetricsPmaFiles(
    const base::FilePath& metrics_dir,
    const std::string& expected_allocator_name,
    base::ProcessId current_pid = base::kNullProcessId);

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_STABILITY_METRICS_HELPER_H_

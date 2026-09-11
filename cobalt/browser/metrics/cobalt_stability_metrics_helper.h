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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/process/process_handle.h"

namespace cobalt {

// Constructs the stability metrics file path for |name| and |pid| in |dir|:
// "<dir>/<name>-<pid_hex>.pma".
base::FilePath ConstructStabilityMetricsFilePath(const base::FilePath& dir,
                                                 std::string_view name,
                                                 base::ProcessId pid);

// Extracts the process ID from a stability metrics PMA file path.
// Supports both "<expected_allocator_name>-<pid_hex>.pma" and
// "<expected_allocator_name>-<stamp_hex>-<pid_hex>.pma".
// Returns std::nullopt if the file does not match or PID <= 0.
std::optional<base::ProcessId> ExtractStabilityMetricsPid(
    const base::FilePath& path,
    std::string_view expected_allocator_name = "");

// Clears all persistent memory allocator (.pma) files in |metrics_dir|
// matching |expected_allocator_name|, except for the single prior session's
// file. Also removes any corrupt or unrecognized .pma files, and any files
// matching |current_pid|. Ensures at most one single prior PMA file exists in
// the directory prior to creating the new session's PMA file.
// Returns the process ID of the retained prior session's .pma file, or
// std::nullopt if none found.
std::optional<base::ProcessId> ClearOtherStabilityMetricsPmaFiles(
    const base::FilePath& metrics_dir,
    std::string_view expected_allocator_name,
    base::ProcessId current_pid = base::kNullProcessId);

// Extracts the process ID of the prior session from persistent memory allocator
// (.pma) files located in |metrics_dir| matching |expected_allocator_name|.
// Ignores non-.pma files, corrupt files, files with mismatched allocator names,
// PIDs <= 0, and |current_pid|.
// Returns std::nullopt if no valid prior session file is found.
std::optional<base::ProcessId> ExtractPriorSessionPid(
    const base::FilePath& metrics_dir,
    std::string_view expected_allocator_name,
    base::ProcessId current_pid = base::kNullProcessId);

// Compatibility helper: returns the prior session PID in a single-element
// vector, or an empty vector if no valid prior session exists.
std::vector<base::ProcessId> ExtractPriorSessionPids(
    const base::FilePath& metrics_dir,
    std::string_view expected_allocator_name,
    base::ProcessId current_pid = base::kNullProcessId);

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_STABILITY_METRICS_HELPER_H_

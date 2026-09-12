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

#include "cobalt/browser/metrics/cobalt_stability_metrics_helper.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace cobalt {

base::FilePath ConstructStabilityMetricsFilePath(const base::FilePath& dir,
                                                 const std::string& name,
                                                 base::ProcessId pid) {
  return dir.AppendASCII(
      base::StringPrintf("%s-%lX.pma", name.c_str(), static_cast<long>(pid)));
}

std::optional<base::ProcessId> ExtractStabilityMetricsPid(
    const base::FilePath& path,
    const std::string& expected_allocator_name) {
  if (path.Extension() != FILE_PATH_LITERAL(".pma")) {
    return std::nullopt;
  }

  std::string filename = path.BaseName().AsUTF8Unsafe();
  std::vector<std::string_view> parts = base::SplitStringPiece(
      filename, "-.", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // Format 1: "<name>-<pid>.pma" -> 3 parts: [name, pid_hex, "pma"]
  // Format 2: "<name>-<stamp>-<pid>.pma" -> 4 parts: [name, stamp_hex, pid_hex,
  // "pma"]
  if (parts.size() != 3 && parts.size() != 4) {
    return std::nullopt;
  }

  if (parts.back() != "pma") {
    return std::nullopt;
  }

  if (!expected_allocator_name.empty() && parts[0] != expected_allocator_name) {
    return std::nullopt;
  }

  int64_t pid_val = 0;
  std::string_view pid_str = (parts.size() == 3) ? parts[1] : parts[2];
  if (!base::HexStringToInt64(pid_str, &pid_val) || pid_val <= 0) {
    return std::nullopt;
  }

  return static_cast<base::ProcessId>(pid_val);
}

void ClearOtherStabilityMetricsPmaFiles(
    const base::FilePath& metrics_dir,
    const std::string& expected_allocator_name,
    base::ProcessId current_pid) {
  base::FilePath latest_file;
  base::Time latest_stamp;

  // First pass: identify the single valid prior session file.
  // If multiple files exist, select the most recently modified.
  base::FileEnumerator file_iter(metrics_dir, /*recursive=*/false,
                                 base::FileEnumerator::FILES);
  for (base::FilePath file = file_iter.Next(); !file.empty();
       file = file_iter.Next()) {
    std::optional<base::ProcessId> pid =
        ExtractStabilityMetricsPid(file, expected_allocator_name);
    if (!pid.has_value()) {
      continue;
    }

    if (current_pid != base::kNullProcessId && *pid == current_pid) {
      continue;
    }

    base::Time stamp = file_iter.GetInfo().GetLastModifiedTime();
    if (latest_file.empty() || stamp > latest_stamp) {
      latest_stamp = stamp;
      latest_file = file;
    }
  }

  // Second pass: remove all other .pma files to ensure only one single PMA
  // exists at a time prior to writing the new PMA file.
  base::FileEnumerator delete_iter(metrics_dir, /*recursive=*/false,
                                   base::FileEnumerator::FILES);
  for (base::FilePath file = delete_iter.Next(); !file.empty();
       file = delete_iter.Next()) {
    if (file.Extension() != FILE_PATH_LITERAL(".pma")) {
      continue;
    }

    if (!latest_file.empty() && file == latest_file) {
      continue;
    }

    base::DeleteFile(file);
  }
}

std::optional<base::ProcessId> ExtractPriorSessionPid(
    const base::FilePath& metrics_dir,
    const std::string& expected_allocator_name,
    base::ProcessId current_pid,
    base::FilePath* out_file_path) {
  base::ProcessId latest_pid = 0;
  base::FilePath latest_file;
  base::Time latest_stamp;

  base::FileEnumerator file_iter(metrics_dir, /*recursive=*/false,
                                 base::FileEnumerator::FILES);
  for (base::FilePath file = file_iter.Next(); !file.empty();
       file = file_iter.Next()) {
    std::optional<base::ProcessId> pid =
        ExtractStabilityMetricsPid(file, expected_allocator_name);
    if (!pid.has_value()) {
      continue;
    }

    if (*pid == current_pid) {
      continue;
    }

    base::Time stamp = file_iter.GetInfo().GetLastModifiedTime();
    if (latest_pid == 0 || stamp > latest_stamp) {
      latest_stamp = stamp;
      latest_pid = *pid;
      latest_file = file;
    }
  }

  if (latest_pid <= 0) {
    return std::nullopt;
  }
  if (out_file_path) {
    *out_file_path = latest_file;
  }
  return latest_pid;
}

std::vector<base::ProcessId> ExtractPriorSessionPids(
    const base::FilePath& metrics_dir,
    const std::string& expected_allocator_name,
    base::ProcessId current_pid) {
  std::optional<base::ProcessId> pid =
      ExtractPriorSessionPid(metrics_dir, expected_allocator_name, current_pid);
  if (!pid.has_value()) {
    return {};
  }
  return {*pid};
}

}  // namespace cobalt

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
#include "base/numerics/safe_conversions.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace cobalt {

base::FilePath ConstructStabilityMetricsFilePath(const base::FilePath& dir,
                                                 std::string_view name,
                                                 base::ProcessId pid) {
  return dir.AppendASCII(
      base::StringPrintf("%.*s-%lX.pma", static_cast<int>(name.size()),
                         name.data(), static_cast<unsigned long>(pid)));
}

std::optional<base::ProcessId> ExtractStabilityMetricsPid(
    const base::FilePath& path,
    std::string_view expected_allocator_name) {
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

  if (parts.size() == 4) {
    int64_t stamp_val = 0;
    if (!base::HexStringToInt64(parts[1], &stamp_val) || stamp_val < 0) {
      return std::nullopt;
    }
  }

  int64_t pid_val = 0;
  std::string_view pid_str = (parts.size() == 3) ? parts[1] : parts[2];
  if (!base::HexStringToInt64(pid_str, &pid_val) ||
      !base::IsValueInRangeForNumericType<base::ProcessId>(pid_val) ||
      static_cast<base::ProcessId>(pid_val) <= 0) {
    return std::nullopt;
  }

  return static_cast<base::ProcessId>(pid_val);
}

std::optional<base::ProcessId> ClearOtherStabilityMetricsPmaFiles(
    const base::FilePath& metrics_dir,
    std::string_view expected_allocator_name,
    base::ProcessId current_pid) {
  std::optional<base::ProcessId> latest_pid;
  base::FilePath latest_file;
  base::Time latest_stamp;
  std::vector<base::FilePath> files_to_delete;

  base::FileEnumerator file_iter(metrics_dir, /*recursive=*/false,
                                 base::FileEnumerator::FILES);
  for (base::FilePath file = file_iter.Next(); !file.empty();
       file = file_iter.Next()) {
    if (file.Extension() != FILE_PATH_LITERAL(".pma")) {
      continue;
    }

    std::optional<base::ProcessId> pid =
        ExtractStabilityMetricsPid(file, expected_allocator_name);
    if (!pid.has_value() ||
        (current_pid != base::kNullProcessId && *pid == current_pid)) {
      files_to_delete.push_back(file);
      continue;
    }

    base::Time stamp = file_iter.GetInfo().GetLastModifiedTime();
    if (!latest_pid.has_value() || stamp > latest_stamp) {
      if (!latest_file.empty()) {
        files_to_delete.push_back(latest_file);
      }
      latest_stamp = stamp;
      latest_file = file;
      latest_pid = *pid;
    } else {
      files_to_delete.push_back(file);
    }
  }

  for (const base::FilePath& file : files_to_delete) {
    base::DeleteFile(file);
  }

  return latest_pid;
}

std::optional<base::ProcessId> ExtractPriorSessionPid(
    const base::FilePath& metrics_dir,
    std::string_view expected_allocator_name,
    base::ProcessId current_pid) {
  std::optional<base::ProcessId> latest_pid;
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

    if (current_pid != base::kNullProcessId && *pid == current_pid) {
      continue;
    }

    base::Time stamp = file_iter.GetInfo().GetLastModifiedTime();
    if (!latest_pid.has_value() || stamp > latest_stamp) {
      latest_stamp = stamp;
      latest_pid = *pid;
    }
  }

  return latest_pid;
}

std::vector<base::ProcessId> ExtractPriorSessionPids(
    const base::FilePath& metrics_dir,
    std::string_view expected_allocator_name,
    base::ProcessId current_pid) {
  std::optional<base::ProcessId> pid =
      ExtractPriorSessionPid(metrics_dir, expected_allocator_name, current_pid);
  if (!pid.has_value()) {
    return {};
  }
  return {*pid};
}

}  // namespace cobalt

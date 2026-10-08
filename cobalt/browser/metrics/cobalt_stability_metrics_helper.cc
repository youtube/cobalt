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

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/process/process_handle.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"

namespace cobalt {

namespace {

struct PmaFileInfo {
  base::FilePath path;
  base::Time timestamp;
  int64_t size;
};

std::string_view ExitReasonToHistogramSuffix(int reason) {
  // Matches dev.cobalt.util.ProcessExitReasonHelper.ExitReason.
  switch (reason) {
    case 0:  // REASON_ANR
      return "Anr";
    case 1:  // REASON_CRASH
      return "Crash";
    case 2:  // REASON_CRASH_NATIVE
      return "CrashNative";
    case 3:  // REASON_DEPENDENCY_DIED
      return "DependencyDied";
    case 4:  // REASON_EXCESSIVE_RESOURCE_USAGE
      return "ExcessiveResourceUsage";
    case 5:  // REASON_EXIT_SELF
      return "ExitSelf";
    case 6:  // REASON_INITIALIZATION_FAILURE
      return "InitFailure";
    case 7:  // REASON_LOW_MEMORY
      return "LowMemory";
    case 8:  // REASON_OTHER
      return "Other";
    case 9:  // REASON_PERMISSION_CHANGE
      return "PermissionChange";
    case 10:  // REASON_SIGNALED
      return "Signaled";
    case 12:  // REASON_USER_REQUESTED
      return "UserRequested";
    case 13:  // REASON_USER_STOPPED
      return "UserStopped";
    case 15:  // REASON_FREEZER
      return "Freezer";
    case 16:  // REASON_PACKAGE_STATE_CHANGE
      return "PackageStateChange";
    case 17:  // REASON_PACKAGE_UPDATED
      return "PackageUpdated";
    default:
      return "Other";
  }
}

}  // namespace

int64_t GetTotalStabilityMetricsPmaDirSizeBytes(
    const base::FilePath& metrics_dir) {
  int64_t total_size = 0;
  base::FileEnumerator file_iter(metrics_dir, /*recursive=*/false,
                                 base::FileEnumerator::FILES);
  for (base::FilePath file = file_iter.Next(); !file.empty();
       file = file_iter.Next()) {
    if (file.Extension() == FILE_PATH_LITERAL(".pma")) {
      total_size += file_iter.GetInfo().GetSize();
    }
  }
  return total_size;
}

bool EnsurePmaDirectoryBudget(const base::FilePath& metrics_dir,
                              const std::string& expected_allocator_name,
                              int64_t max_total_bytes,
                              int64_t bytes_to_add) {
  if (bytes_to_add > max_total_bytes) {
    return false;
  }

  std::vector<PmaFileInfo> pma_files;
  int64_t total_size = 0;

  base::FileEnumerator file_iter(metrics_dir, /*recursive=*/false,
                                 base::FileEnumerator::FILES);
  for (base::FilePath file = file_iter.Next(); !file.empty();
       file = file_iter.Next()) {
    if (file.Extension() != FILE_PATH_LITERAL(".pma")) {
      continue;
    }

    int64_t file_size = file_iter.GetInfo().GetSize();

    std::string name;
    base::Time stamp;
    base::ProcessId file_pid;
    bool valid = base::GlobalHistogramAllocator::ParseFilePath(
        file, &name, &stamp, &file_pid);
    if (!valid || name != expected_allocator_name || file_pid <= 0) {
      base::DeleteFile(file);
      continue;
    }

    pma_files.push_back({file, stamp, file_size});
    total_size += file_size;
  }

  if (total_size + bytes_to_add <= max_total_bytes) {
    return true;
  }

  std::sort(pma_files.begin(), pma_files.end(),
            [](const PmaFileInfo& a, const PmaFileInfo& b) {
              return a.timestamp < b.timestamp;
            });

  for (const auto& file_info : pma_files) {
    if (total_size + bytes_to_add <= max_total_bytes) {
      break;
    }
    if (base::DeleteFile(file_info.path)) {
      total_size -= file_info.size;
    }
  }

  return total_size + bytes_to_add <= max_total_bytes;
}

void ClearOtherStabilityMetricsPmaFiles(
    const base::FilePath& metrics_dir,
    const std::string& expected_allocator_name,
    base::ProcessId current_pid) {
  int64_t total_pma_bytes =
      GetTotalStabilityMetricsPmaDirSizeBytes(metrics_dir);
  base::UmaHistogramMemoryKB("Cobalt.Stability.Pma.StartupTotalSizeKB",
                             static_cast<int>(total_pma_bytes / 1024));

  base::FileEnumerator file_iter(metrics_dir, /*recursive=*/false,
                                 base::FileEnumerator::FILES);
  for (base::FilePath file = file_iter.Next(); !file.empty();
       file = file_iter.Next()) {
    if (file.Extension() != FILE_PATH_LITERAL(".pma")) {
      continue;
    }

    std::string name;
    base::Time stamp;
    base::ProcessId file_pid;
    bool valid = base::GlobalHistogramAllocator::ParseFilePath(
        file, &name, &stamp, &file_pid);
    if (!valid || name != expected_allocator_name || file_pid <= 0) {
      base::DeleteFile(file);
      continue;
    }

    if (current_pid != base::kNullProcessId && file_pid == current_pid) {
      continue;
    }

    // Read prior session PMA file into memory and merge to StatisticsRecorder.
    {
      auto mapped = std::make_unique<base::MemoryMappedFile>();
      if (mapped->Initialize(file, base::MemoryMappedFile::READ_ONLY) &&
          base::FilePersistentMemoryAllocator::IsFileAcceptable(
              *mapped, /*read_only=*/true)) {
        auto memory_allocator =
            std::make_unique<base::FilePersistentMemoryAllocator>(
                std::move(mapped), 0, 0, std::string_view(),
                base::FilePersistentMemoryAllocator::kReadOnly);
        if (memory_allocator->GetMemoryState() !=
                base::PersistentMemoryAllocator::MEMORY_DELETED &&
            !memory_allocator->IsCorrupt()) {
          base::PersistentHistogramAllocator allocator(
              std::move(memory_allocator));
          base::PersistentHistogramAllocator::Iterator histogram_iter(
              &allocator);
          while (auto histogram = histogram_iter.GetNext()) {
            allocator.MergeHistogramFinalDeltaToStatisticsRecorder(
                histogram.get());
          }
        }
      }
    }
    base::DeleteFile(file);
  }
}

void EmitPriorSessionExitSummaryHistograms(
    int exit_reason,
    const ProcessStateSummaryData& summary) {
  if (summary.startup_guard_triggered_kill) {
    base::UmaHistogramExactLinear(
        "Cobalt.Stability.Android.StartupGuardWatchdogKilled.HighestMilestone",
        summary.highest_milestone, 64);
    return;
  }

  std::string_view suffix = ExitReasonToHistogramSuffix(exit_reason);

  // 1. Emit per-exit-reason histograms
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Cobalt.Stability.Android.PriorSessionExit.StartupGuardArmed.",
           suffix}),
      summary.startup_guard_armed);
  base::UmaHistogramExactLinear(
      base::StrCat(
          {"Cobalt.Stability.Android.PriorSessionExit.HighestMilestone.",
           suffix}),
      summary.highest_milestone, 64);

  // 2. Emit baseline aggregate across all exits
  base::UmaHistogramBoolean(
      "Cobalt.Stability.Android.PriorSessionExit.StartupGuardArmed.AllExits",
      summary.startup_guard_armed);
  base::UmaHistogramExactLinear(
      "Cobalt.Stability.Android.PriorSessionExit.HighestMilestone.AllExits",
      summary.highest_milestone, 64);
}

}  // namespace cobalt

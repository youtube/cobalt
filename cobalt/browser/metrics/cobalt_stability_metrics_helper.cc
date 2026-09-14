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
#include <set>
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
#include "cobalt/browser/metrics/cobalt_process_state_summary_manager.h"

namespace cobalt {

namespace {

struct PmaFileInfo {
  base::FilePath path;
  base::Time timestamp;
  int64_t size;
};

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
    const ProcessStateSnapshot& snapshot) {
  // If the process was intentionally terminated by Cobalt's StartupGuard
  // watchdog, attribute it to StartupGuardWatchdogKilled metrics and do not
  // emit organic OS exit histograms.
  if (snapshot.was_killed_by_startup_guard()) {
    base::UmaHistogramExactLinear(
        "Cobalt.Stability.Android.StartupGuardWatchdogKilled.HighestMilestone",
        snapshot.highest_milestone, 64);
    base::UmaHistogramMemoryLargeMB(
        "Cobalt.Stability.Android.StartupGuardWatchdogKilled.PeakV8CodeMB",
        snapshot.peak_v8_code_kb / 1024);
    base::UmaHistogramMemoryLargeMB(
        "Cobalt.Stability.Android.StartupGuardWatchdogKilled.PeakRssMB",
        snapshot.peak_rss_kb / 1024);
    base::UmaHistogramMemoryLargeMB(
        "Cobalt.Stability.Android.StartupGuardWatchdogKilled.PeakPmfMB",
        snapshot.peak_pmf_kb / 1024);
    base::UmaHistogramCustomCounts(
        "Cobalt.Stability.Android.StartupGuardWatchdogKilled.UptimeSeconds",
        std::max<int>(1, snapshot.uptime_sec), 1, 3600, 50);
    return;
  }

  std::string suffix(ExitReasonToHistogramSuffix(exit_reason));

  // 1. Emit pre-joined histograms for the specific exit reason
  base::UmaHistogramMemoryLargeMB(
      base::StrCat({"Cobalt.Stability.Android.PeakRssMB.", suffix}),
      snapshot.peak_rss_kb / 1024);
  base::UmaHistogramMemoryLargeMB(
      base::StrCat({"Cobalt.Stability.Android.PeakPmfMB.", suffix}),
      snapshot.peak_pmf_kb / 1024);
  base::UmaHistogramMemoryLargeMB(
      base::StrCat({"Cobalt.Stability.Android.PeakV8CodeMB.", suffix}),
      snapshot.peak_v8_code_kb / 1024);
  base::UmaHistogramCustomCounts(
      base::StrCat({"Cobalt.Stability.Android.UptimeMinutes.", suffix}),
      std::max<int>(1, snapshot.uptime_minutes()), 1, 2880, 50);
  base::UmaHistogramExactLinear(
      base::StrCat({"Cobalt.Stability.Android.LastTrimLevel.", suffix}),
      snapshot.last_trim_level, 100);

  // StartupGuard milestone and phase attribution:
  bool is_early_startup = snapshot.is_startup_guard_armed();
  base::UmaHistogramBoolean(
      base::StrCat({"Cobalt.Stability.Android.StartupGuardArmed.", suffix}),
      is_early_startup);
  base::UmaHistogramExactLinear(
      base::StrCat({"Cobalt.Stability.Android.HighestMilestone.", suffix}),
      snapshot.highest_milestone, 64);

  if (is_early_startup) {
    base::UmaHistogramMemoryLargeMB(
        base::StrCat(
            {"Cobalt.Stability.Android.PeakRssMB.EarlyStartup.", suffix}),
        snapshot.peak_rss_kb / 1024);
    base::UmaHistogramMemoryLargeMB(
        base::StrCat(
            {"Cobalt.Stability.Android.PeakPmfMB.EarlyStartup.", suffix}),
        snapshot.peak_pmf_kb / 1024);
    base::UmaHistogramMemoryLargeMB(
        base::StrCat(
            {"Cobalt.Stability.Android.PeakV8CodeMB.EarlyStartup.", suffix}),
        snapshot.peak_v8_code_kb / 1024);
    base::UmaHistogramCustomCounts(
        base::StrCat(
            {"Cobalt.Stability.Android.UptimeSeconds.EarlyStartup.", suffix}),
        std::max<int>(1, snapshot.uptime_sec), 1, 3600, 50);
  }

  // 2. Emit baseline aggregate across all exits
  base::UmaHistogramMemoryLargeMB("Cobalt.Stability.Android.PeakRssMB.AllExits",
                                  snapshot.peak_rss_kb / 1024);
  base::UmaHistogramMemoryLargeMB("Cobalt.Stability.Android.PeakPmfMB.AllExits",
                                  snapshot.peak_pmf_kb / 1024);
  base::UmaHistogramMemoryLargeMB(
      "Cobalt.Stability.Android.PeakV8CodeMB.AllExits",
      snapshot.peak_v8_code_kb / 1024);
  base::UmaHistogramCustomCounts(
      "Cobalt.Stability.Android.UptimeMinutes.AllExits",
      std::max<int>(1, snapshot.uptime_minutes()), 1, 2880, 50);
  base::UmaHistogramExactLinear(
      "Cobalt.Stability.Android.LastTrimLevel.AllExits",
      snapshot.last_trim_level, 100);
  base::UmaHistogramBoolean(
      "Cobalt.Stability.Android.StartupGuardArmed.AllExits", is_early_startup);
  base::UmaHistogramExactLinear(
      "Cobalt.Stability.Android.HighestMilestone.AllExits",
      snapshot.highest_milestone, 64);
  if (is_early_startup) {
    base::UmaHistogramMemoryLargeMB(
        "Cobalt.Stability.Android.PeakRssMB.EarlyStartup.AllExits",
        snapshot.peak_rss_kb / 1024);
    base::UmaHistogramMemoryLargeMB(
        "Cobalt.Stability.Android.PeakPmfMB.EarlyStartup.AllExits",
        snapshot.peak_pmf_kb / 1024);
    base::UmaHistogramMemoryLargeMB(
        "Cobalt.Stability.Android.PeakV8CodeMB.EarlyStartup.AllExits",
        snapshot.peak_v8_code_kb / 1024);
    base::UmaHistogramCustomCounts(
        "Cobalt.Stability.Android.UptimeSeconds.EarlyStartup.AllExits",
        std::max<int>(1, snapshot.uptime_sec), 1, 3600, 50);
  }
}
std::vector<base::ProcessId> ExtractPriorSessionPids(
    const base::FilePath& metrics_dir,
    const std::string& expected_allocator_name,
    base::ProcessId current_pid) {
  std::vector<base::ProcessId> pids;
  std::set<base::ProcessId> seen_pids;

  base::FileEnumerator file_iter(metrics_dir, /*recursive=*/false,
                                 base::FileEnumerator::FILES);
  for (base::FilePath file = file_iter.Next(); !file.empty();
       file = file_iter.Next()) {
    if (file.Extension() != FILE_PATH_LITERAL(".pma")) {
      continue;
    }

    std::string name;
    base::Time stamp;
    base::ProcessId previous_pid;
    if (!base::GlobalHistogramAllocator::ParseFilePath(file, &name, &stamp,
                                                       &previous_pid)) {
      continue;
    }

    if (name != expected_allocator_name) {
      continue;
    }

    if (previous_pid <= 0 || previous_pid == current_pid) {
      continue;
    }

    if (seen_pids.insert(previous_pid).second) {
      pids.push_back(previous_pid);
    }
  }

  return pids;
}

}  // namespace cobalt

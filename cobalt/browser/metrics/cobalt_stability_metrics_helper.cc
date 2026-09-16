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

void ClearOtherStabilityMetricsPmaFiles(
    const base::FilePath& metrics_dir,
    const std::string& expected_allocator_name,
    base::ProcessId current_pid) {
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
    if (!valid || name != expected_allocator_name || file_pid == current_pid ||
        file_pid <= 0) {
      base::DeleteFile(file);
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

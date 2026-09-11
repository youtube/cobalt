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

#include <set>
#include <string>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/process/process_handle.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_ANDROID)
#include <optional>

#include "base/android/build_info.h"
#include "base/base_paths.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "cobalt/browser/metrics/cobalt_process_state_summary_manager.h"
#include "components/crash/content/browser/process_exit_reason_from_system_android.h"
#endif

namespace cobalt {

namespace {

#if BUILDFLAG(IS_ANDROID)
base::Lock& GetLock() {
  static base::NoDestructor<base::Lock> s_lock;
  return *s_lock;
}

// Guarded by GetLock(). Indicates whether prior session exit reasons have been
// recorded to UMA.
bool g_exit_reasons_recorded = false;

std::vector<base::OnceCallback<void(bool)>>* GetPendingCallbacks() {
  static base::NoDestructor<std::vector<base::OnceCallback<void(bool)>>>
      s_pending_callbacks;
  return s_pending_callbacks.get();
}

base::OnceCallback<void(bool)> WrapCallbackForCaller(
    base::OnceCallback<void(bool)> callback) {
  if (base::SequencedTaskRunner::HasCurrentDefault()) {
    return base::BindPostTaskToCurrentDefault(std::move(callback));
  }
  return callback;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

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
      snapshot.uptime_minutes(), 1, 2880, 50);
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
      snapshot.uptime_minutes(), 1, 2880, 50);
}
#if BUILDFLAG(IS_ANDROID)
void RecordPriorSessionExitReasons() {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    return;
  }
  base::FilePath base_dir;
  if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &base_dir)) {
    return;
  }
  base::FilePath metrics_dir =
      base_dir.AppendASCII(kBrowserStabilityMetricsName);
  std::vector<base::ProcessId> prior_pids = ExtractPriorSessionPids(
      metrics_dir, kBrowserStabilityMetricsName, base::GetCurrentProcId());

  for (base::ProcessId pid : prior_pids) {
    // 1. Record original SystemExitReason histogram
    crash_reporter::ProcessExitReasonFromSystem::RecordExitReasonToUma(
        pid, kSystemExitReasonHistogram);

    // 2. Read prior session state summary snapshot
    std::optional<ProcessStateSnapshot> snapshot =
        CobaltProcessStateSummaryManager::GetInstance()
            ->ReadPriorSessionSnapshot(pid);

    if (!snapshot.has_value()) {
      continue;
    }

    // 3. Resolve exit reason to determine histogram suffix
    int exit_reason =
        crash_reporter::ProcessExitReasonFromSystem::GetExitReason(pid);

    // 4. Emit pre-joined memory and stability metrics
    EmitPriorSessionExitSummaryHistograms(exit_reason, *snapshot);
  }
}

bool WasPriorSessionLowMemoryKilled() {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(kSystemExitReasonHistogram);
  if (!histogram) {
    return false;
  }
  auto samples = histogram->SnapshotSamples();
  if (!samples) {
    return false;
  }
  return samples->GetCount(kAndroidExitReasonLowMemory) > 0;
}

void OnPriorSessionExitReasonsRecorded() {
  std::vector<base::OnceCallback<void(bool)>> callbacks;
  {
    base::AutoLock lock(GetLock());
    g_exit_reasons_recorded = true;
    callbacks.swap(*GetPendingCallbacks());
  }
  // Compute WasPriorSessionLowMemoryKilled() on the background thread.
  bool was_lmk = WasPriorSessionLowMemoryKilled();
  for (auto& cb : callbacks) {
    std::move(cb).Run(was_lmk);
  }
}

void GetWasPriorSessionLowMemoryKilledAsync(
    base::OnceCallback<void(bool)> callback) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    if (base::SequencedTaskRunner::HasCurrentDefault()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), false));
    } else {
      std::move(callback).Run(false);
    }
    return;
  }
  {
    base::AutoLock lock(GetLock());
    if (!g_exit_reasons_recorded) {
      GetPendingCallbacks()->push_back(
          WrapCallbackForCaller(std::move(callback)));
      return;
    }
  }
  // Exit reasons already recorded: resolve on a background thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&WasPriorSessionLowMemoryKilled), std::move(callback));
}

void ResetPriorSessionExitReasonsForTesting() {
  base::AutoLock lock(GetLock());
  g_exit_reasons_recorded = false;
  GetPendingCallbacks()->clear();
}
#endif  // BUILDFLAG(IS_ANDROID)

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

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/system_memory_pressure_evaluator_win.h"

#include <windows.h>

#include <psapi.h>

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/win/object_watcher.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"

namespace memory_pressure {
namespace win {

namespace {

// Whether to use available memory commit instead of available physical
// memory for Windows memory pressure detection.
BASE_FEATURE(kCommitAvailableMemoryPressure,
             "UseAvailableMemoryThresholds",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, allows setting custom thresholds for commit-based
// memory pressure detection via the |kCommitAvailableCriticalThresholdMB|
// and |kCommitAvailableModerateThresholdMB| parameters.
BASE_FEATURE(kCommitAvailableMemoryPressureThresholds,
             "CommitAvailableMemoryPressureThresholds",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default thresholds for commit-based memory pressure detection.
const int kDefaultCommitAvailableCriticalThresholdMb = 200;
const int kDefaultCommitAvailableModerateThresholdMb = 500;

// The amount of commit available (in MB) below which the system is considered
// to be under critical memory pressure. The default value is equal to
// kSmallMemoryDefaultCriticalThresholdMb (200).
BASE_FEATURE_PARAM(int,
                   kCommitAvailableCriticalThresholdMB,
                   &kCommitAvailableMemoryPressureThresholds,
                   "CommitAvailableCriticalThresholdMB",
                   kDefaultCommitAvailableCriticalThresholdMb);

// The amount of commit available (in MB) below which the system is considered
// to be under moderate memory pressure. The default value is equal to
// kSmallMemoryDefaultModerateThresholdMb (500).
BASE_FEATURE_PARAM(int,
                   kCommitAvailableModerateThresholdMB,
                   &kCommitAvailableMemoryPressureThresholds,
                   "CommitAvailableModerateThresholdMB",
                   kDefaultCommitAvailableModerateThresholdMb);

static const DWORDLONG kMBBytes = 1024 * 1024;

// Implements ObjectWatcher::Delegate by forwarding to a provided callback.
class MemoryPressureWatcherDelegate
    : public base::win::ObjectWatcher::Delegate {
 public:
  MemoryPressureWatcherDelegate(base::win::ScopedHandle handle,
                                base::OnceClosure callback);
  ~MemoryPressureWatcherDelegate() override;
  MemoryPressureWatcherDelegate(const MemoryPressureWatcherDelegate& other) =
      delete;
  MemoryPressureWatcherDelegate& operator=(
      const MemoryPressureWatcherDelegate&) = delete;

  void ReplaceWatchedHandleForTesting(base::win::ScopedHandle handle);
  void SetCallbackForTesting(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

 private:
  void OnObjectSignaled(HANDLE handle) override;

  base::win::ScopedHandle handle_;
  base::win::ObjectWatcher watcher_;
  base::OnceClosure callback_;
};

MemoryPressureWatcherDelegate::MemoryPressureWatcherDelegate(
    base::win::ScopedHandle handle,
    base::OnceClosure callback)
    : handle_(std::move(handle)), callback_(std::move(callback)) {
  DCHECK(handle_.IsValid());
  CHECK(watcher_.StartWatchingOnce(handle_.Get(), this));
}

MemoryPressureWatcherDelegate::~MemoryPressureWatcherDelegate() = default;

void MemoryPressureWatcherDelegate::ReplaceWatchedHandleForTesting(
    base::win::ScopedHandle handle) {
  if (watcher_.IsWatching()) {
    watcher_.StopWatching();
  }
  handle_ = std::move(handle);
  CHECK(watcher_.StartWatchingOnce(handle_.Get(), this));
}

void MemoryPressureWatcherDelegate::OnObjectSignaled(HANDLE handle) {
  DCHECK_EQ(handle, handle_.Get());
  std::move(callback_).Run();
}

}  // namespace

// Check the amount of RAM left every 5 seconds.
const base::TimeDelta SystemMemoryPressureEvaluator::kMemorySamplingPeriod =
    base::Seconds(5);

// The following constants have been lifted from similar values in the ChromeOS
// memory pressure monitor. The values were determined experimentally to ensure
// sufficient responsiveness of the memory pressure subsystem, and minimal
// overhead.
const base::TimeDelta SystemMemoryPressureEvaluator::kModeratePressureCooldown =
    base::Seconds(10);

// TODO(chrisha): Explore the following constants further with an experiment.

// A system is considered 'high memory' if it has more than 1.5GB of system
// memory available for use by the memory manager (not reserved for hardware
// and drivers). This is a fuzzy version of the ~2GB discussed below.
const int SystemMemoryPressureEvaluator::kLargeMemoryThresholdMb = 1536;

// These are the default thresholds used for systems with < ~2GB of physical
// memory. Such systems have been observed to always maintain ~100MB of
// available memory, paging until that is the case. To try to avoid paging a
// threshold slightly above this is chosen. The moderate threshold is slightly
// less grounded in reality and chosen as 2.5x critical.
const int
    SystemMemoryPressureEvaluator::kSmallMemoryDefaultModerateThresholdMb = 500;
const int
    SystemMemoryPressureEvaluator::kSmallMemoryDefaultCriticalThresholdMb = 200;

// These are the default thresholds used for systems with >= ~2GB of physical
// memory. Such systems have been observed to always maintain ~300MB of
// available memory, paging until that is the case.
const int
    SystemMemoryPressureEvaluator::kLargeMemoryDefaultModerateThresholdMb =
        1000;
const int
    SystemMemoryPressureEvaluator::kLargeMemoryDefaultCriticalThresholdMb = 400;

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    std::unique_ptr<MemoryPressureVoter> voter)
    : memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
      moderate_threshold_mb_(0),
      critical_threshold_mb_(0),
      moderate_pressure_repeat_count_(0) {
  InferThresholds();
  StartObserving();
}

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    int moderate_threshold_mb,
    int critical_threshold_mb,
    std::unique_ptr<MemoryPressureVoter> voter)
    : memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
      moderate_threshold_mb_(moderate_threshold_mb),
      critical_threshold_mb_(critical_threshold_mb),
      moderate_pressure_repeat_count_(0) {
  DCHECK_GE(moderate_threshold_mb_, critical_threshold_mb_);
  DCHECK_LE(0, critical_threshold_mb_);
  StartObserving();
}

SystemMemoryPressureEvaluator::~SystemMemoryPressureEvaluator() {
  StopObserving();
}

void SystemMemoryPressureEvaluator::InferThresholds() {
  // Default to a 'high' memory situation, which uses more conservative
  // thresholds.
  bool high_memory = true;
  MEMORYSTATUSEX mem_status = {};
  if (GetSystemMemoryStatus(&mem_status)) {
    static const DWORDLONG kLargeMemoryThresholdBytes =
        static_cast<DWORDLONG>(kLargeMemoryThresholdMb) * kMBBytes;
    high_memory = mem_status.ullTotalPhys >= kLargeMemoryThresholdBytes;
  }

  if (high_memory) {
    moderate_threshold_mb_ = kLargeMemoryDefaultModerateThresholdMb;
    critical_threshold_mb_ = kLargeMemoryDefaultCriticalThresholdMb;
  } else {
    moderate_threshold_mb_ = kSmallMemoryDefaultModerateThresholdMb;
    critical_threshold_mb_ = kSmallMemoryDefaultCriticalThresholdMb;
  }
}

void SystemMemoryPressureEvaluator::StartObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timer_.Start(
      FROM_HERE, kMemorySamplingPeriod,
      BindRepeating(&SystemMemoryPressureEvaluator::CheckMemoryPressure,
                    weak_ptr_factory_.GetWeakPtr()));
}

void SystemMemoryPressureEvaluator::StopObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If StartObserving failed, StopObserving will still get called.
  timer_.Stop();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SystemMemoryPressureEvaluator::CheckMemoryPressure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get the previous pressure level and update the current one.
  MemoryPressureLevel old_vote = current_vote();
  SetCurrentVote(CalculateCurrentPressureLevel());

  // |notify| will be set to true if MemoryPressureListeners need to be
  // notified of a memory pressure level state change.
  bool notify = false;
  switch (current_vote()) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      break;

    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      if (old_vote != current_vote()) {
        // This is a new transition to moderate pressure so notify.
        moderate_pressure_repeat_count_ = 0;
        notify = true;
      } else {
        // Already in moderate pressure, only notify if sustained over the
        // cooldown period.
        const int kModeratePressureCooldownCycles =
            kModeratePressureCooldown / kMemorySamplingPeriod;
        if (++moderate_pressure_repeat_count_ ==
            kModeratePressureCooldownCycles) {
          moderate_pressure_repeat_count_ = 0;
          notify = true;
        }
      }
      break;

    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // Always notify of critical pressure levels.
      notify = true;
      break;
  }

  SendCurrentVote(notify);
}

base::MemoryPressureListener::MemoryPressureLevel
SystemMemoryPressureEvaluator::CalculateCurrentPressureLevel() {
  MEMORYSTATUSEX mem_status = {};
  bool got_system_memory_status = GetSystemMemoryStatus(&mem_status);
  // Report retrieval outcome before early returning on failure.
  base::UmaHistogramBoolean("Memory.MemoryStatusRetrievalSuccess",
                            got_system_memory_status);

  if (!got_system_memory_status) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  }
  RecordCommitHistograms(mem_status);

  // How much physical system memory is available for use right now, in MBs.
  int phys_free_mb = static_cast<int>(mem_status.ullAvailPhys / kMBBytes);

  // The maximum amount of memory the current process can commit, in MBs.
  int commit_available_mb =
      static_cast<int>(mem_status.ullAvailPageFile / kMBBytes);

  if (phys_free_mb > moderate_threshold_mb_ &&
      commit_available_mb > kCommitAvailableModerateThresholdMB.Get()) {
    // No memory pressure under any of the 2 detection systems. Return
    // early to avoid activating the experiment for clients who don't
    // have memory pressure.
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  }

  if (base::FeatureList::IsEnabled(kCommitAvailableMemoryPressure)) {
    if (commit_available_mb < kCommitAvailableCriticalThresholdMB.Get()) {
      return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
    }

    if (commit_available_mb < kCommitAvailableModerateThresholdMB.Get()) {
      return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
    }

    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  }

  // TODO(chrisha): This should eventually care about address space pressure,
  // but the browser process (where this is running) effectively never runs out
  // of address space. Renderers occasionally do, but it does them no good to
  // have the browser process monitor address space pressure. Long term,
  // renderers should run their own address space pressure monitors and act
  // accordingly, with the browser making cross-process decisions based on
  // system memory pressure.

  // Determine if the physical memory is under critical memory pressure.
  if (phys_free_mb <= critical_threshold_mb_) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }

  // Determine if the physical memory is under moderate memory pressure.
  if (phys_free_mb <= moderate_threshold_mb_) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  }

  // No memory pressure was detected.
  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

bool SystemMemoryPressureEvaluator::GetSystemMemoryStatus(
    MEMORYSTATUSEX* mem_status) {
  DCHECK(mem_status);
  mem_status->dwLength = sizeof(*mem_status);
  if (!::GlobalMemoryStatusEx(mem_status)) {
    return false;
  }
  return true;
}

void SystemMemoryPressureEvaluator::RecordCommitHistograms(
    const MEMORYSTATUSEX& mem_status) {
  // Calculate commit limit in MB.
  uint64_t commit_limit_mb = mem_status.ullTotalPageFile / kMBBytes;

  // Calculate amount of available commit space in MB.
  uint64_t commit_available_mb = mem_status.ullAvailPageFile / kMBBytes;

  base::UmaHistogramCounts10M("Memory.CommitLimitMB",
                              base::saturated_cast<int>(commit_limit_mb));
  base::UmaHistogramCounts10M("Memory.CommitAvailableMB",
                              base::saturated_cast<int>(commit_available_mb));

  // Calculate percentage used
  int percentage_used;
  if (commit_limit_mb == 0) {
    // Handle division by zero.
    percentage_used = 0;
  } else {
    uint64_t percentage_remaining =
        (commit_available_mb * 100) / commit_limit_mb;
    percentage_used = static_cast<int>(
        percentage_remaining > 100 ? 0u : 100 - percentage_remaining);
  }

  base::UmaHistogramPercentage("Memory.CommitPercentageUsed", percentage_used);
}

}  // namespace win
}  // namespace memory_pressure

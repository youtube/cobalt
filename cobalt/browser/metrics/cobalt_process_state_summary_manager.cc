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

#include "cobalt/browser/metrics/cobalt_process_state_summary_manager.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"

namespace cobalt {

std::string_view ExitReasonToHistogramSuffix(int exit_reason) {
  switch (static_cast<AndroidExitReason>(exit_reason)) {
    case AndroidExitReason::kAnr:
      return "Anr";
    case AndroidExitReason::kCrash:
      return "Crash";
    case AndroidExitReason::kCrashNative:
      return "CrashNative";
    case AndroidExitReason::kDependencyDied:
      return "DependencyDied";
    case AndroidExitReason::kExcessiveResourceUsage:
      return "ExcessiveResourceUsage";
    case AndroidExitReason::kExitSelf:
      return "ExitSelf";
    case AndroidExitReason::kInitializationFailure:
      return "InitializationFailure";
    case AndroidExitReason::kLowMemory:
      return "LowMemory";
    case AndroidExitReason::kOther:
      return "Other";
    case AndroidExitReason::kPermissionChange:
      return "PermissionChange";
    case AndroidExitReason::kSignaled:
      return "Signaled";
    case AndroidExitReason::kUnknown:
      return "Unknown";
    case AndroidExitReason::kUserRequested:
      return "UserRequested";
    case AndroidExitReason::kUserStopped:
      return "UserStopped";
    case AndroidExitReason::kApiFailed:
      return "ApiFailed";
    case AndroidExitReason::kFreezer:
      return "Freezer";
    case AndroidExitReason::kPackageStateChange:
      return "PackageStateChange";
    case AndroidExitReason::kPackageUpdated:
      return "PackageUpdated";
  }
  return "Other";
}

CobaltProcessStateSummaryManager*
CobaltProcessStateSummaryManager::GetInstance() {
  static base::NoDestructor<CobaltProcessStateSummaryManager> instance;
  return instance.get();
}

CobaltProcessStateSummaryManager::CobaltProcessStateSummaryManager()
    : start_ticks_(base::TimeTicks::Now()) {}

// static
std::vector<uint8_t> CobaltProcessStateSummaryManager::SerializeSnapshot(
    const ProcessStateSnapshot& snapshot) {
  std::vector<uint8_t> buffer(kProcessStateSummaryPayloadSize, 0);
  buffer[0] = kProcessStateSummaryMagic;
  buffer[1] = kProcessStateSummaryVersion;
  buffer[2] = snapshot.last_trim_level;
  buffer[3] = snapshot.flags;

  buffer[4] = static_cast<uint8_t>(snapshot.pid & 0xFF);
  buffer[5] = static_cast<uint8_t>((snapshot.pid >> 8) & 0xFF);
  buffer[6] = static_cast<uint8_t>((snapshot.pid >> 16) & 0xFF);
  buffer[7] = static_cast<uint8_t>((snapshot.pid >> 24) & 0xFF);

  buffer[8] = static_cast<uint8_t>(snapshot.peak_rss_kb & 0xFF);
  buffer[9] = static_cast<uint8_t>((snapshot.peak_rss_kb >> 8) & 0xFF);
  buffer[10] = static_cast<uint8_t>((snapshot.peak_rss_kb >> 16) & 0xFF);
  buffer[11] = static_cast<uint8_t>((snapshot.peak_rss_kb >> 24) & 0xFF);

  buffer[12] = static_cast<uint8_t>(snapshot.peak_pmf_kb & 0xFF);
  buffer[13] = static_cast<uint8_t>((snapshot.peak_pmf_kb >> 8) & 0xFF);
  buffer[14] = static_cast<uint8_t>((snapshot.peak_pmf_kb >> 16) & 0xFF);
  buffer[15] = static_cast<uint8_t>((snapshot.peak_pmf_kb >> 24) & 0xFF);

  buffer[16] = static_cast<uint8_t>(snapshot.peak_v8_code_kb & 0xFF);
  buffer[17] = static_cast<uint8_t>((snapshot.peak_v8_code_kb >> 8) & 0xFF);
  buffer[18] = static_cast<uint8_t>((snapshot.peak_v8_code_kb >> 16) & 0xFF);
  buffer[19] = static_cast<uint8_t>((snapshot.peak_v8_code_kb >> 24) & 0xFF);

  buffer[20] = static_cast<uint8_t>(snapshot.uptime_sec & 0xFF);
  buffer[21] = static_cast<uint8_t>((snapshot.uptime_sec >> 8) & 0xFF);
  buffer[22] = static_cast<uint8_t>((snapshot.uptime_sec >> 16) & 0xFF);
  buffer[23] = static_cast<uint8_t>((snapshot.uptime_sec >> 24) & 0xFF);

  buffer[24] = static_cast<uint8_t>(snapshot.startup_milestones & 0xFF);
  buffer[25] = static_cast<uint8_t>((snapshot.startup_milestones >> 8) & 0xFF);
  buffer[26] = static_cast<uint8_t>((snapshot.startup_milestones >> 16) & 0xFF);
  buffer[27] = static_cast<uint8_t>((snapshot.startup_milestones >> 24) & 0xFF);
  buffer[28] = static_cast<uint8_t>((snapshot.startup_milestones >> 32) & 0xFF);
  buffer[29] = static_cast<uint8_t>((snapshot.startup_milestones >> 40) & 0xFF);
  buffer[30] = static_cast<uint8_t>((snapshot.startup_milestones >> 48) & 0xFF);
  buffer[31] = static_cast<uint8_t>((snapshot.startup_milestones >> 56) & 0xFF);

  buffer[32] = snapshot.highest_milestone;
  buffer[33] = 0;
  buffer[34] = 0;
  buffer[35] = 0;

  // Compute 32-bit PersistentHash checksum over bytes [0..35].
  uint32_t checksum = base::PersistentHash(base::span(buffer).first(36u));
  buffer[36] = static_cast<uint8_t>(checksum & 0xFF);
  buffer[37] = static_cast<uint8_t>((checksum >> 8) & 0xFF);
  buffer[38] = static_cast<uint8_t>((checksum >> 16) & 0xFF);
  buffer[39] = static_cast<uint8_t>((checksum >> 24) & 0xFF);

  return buffer;
}

// static
std::optional<ProcessStateSnapshot>
CobaltProcessStateSummaryManager::DeserializeSnapshot(
    base::span<const uint8_t> buffer) {
  // Layer 1: Payload size check (supports V2: 40 bytes, or V1: 28 bytes).
  if (buffer.size() != kProcessStateSummaryPayloadSize &&
      buffer.size() != kProcessStateSummaryV1PayloadSize) {
    LOG(WARNING) << "Invalid process state summary payload size: expected "
                 << kProcessStateSummaryPayloadSize << ", got "
                 << buffer.size();
    return std::nullopt;
  }

  // Layer 2: Magic check.
  if (buffer[0] != kProcessStateSummaryMagic) {
    LOG(WARNING) << "Invalid process state summary magic: "
                 << static_cast<int>(buffer[0]);
    return std::nullopt;
  }

  uint8_t version = buffer[1];
  if (version != 1 && version != kProcessStateSummaryVersion) {
    LOG(WARNING) << "Unsupported process state summary version: "
                 << static_cast<int>(version);
    return std::nullopt;
  }

  // Layer 3: Checksum verification over payload data bytes before checksum.
  if (version == kProcessStateSummaryVersion) {
    if (buffer.size() != kProcessStateSummaryPayloadSize) {
      return std::nullopt;
    }
    uint32_t stored_checksum = static_cast<uint32_t>(buffer[36]) |
                               (static_cast<uint32_t>(buffer[37]) << 8) |
                               (static_cast<uint32_t>(buffer[38]) << 16) |
                               (static_cast<uint32_t>(buffer[39]) << 24);
    uint32_t computed_checksum = base::PersistentHash(buffer.first(36u));
    if (stored_checksum != computed_checksum) {
      LOG(WARNING) << "Process state summary checksum mismatch: stored="
                   << stored_checksum << ", computed=" << computed_checksum;
      return std::nullopt;
    }
  } else if (version == 1) {
    if (buffer.size() != kProcessStateSummaryV1PayloadSize) {
      return std::nullopt;
    }
    uint32_t stored_checksum = static_cast<uint32_t>(buffer[24]) |
                               (static_cast<uint32_t>(buffer[25]) << 8) |
                               (static_cast<uint32_t>(buffer[26]) << 16) |
                               (static_cast<uint32_t>(buffer[27]) << 24);
    uint32_t computed_checksum = base::PersistentHash(buffer.first(24u));
    if (stored_checksum != computed_checksum) {
      LOG(WARNING) << "Process state summary V1 checksum mismatch: stored="
                   << stored_checksum << ", computed=" << computed_checksum;
      return std::nullopt;
    }
  }

  ProcessStateSnapshot snapshot;
  snapshot.last_trim_level = buffer[2];
  snapshot.flags = buffer[3];

  snapshot.pid = static_cast<uint32_t>(buffer[4]) |
                 (static_cast<uint32_t>(buffer[5]) << 8) |
                 (static_cast<uint32_t>(buffer[6]) << 16) |
                 (static_cast<uint32_t>(buffer[7]) << 24);

  snapshot.peak_rss_kb = static_cast<uint32_t>(buffer[8]) |
                         (static_cast<uint32_t>(buffer[9]) << 8) |
                         (static_cast<uint32_t>(buffer[10]) << 16) |
                         (static_cast<uint32_t>(buffer[11]) << 24);

  snapshot.peak_pmf_kb = static_cast<uint32_t>(buffer[12]) |
                         (static_cast<uint32_t>(buffer[13]) << 8) |
                         (static_cast<uint32_t>(buffer[14]) << 16) |
                         (static_cast<uint32_t>(buffer[15]) << 24);

  snapshot.peak_v8_code_kb = static_cast<uint32_t>(buffer[16]) |
                             (static_cast<uint32_t>(buffer[17]) << 8) |
                             (static_cast<uint32_t>(buffer[18]) << 16) |
                             (static_cast<uint32_t>(buffer[19]) << 24);

  snapshot.uptime_sec = static_cast<uint32_t>(buffer[20]) |
                        (static_cast<uint32_t>(buffer[21]) << 8) |
                        (static_cast<uint32_t>(buffer[22]) << 16) |
                        (static_cast<uint32_t>(buffer[23]) << 24);

  if (version >= 2) {
    snapshot.startup_milestones = static_cast<uint64_t>(buffer[24]) |
                                  (static_cast<uint64_t>(buffer[25]) << 8) |
                                  (static_cast<uint64_t>(buffer[26]) << 16) |
                                  (static_cast<uint64_t>(buffer[27]) << 24) |
                                  (static_cast<uint64_t>(buffer[28]) << 32) |
                                  (static_cast<uint64_t>(buffer[29]) << 40) |
                                  (static_cast<uint64_t>(buffer[30]) << 48) |
                                  (static_cast<uint64_t>(buffer[31]) << 56);
    snapshot.highest_milestone = buffer[32];
  }

  // Layer 4: Non-trivial / non-zero validation.
  if (snapshot.peak_rss_kb == 0xFFFFFFFF ||
      snapshot.peak_pmf_kb == 0xFFFFFFFF) {
    LOG(WARNING)
        << "Process state summary contains trivial or uninitialized memory: "
        << "rss=" << snapshot.peak_rss_kb << ", pmf=" << snapshot.peak_pmf_kb;
    return std::nullopt;
  }

  // Layer 5: Plausibility range invariants.
  constexpr uint32_t kMaxPlausibleMemoryKb = 64 * 1024 * 1024;  // 64 GB
  if (snapshot.peak_rss_kb > kMaxPlausibleMemoryKb ||
      snapshot.peak_pmf_kb > kMaxPlausibleMemoryKb ||
      snapshot.peak_v8_code_kb > kMaxPlausibleMemoryKb) {
    LOG(WARNING) << "Process state summary contains implausible memory values: "
                 << "rss=" << snapshot.peak_rss_kb
                 << ", pmf=" << snapshot.peak_pmf_kb;
    return std::nullopt;
  }

  constexpr uint32_t kMaxPlausibleUptimeSec = 180 * 86400;  // 180 days
  if (snapshot.uptime_sec > kMaxPlausibleUptimeSec) {
    LOG(WARNING) << "Process state summary contains implausible uptime: "
                 << snapshot.uptime_sec;
    return std::nullopt;
  }

  if (snapshot.last_trim_level > 100) {
    LOG(WARNING) << "Process state summary contains invalid trim level: "
                 << static_cast<int>(snapshot.last_trim_level);
    return std::nullopt;
  }

  if (snapshot.highest_milestone >= 64) {
    LOG(WARNING) << "Process state summary contains invalid milestone: "
                 << static_cast<int>(snapshot.highest_milestone);
    return std::nullopt;
  }

  if ((snapshot.flags & ~kProcessStateKnownFlagsMask) != 0) {
    LOG(WARNING) << "Process state summary contains unknown flag bits: "
                 << static_cast<int>(snapshot.flags);
    return std::nullopt;
  }

  return snapshot;
}

base::TimeTicks CobaltProcessStateSummaryManager::GetLastSyncTicksForTesting()
    const {
  base::AutoLock auto_lock(lock_);
  return last_sync_ticks_;
}

bool CobaltProcessStateSummaryManager::IsDirtyForTesting() const {
  base::AutoLock auto_lock(lock_);
  return is_dirty_;
}

bool CobaltProcessStateSummaryManager::IsTaskScheduledForTesting() const {
  base::AutoLock auto_lock(lock_);
  return is_task_scheduled_;
}

void CobaltProcessStateSummaryManager::ResetForTesting() {
  base::AutoLock auto_lock(lock_);
  start_ticks_ = base::TimeTicks::Now();
  last_sync_ticks_ = base::TimeTicks();
  is_dirty_ = false;
  is_task_scheduled_ = false;
  task_runner_.reset();
  peak_rss_kb_ = 0;
  peak_pmf_kb_ = 0;
  peak_v8_code_kb_ = 0;
  startup_milestones_ = 0;
  highest_milestone_ = 0;
  last_trim_level_ = 0;
  flags_ = 0;
}

void CobaltProcessStateSummaryManager::UpdateMemoryFootprint(
    uint32_t current_rss_kb,
    uint32_t current_pmf_kb,
    uint32_t current_v8_code_kb) {
  std::optional<std::vector<uint8_t>> payload;
  {
    base::AutoLock auto_lock(lock_);
    bool new_peak = false;
    if (current_rss_kb > peak_rss_kb_) {
      peak_rss_kb_ = current_rss_kb;
      new_peak = true;
    }
    if (current_pmf_kb > peak_pmf_kb_) {
      peak_pmf_kb_ = current_pmf_kb;
      new_peak = true;
    }
    if (current_v8_code_kb > peak_v8_code_kb_) {
      peak_v8_code_kb_ = current_v8_code_kb;
      new_peak = true;
    }

    if (new_peak) {
      is_dirty_ = true;
      payload = MaybeSyncOrScheduleLocked(/*is_emergency=*/false);
    }
  }

  if (payload) {
    SyncToSystemServer(*payload);
  }
}

void CobaltProcessStateSummaryManager::UpdateTrimMemoryLevel(uint8_t level) {
  std::optional<std::vector<uint8_t>> payload;
  {
    base::AutoLock auto_lock(lock_);
    if (last_trim_level_ == level) {
      return;
    }
    last_trim_level_ = level;
    is_dirty_ = true;
    payload = MaybeSyncOrScheduleLocked(/*is_emergency=*/false);
  }

  if (payload) {
    SyncToSystemServer(*payload);
  }
}

void CobaltProcessStateSummaryManager::SetFlags(uint8_t flags) {
  std::optional<std::vector<uint8_t>> payload;
  {
    base::AutoLock auto_lock(lock_);
    if (flags_ == flags) {
      return;
    }
    flags_ = flags;
    is_dirty_ = true;
    payload = MaybeSyncOrScheduleLocked(/*is_emergency=*/false);
  }

  if (payload) {
    SyncToSystemServer(*payload);
  }
}

void CobaltProcessStateSummaryManager::SetStartupMilestone(uint8_t milestone) {
  if (milestone >= 64) {
    return;
  }
  std::optional<std::vector<uint8_t>> payload;
  {
    base::AutoLock auto_lock(lock_);
    uint64_t mask = 1ULL << milestone;
    if ((startup_milestones_ & mask) != 0 && highest_milestone_ >= milestone) {
      return;
    }
    startup_milestones_ |= mask;
    if (milestone > highest_milestone_) {
      highest_milestone_ = milestone;
    }
    is_dirty_ = true;
    payload = MaybeSyncOrScheduleLocked(/*is_emergency=*/false);
  }

  if (payload) {
    SyncToSystemServer(*payload);
  }
}

void CobaltProcessStateSummaryManager::SetStartupGuardArmed(bool is_armed) {
  std::optional<std::vector<uint8_t>> payload;
  {
    base::AutoLock auto_lock(lock_);
    uint8_t new_flags = flags_;
    if (is_armed) {
      new_flags |= kFlagStartupGuardArmed;
    } else {
      new_flags &= ~kFlagStartupGuardArmed;
    }
    if (flags_ == new_flags) {
      return;
    }
    flags_ = new_flags;
    is_dirty_ = true;
    payload = MaybeSyncOrScheduleLocked(/*is_emergency=*/false);
  }

  if (payload) {
    SyncToSystemServer(*payload);
  }
}

void CobaltProcessStateSummaryManager::SetStartupGuardTriggeredKill() {
  std::optional<std::vector<uint8_t>> payload;
  {
    base::AutoLock auto_lock(lock_);
    flags_ |= kFlagStartupGuardTriggeredKill;
    is_dirty_ = true;
    payload = MaybeSyncOrScheduleLocked(/*is_emergency=*/true);
  }

  if (payload) {
    SyncToSystemServer(*payload);
  }
}

void CobaltProcessStateSummaryManager::FlushPendingSyncNow() {
  std::optional<std::vector<uint8_t>> payload;
  {
    base::AutoLock auto_lock(lock_);
    if (!is_dirty_) {
      return;
    }
    is_dirty_ = false;
    is_task_scheduled_ = false;
    last_sync_ticks_ = base::TimeTicks::Now();
    payload = PreparePayloadLocked();
  }

  if (payload) {
    SyncToSystemServer(*payload);
  }
}

void CobaltProcessStateSummaryManager::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  base::AutoLock auto_lock(lock_);
  task_runner_ = std::move(task_runner);
}

std::optional<std::vector<uint8_t>>
CobaltProcessStateSummaryManager::PreparePayloadLocked() {
  uint32_t uptime_sec = static_cast<uint32_t>(std::max<int64_t>(
      0, (base::TimeTicks::Now() - start_ticks_).InSeconds()));

  ProcessStateSnapshot snapshot;
  snapshot.pid = static_cast<uint32_t>(base::GetCurrentProcId());
  snapshot.last_trim_level = last_trim_level_;
  snapshot.flags = flags_;
  snapshot.peak_rss_kb = peak_rss_kb_;
  snapshot.peak_pmf_kb = peak_pmf_kb_;
  snapshot.peak_v8_code_kb = peak_v8_code_kb_;
  snapshot.uptime_sec = uptime_sec;
  snapshot.startup_milestones = startup_milestones_;
  snapshot.highest_milestone = highest_milestone_;

  return SerializeSnapshot(snapshot);
}

std::optional<std::vector<uint8_t>>
CobaltProcessStateSummaryManager::MaybeSyncOrScheduleLocked(bool is_emergency) {
  if (!is_dirty_) {
    return std::nullopt;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  bool can_sync_immediately =
      is_emergency || last_sync_ticks_.is_null() ||
      (now - last_sync_ticks_ >= kMinProcessStateSummarySyncInterval);

  if (can_sync_immediately) {
    is_dirty_ = false;
    is_task_scheduled_ = false;
    last_sync_ticks_ = now;
    return PreparePayloadLocked();
  }

  if (!is_task_scheduled_) {
    base::TimeDelta delay =
        kMinProcessStateSummarySyncInterval - (now - last_sync_ticks_);
    if (delay < base::TimeDelta()) {
      delay = base::TimeDelta();
    }
    is_task_scheduled_ = true;
    PostDelayedSyncTaskLocked(delay);
  }
  return std::nullopt;
}

void CobaltProcessStateSummaryManager::FlushDirtyStateFromTask() {
  std::optional<std::vector<uint8_t>> payload;
  {
    base::AutoLock auto_lock(lock_);
    is_task_scheduled_ = false;
    if (!is_dirty_) {
      return;
    }
    base::TimeTicks now = base::TimeTicks::Now();
    if (!last_sync_ticks_.is_null() &&
        (now - last_sync_ticks_ < kMinProcessStateSummarySyncInterval)) {
      base::TimeDelta remaining =
          kMinProcessStateSummarySyncInterval - (now - last_sync_ticks_);
      is_task_scheduled_ = true;
      PostDelayedSyncTaskLocked(remaining);
      return;
    }
    is_dirty_ = false;
    last_sync_ticks_ = now;
    payload = PreparePayloadLocked();
  }

  if (payload) {
    SyncToSystemServer(*payload);
  }
}

void CobaltProcessStateSummaryManager::PostDelayedSyncTaskLocked(
    base::TimeDelta delay) {
  scoped_refptr<base::SequencedTaskRunner> runner = GetTaskRunnerLocked();
  if (!runner) {
    return;
  }
  runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CobaltProcessStateSummaryManager::FlushDirtyStateFromTask,
                     base::Unretained(this)),
      delay);
}

scoped_refptr<base::SequencedTaskRunner>
CobaltProcessStateSummaryManager::GetTaskRunnerLocked() {
  if (task_runner_) {
    return task_runner_;
  }
  if (base::SequencedTaskRunner::HasCurrentDefault()) {
    return base::SequencedTaskRunner::GetCurrentDefault();
  }
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

}  // namespace cobalt

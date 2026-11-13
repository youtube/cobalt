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

#ifndef COBALT_BROWSER_METRICS_COBALT_PROCESS_STATE_SUMMARY_MANAGER_H_
#define COBALT_BROWSER_METRICS_COBALT_PROCESS_STATE_SUMMARY_MANAGER_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"

namespace cobalt {

// Holds the deserialized 16-byte V1 Android process state summary.
struct ProcessStateSummaryData {
  bool startup_guard_armed = false;
  bool startup_guard_triggered_kill = false;
  uint64_t startup_milestones = 0;
  uint8_t highest_milestone = 0;

  bool operator==(const ProcessStateSummaryData& other) const = default;
};

// Android-only manager that attaches and retrieves the 16-byte process state
// summary via Android's ActivityManager.setProcessStateSummary() and
// ApplicationExitInfo.getProcessStateSummary() (Android R+ / API level 30+).
//
// Lifetime & ownership: Process-wide singleton owned by base::NoDestructor
// (overridable in tests via SetInstanceForTesting()).
// Threading model: Thread-safe. In-memory state is guarded by `lock_`, while
// JNI/Binder IPC is serialized via a single-flight loop outside `lock_`.
//
// Wire format (16 bytes):
//   [0]     : Magic (0xCB)
//   [1]     : Version (0x01)
//   [2]     : Flags (bit 0: guard armed, bit 1: guard triggered kill)
//   [3..10] : startup_milestones (uint64_t little-endian)
//   [11]    : highest_milestone (uint8_t)
//   [12..15]: 32-bit PersistentHash checksum of bytes [0..11] (little-endian)
class CobaltProcessStateSummaryManager {
 public:
  static constexpr uint8_t kMagicByte = 0xCB;
  static constexpr uint8_t kVersionByte = 0x01;
  static constexpr uint8_t kFlagStartupGuardArmed = 0x01;
  static constexpr uint8_t kFlagStartupGuardTriggeredKill = 0x02;
  static constexpr size_t kSummaryPayloadSize = 16;
  static constexpr size_t kChecksumDataSize = 12;

  // `startup_milestones` is stored as a 64-bit bitmask (`uint64_t`) where bit N
  // (`1ULL << N`) represents milestone N. Value 0 (`highest_milestone == 0`) is
  // reserved to indicate that no startup milestone has been recorded yet, so
  // valid milestone IDs span bits 1 through 63.
  static constexpr int kMinStartupMilestone = 1;
  static constexpr int kMaxStartupMilestone = 63;

  static CobaltProcessStateSummaryManager* GetInstance();

  CobaltProcessStateSummaryManager(const CobaltProcessStateSummaryManager&) =
      delete;
  CobaltProcessStateSummaryManager& operator=(
      const CobaltProcessStateSummaryManager&) = delete;

  // Records a startup milestone in [kMinStartupMilestone,
  // kMaxStartupMilestone].
  void SetStartupMilestone(int milestone);

  // Updates whether StartupGuard is currently armed.
  void SetStartupGuardArmed(bool armed);

  // Forces an immediate sync to the OS, bypassing the 15-second rate limiter.
  void FlushSummaryImmediate();

  // Serializes summary data into a 16-byte vector with checksum.
  static std::vector<uint8_t> Serialize(const ProcessStateSummaryData& data);

  // Deserializes and validates a 16-byte summary payload.
  static std::optional<ProcessStateSummaryData> Deserialize(
      base::span<const uint8_t> bytes);

  // Retrieves the prior-session summary and optional foreground exit reason
  // from the OS (Android R+).
  std::optional<ProcessStateSummaryData> GetPriorSessionSummary(
      int* out_exit_reason = nullptr);

  static void SetInstanceForTesting(CobaltProcessStateSummaryManager* instance);

 protected:
  friend class base::NoDestructor<CobaltProcessStateSummaryManager>;
  CobaltProcessStateSummaryManager();
  virtual ~CobaltProcessStateSummaryManager();

  virtual void FlushPayloadToPlatform(base::span<const uint8_t> payload)
      LOCKS_EXCLUDED(lock_);

  void ResetForTesting();
  ProcessStateSummaryData GetCurrentDataForTesting();
  bool IsDirtyForTesting();

 private:
  static void RunScheduledDelayedFlush(uint64_t generation);
  void OnDelayedFlushFired(uint64_t generation) LOCKS_EXCLUDED(lock_);

  base::TimeDelta GetRemainingFlushCooldownLocked(base::TimeTicks now) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool IsFlushCooldownExpiredLocked(base::TimeTicks now) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool ShouldFlushLocked(base::TimeTicks now) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  std::optional<std::vector<uint8_t>> ClaimNextFlushPayload()
      LOCKS_EXCLUDED(lock_);
  void CompleteCurrentFlush() LOCKS_EXCLUDED(lock_);
  void MaybeScheduleDelayedFlush() LOCKS_EXCLUDED(lock_);
  void MaybeFlush() LOCKS_EXCLUDED(lock_);

  base::Lock lock_;
  ProcessStateSummaryData current_data_ GUARDED_BY(lock_);
  base::TimeTicks last_flush_time_ GUARDED_BY(lock_);
  uint64_t flush_generation_ GUARDED_BY(lock_) = 0;
  bool dirty_ GUARDED_BY(lock_) = false;
  bool flush_in_progress_ GUARDED_BY(lock_) = false;
  bool pending_immediate_flush_ GUARDED_BY(lock_) = false;
  bool delayed_flush_scheduled_ GUARDED_BY(lock_) = false;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_PROCESS_STATE_SUMMARY_MANAGER_H_

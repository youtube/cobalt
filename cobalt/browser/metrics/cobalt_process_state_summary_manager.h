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
#include "base/time/time.h"

namespace cobalt {

// Holds the deserialized 16-byte V1 process state summary.
struct ProcessStateSummaryData {
  bool startup_guard_armed = false;
  bool startup_guard_triggered_kill = false;
  uint64_t startup_milestones = 0;
  uint8_t highest_milestone = 0;

  bool operator==(const ProcessStateSummaryData& other) const {
    return startup_guard_armed == other.startup_guard_armed &&
           startup_guard_triggered_kill == other.startup_guard_triggered_kill &&
           startup_milestones == other.startup_milestones &&
           highest_milestone == other.highest_milestone;
  }
};

// Manages attaching and retrieving the 16-byte process state summary.
// Wire format (16 bytes):
//   [0]      : Magic (0xCB)
//   [1]      : Version (0x01)
//   [2]      : Flags (bit 0: startup_guard_armed, bit 1:
//   startup_guard_triggered_kill) [3..10]  : startup_milestones (uint64_t
//   little-endian) [11]     : highest_milestone (uint8_t) [12..15] : 32-bit
//   PersistentHash checksum of bytes [0..11] (little-endian)
class CobaltProcessStateSummaryManager {
 public:
  static constexpr uint8_t kMagicByte = 0xCB;
  static constexpr uint8_t kVersionByte = 0x01;
  static constexpr uint8_t kFlagStartupGuardArmed = 0x01;
  static constexpr uint8_t kFlagStartupGuardTriggeredKill = 0x02;
  static constexpr size_t kSummaryPayloadSize = 16;
  static constexpr size_t kChecksumDataSize = 12;
  static constexpr base::TimeDelta kMinFlushInterval = base::Seconds(15);

  static CobaltProcessStateSummaryManager* GetInstance();

  CobaltProcessStateSummaryManager(const CobaltProcessStateSummaryManager&) =
      delete;
  CobaltProcessStateSummaryManager& operator=(
      const CobaltProcessStateSummaryManager&) = delete;

  // Records a startup milestone (1..63).
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

  void ResetForTesting();
  ProcessStateSummaryData GetCurrentDataForTesting();
  bool IsDirtyForTesting();

 private:
  friend class base::NoDestructor<CobaltProcessStateSummaryManager>;
  CobaltProcessStateSummaryManager();
  ~CobaltProcessStateSummaryManager();

  void FlushSummaryLocked();
  void MaybeFlushSummaryLocked();
  void PlatformSetProcessStateSummary(base::span<const uint8_t> summary_bytes);
  std::optional<std::vector<uint8_t>> PlatformGetPriorSessionSummary(
      int* out_exit_reason);

  base::Lock lock_;
  ProcessStateSummaryData current_data_;
  base::TimeTicks last_flush_time_;
  bool dirty_ = false;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_PROCESS_STATE_SUMMARY_MANAGER_H_

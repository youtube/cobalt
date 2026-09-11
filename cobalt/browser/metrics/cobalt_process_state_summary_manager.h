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

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace cobalt {

// Magic identifier byte for Cobalt process state summary: 'C'obalt (0xCB).
inline constexpr uint8_t kProcessStateSummaryMagic = 0xCB;
// Current serialization format version.
inline constexpr uint8_t kProcessStateSummaryVersion = 1;
// Fixed payload length in bytes (strictly <= 128 bytes limit in Android OS).
inline constexpr size_t kProcessStateSummaryPayloadSize = 20;

// Flag bits packed into snapshot flags byte.
enum ProcessStateFlags : uint8_t {
  kFlagForeground = 1 << 0,
  kFlagMediaPlaying = 1 << 1,
};

struct ProcessStateSnapshot {
  uint32_t peak_rss_kb = 0;
  uint32_t peak_pmf_kb = 0;
  uint32_t peak_v8_code_kb = 0;
  uint32_t uptime_sec = 0;
  uint8_t last_trim_level = 0;
  uint8_t flags = 0;

  uint32_t uptime_minutes() const { return uptime_sec / 60; }

  bool operator==(const ProcessStateSnapshot& other) const = default;
};

// Maps Android Exit Reason enum to standardized UMA histogram token suffix.
std::string_view ExitReasonToHistogramSuffix(int exit_reason);

class CobaltProcessStateSummaryManager {
 public:
  static CobaltProcessStateSummaryManager* GetInstance();

  // Updates memory footprint and synchronizes to Android system server
  // if a new peak is detected. Safe to call from any thread.
  void UpdateMemoryFootprint(uint32_t current_rss_kb,
                             uint32_t current_pmf_kb,
                             uint32_t current_v8_code_kb);

  // Updates the last received ComponentCallbacks2 onTrimMemory level.
  void UpdateTrimMemoryLevel(uint8_t level);

  // Updates session status flags (e.g. foreground, media playing).
  void SetFlags(uint8_t flags);

  // Queries Android OS for the prior session PID's state summary.
  std::optional<ProcessStateSnapshot> ReadPriorSessionSnapshot(
      base::ProcessId prior_pid);

  // Queries Android OS for the prior session PID's exit reason code.
  int GetPriorSessionExitReason(base::ProcessId prior_pid);

  // Serializes |snapshot| into the 20-byte packed binary format.
  static std::vector<uint8_t> SerializeSnapshot(
      const ProcessStateSnapshot& snapshot);

  // Deserializes a binary buffer into a ProcessStateSnapshot.
  // Returns std::nullopt if the buffer is malformed, truncated, or has an
  // invalid magic/version.
  static std::optional<ProcessStateSnapshot> DeserializeSnapshot(
      base::span<const uint8_t> buffer);

  // Resets internal state for unit testing.
  void ResetForTesting();

 private:
  friend class base::NoDestructor<CobaltProcessStateSummaryManager>;
  CobaltProcessStateSummaryManager();

  void SyncToSystemServerLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  base::Lock lock_;
  base::TimeTicks start_ticks_;
  uint32_t peak_rss_kb_ GUARDED_BY(lock_) = 0;
  uint32_t peak_pmf_kb_ GUARDED_BY(lock_) = 0;
  uint32_t peak_v8_code_kb_ GUARDED_BY(lock_) = 0;
  uint8_t last_trim_level_ GUARDED_BY(lock_) = 0;
  uint8_t flags_ GUARDED_BY(lock_) = 0;
  bool dirty_ GUARDED_BY(lock_) = false;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_PROCESS_STATE_SUMMARY_MANAGER_H_

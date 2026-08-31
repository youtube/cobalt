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

#ifndef COBALT_BROWSER_METRICS_COBALT_STARTUP_TOMBSTONE_H_
#define COBALT_BROWSER_METRICS_COBALT_STARTUP_TOMBSTONE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/synchronization/lock.h"

namespace cobalt {

// Fixed size of the startup tombstone file (4 KiB = 1 OS page).
inline constexpr size_t kStartupTombstoneFileSize = 4096;
inline constexpr uint32_t kStartupTombstoneMagic = 0x53544D42;  // 'STMB'
inline constexpr uint32_t kStartupTombstoneVersion = 1;

enum class StartupTombstoneState : uint32_t {
  kUninitialized = 0,
  kEarlyInit = 1,
  kPreCreateThreads = 2,
  kPreMainLoop = 3,
  kNavigationStarted = 4,
  kNavigationCommitted = 5,
  kStartupComplete = 6,
  kCleanShutdown = 7,
  kCrashed = 8,
  kWatchdogTimeout = 9,
  kOom = 10,
};

enum class PriorRunStatus : int {
  kCleanOrNone = 0,
  kIncompleteStartup = 1,
  kCrashCaught = 2,
  kWatchdogTimeout = 3,
  kCleanShutdown = 4,
  kMaxValue = kCleanShutdown,
};

#pragma pack(push, 4)
struct alignas(8) StartupTombstoneHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t state;
  uint32_t process_id;
  int64_t start_time_us;
  int64_t last_update_time_us;
  uint64_t milestone_bitmask;
  int32_t exit_code;
  uint32_t pma_capacity_bytes;
  uint32_t pma_used_bytes;
  uint32_t crash_signal;
  char last_stage_name[64];
  char crash_reason[128];
  uint32_t checksum;
  uint32_t reserved_u32;
  uint8_t reserved[4096 - 256];
};
#pragma pack(pop)

static_assert(sizeof(StartupTombstoneHeader) == kStartupTombstoneFileSize,
              "StartupTombstoneHeader must be exactly 4096 bytes");

class CobaltStartupTombstone {
 public:
  static CobaltStartupTombstone* GetInstance();

  CobaltStartupTombstone();
  ~CobaltStartupTombstone();

  CobaltStartupTombstone(const CobaltStartupTombstone&) = delete;
  CobaltStartupTombstone& operator=(const CobaltStartupTombstone&) = delete;

  // Initialize tombstone with storage directory.
  // Processes prior run tombstone if one exists, then maps new tombstone for
  // current session.
  bool Initialize(const base::FilePath& storage_dir);

  // Read prior run tombstone (if any) and emit stability histograms.
  void ProcessPriorRunTombstone();

  // Mark startup milestone (0-63) in bitmask and update timestamp.
  void SetMilestone(int milestone_id);

  // Update current startup stage and stage name.
  void SetStage(StartupTombstoneState state, std::string_view stage_name);

  // Record crash or unexpected failure before aborting.
  void RecordCrash(int signal, std::string_view reason);

  // Update PMA capacity / used metrics snapshot.
  void UpdatePmaStats(size_t used_bytes, size_t capacity_bytes);

  // Mark startup successfully completed.
  void MarkStartupComplete();

  // Mark process clean shutdown.
  void MarkCleanShutdown();

  // Accessors for testing.
  const StartupTombstoneHeader* GetHeaderForTesting() const { return header_; }
  const StartupTombstoneHeader& GetPriorHeaderForTesting() const {
    return prior_header_;
  }
  bool HasPriorTombstoneForTesting() const { return has_prior_tombstone_; }
  bool IsInitializedForTesting() const { return header_ != nullptr; }

 private:
  uint32_t ComputeChecksum(const StartupTombstoneHeader* hdr) const;
  void UpdateHeaderChecksumLocked();

  base::Lock lock_;
  base::FilePath tombstone_path_;
  std::unique_ptr<base::MemoryMappedFile> mapped_file_;
  StartupTombstoneHeader* header_ = nullptr;
  StartupTombstoneHeader prior_header_ = {};
  bool has_prior_tombstone_ = false;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_STARTUP_TOMBSTONE_H_

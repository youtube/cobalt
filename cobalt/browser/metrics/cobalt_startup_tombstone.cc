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

#include "cobalt/browser/metrics/cobalt_startup_tombstone.h"

#include <string.h>

#include <algorithm>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"

namespace cobalt {

namespace {

constexpr char kTombstoneFileName[] = "startup_tombstone.dat";

}  // namespace

// static
CobaltStartupTombstone* CobaltStartupTombstone::GetInstance() {
  static base::NoDestructor<CobaltStartupTombstone> instance;
  return instance.get();
}

CobaltStartupTombstone::CobaltStartupTombstone() = default;

CobaltStartupTombstone::~CobaltStartupTombstone() {
  if (header_) {
    MarkCleanShutdown();
  }
}

uint32_t CobaltStartupTombstone::ComputeChecksum(
    const StartupTombstoneHeader* hdr) const {
  if (!hdr) {
    return 0;
  }
  const uint8_t* data = reinterpret_cast<const uint8_t*>(hdr);
  size_t size = offsetof(StartupTombstoneHeader, checksum);
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < size; ++i) {
    hash ^= data[i];
    hash *= 16777619u;
  }
  return hash;
}

void CobaltStartupTombstone::UpdateHeaderChecksumLocked() {
  if (header_) {
    header_->checksum = ComputeChecksum(header_);
  }
}

bool CobaltStartupTombstone::Initialize(const base::FilePath& storage_dir) {
  base::AutoLock auto_lock(lock_);
  if (header_) {
    return true;
  }

  tombstone_path_ = storage_dir.AppendASCII(kTombstoneFileName);

  // 1. Inspect any existing prior run tombstone
  if (base::PathExists(tombstone_path_)) {
    std::optional<int64_t> file_size = base::GetFileSize(tombstone_path_);
    if (file_size.has_value() &&
        static_cast<size_t>(file_size.value()) == kStartupTombstoneFileSize) {
      base::File existing_file(tombstone_path_,
                               base::File::FLAG_OPEN | base::File::FLAG_READ);
      if (existing_file.IsValid()) {
        StartupTombstoneHeader read_hdr = {};
        int bytes_read = existing_file.Read(
            0, reinterpret_cast<char*>(&read_hdr), sizeof(read_hdr));
        if (bytes_read == sizeof(read_hdr) &&
            read_hdr.magic == kStartupTombstoneMagic &&
            read_hdr.version == kStartupTombstoneVersion) {
          uint32_t expected_checksum = ComputeChecksum(&read_hdr);
          if (read_hdr.checksum == expected_checksum) {
            prior_header_ = read_hdr;
            has_prior_tombstone_ = true;
            LOG(INFO)
                << "Startup Tombstone: Recovered prior run tombstone (state="
                << prior_header_.state
                << ", last_stage=" << prior_header_.last_stage_name
                << ", bitmask=0x" << std::hex << prior_header_.milestone_bitmask
                << std::dec << ")";
          } else {
            LOG(WARNING)
                << "Startup Tombstone: Prior tombstone checksum mismatch!";
          }
        }
      }
    }
  }

  // 2. Open / create the file with 4096 bytes and memory map it for current
  // session
  base::File file(tombstone_path_, base::File::FLAG_CREATE_ALWAYS |
                                       base::File::FLAG_READ |
                                       base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(ERROR) << "Startup Tombstone: Failed to create tombstone file at "
               << tombstone_path_.value();
    return false;
  }

  if (!file.SetLength(kStartupTombstoneFileSize)) {
    LOG(ERROR) << "Startup Tombstone: Failed to set length of tombstone file";
    return false;
  }

  mapped_file_ = std::make_unique<base::MemoryMappedFile>();
  if (!mapped_file_->Initialize(std::move(file),
                                base::MemoryMappedFile::READ_WRITE)) {
    LOG(ERROR) << "Startup Tombstone: Failed to memory map tombstone file";
    mapped_file_.reset();
    return false;
  }

  header_ = reinterpret_cast<StartupTombstoneHeader*>(mapped_file_->data());
  memset(header_, 0, kStartupTombstoneFileSize);

  int64_t now_us = base::TimeTicks::Now().since_origin().InMicroseconds();
  header_->magic = kStartupTombstoneMagic;
  header_->version = kStartupTombstoneVersion;
  header_->state = static_cast<uint32_t>(StartupTombstoneState::kEarlyInit);
  header_->process_id = static_cast<uint32_t>(base::GetCurrentProcId());
  header_->start_time_us = now_us;
  header_->last_update_time_us = now_us;
  header_->milestone_bitmask = 0;
  header_->exit_code = 0;
  header_->crash_signal = 0;
  strncpy(header_->last_stage_name, "EarlyInitialization",
          sizeof(header_->last_stage_name) - 1);
  UpdateHeaderChecksumLocked();

  LOG(INFO) << "Startup Tombstone: Initialized 4KB memory-mapped tombstone at "
            << tombstone_path_.value();
  return true;
}

void CobaltStartupTombstone::ProcessPriorRunTombstone() {
  base::AutoLock auto_lock(lock_);
  if (!has_prior_tombstone_) {
    base::UmaHistogramEnumeration("Cobalt.Startup.Tombstone.PriorRunStatus",
                                  PriorRunStatus::kCleanOrNone);
    return;
  }

  StartupTombstoneState prior_state =
      static_cast<StartupTombstoneState>(prior_header_.state);

  PriorRunStatus status = PriorRunStatus::kCleanOrNone;
  if (prior_state == StartupTombstoneState::kCleanShutdown) {
    status = PriorRunStatus::kCleanShutdown;
  } else if (prior_state == StartupTombstoneState::kStartupComplete) {
    // Reached complete startup in prior run before exiting
    status = PriorRunStatus::kCleanOrNone;
  } else if (prior_state == StartupTombstoneState::kCrashed) {
    status = PriorRunStatus::kCrashCaught;
  } else if (prior_state == StartupTombstoneState::kWatchdogTimeout) {
    status = PriorRunStatus::kWatchdogTimeout;
  } else {
    // Process terminated abruptly before startup completed (e.g. killed by LMK
    // or unhandled signal)
    status = PriorRunStatus::kIncompleteStartup;
  }

  base::UmaHistogramEnumeration("Cobalt.Startup.Tombstone.PriorRunStatus",
                                status);

  // If prior run was an incomplete startup or crash, record forensic histograms
  if (status == PriorRunStatus::kIncompleteStartup ||
      status == PriorRunStatus::kCrashCaught ||
      status == PriorRunStatus::kWatchdogTimeout) {
    // Find the highest milestone bit reached
    int highest_milestone = -1;
    for (int bit = 63; bit >= 0; --bit) {
      if ((prior_header_.milestone_bitmask >> bit) & 1ULL) {
        highest_milestone = bit;
        break;
      }
    }
    if (highest_milestone >= 0) {
      base::UmaHistogramExactLinear("Cobalt.Startup.Tombstone.LastMilestone",
                                    highest_milestone, 64);
    }

    // Record elapsed time before failure
    if (prior_header_.last_update_time_us >= prior_header_.start_time_us) {
      int64_t duration_ms =
          (prior_header_.last_update_time_us - prior_header_.start_time_us) /
          1000;
      base::UmaHistogramLongTimes(
          "Cobalt.Startup.Tombstone.DurationBeforeCrash",
          base::Milliseconds(duration_ms));
    }

    // Record crash signal if available
    if (prior_header_.crash_signal > 0) {
      base::UmaHistogramSparse("Cobalt.Startup.Tombstone.CrashSignal",
                               prior_header_.crash_signal);
    }

    LOG(WARNING)
        << "Startup Tombstone: Prior session crashed or died during startup! "
        << "Status=" << static_cast<int>(status)
        << ", Stage=" << prior_header_.last_stage_name
        << ", LastMilestone=" << highest_milestone;
  }
}

void CobaltStartupTombstone::SetMilestone(int milestone_id) {
  base::AutoLock auto_lock(lock_);
  if (!header_) {
    return;
  }
  if (milestone_id >= 0 && milestone_id < 64) {
    header_->milestone_bitmask |= (1ULL << milestone_id);
  }
  header_->last_update_time_us =
      base::TimeTicks::Now().since_origin().InMicroseconds();
  UpdateHeaderChecksumLocked();
}

void CobaltStartupTombstone::SetStage(StartupTombstoneState state,
                                      std::string_view stage_name) {
  base::AutoLock auto_lock(lock_);
  if (!header_) {
    return;
  }
  header_->state = static_cast<uint32_t>(state);
  header_->last_update_time_us =
      base::TimeTicks::Now().since_origin().InMicroseconds();
  size_t copy_len =
      std::min(stage_name.size(), sizeof(header_->last_stage_name) - 1);
  memcpy(header_->last_stage_name, stage_name.data(), copy_len);
  header_->last_stage_name[copy_len] = '\0';
  UpdateHeaderChecksumLocked();
}

void CobaltStartupTombstone::RecordCrash(int signal, std::string_view reason) {
  base::AutoLock auto_lock(lock_);
  if (!header_) {
    return;
  }
  header_->state = static_cast<uint32_t>(StartupTombstoneState::kCrashed);
  header_->crash_signal = static_cast<uint32_t>(signal);
  header_->last_update_time_us =
      base::TimeTicks::Now().since_origin().InMicroseconds();
  size_t copy_len = std::min(reason.size(), sizeof(header_->crash_reason) - 1);
  memcpy(header_->crash_reason, reason.data(), copy_len);
  header_->crash_reason[copy_len] = '\0';
  UpdateHeaderChecksumLocked();
}

void CobaltStartupTombstone::UpdatePmaStats(size_t used_bytes,
                                            size_t capacity_bytes) {
  base::AutoLock auto_lock(lock_);
  if (!header_) {
    return;
  }
  header_->pma_used_bytes = static_cast<uint32_t>(used_bytes);
  header_->pma_capacity_bytes = static_cast<uint32_t>(capacity_bytes);
  header_->last_update_time_us =
      base::TimeTicks::Now().since_origin().InMicroseconds();
  UpdateHeaderChecksumLocked();
}

void CobaltStartupTombstone::MarkStartupComplete() {
  base::AutoLock auto_lock(lock_);
  if (!header_) {
    return;
  }
  header_->state =
      static_cast<uint32_t>(StartupTombstoneState::kStartupComplete);
  header_->last_update_time_us =
      base::TimeTicks::Now().since_origin().InMicroseconds();
  strncpy(header_->last_stage_name, "StartupComplete",
          sizeof(header_->last_stage_name) - 1);
  UpdateHeaderChecksumLocked();
}

void CobaltStartupTombstone::MarkCleanShutdown() {
  base::AutoLock auto_lock(lock_);
  if (!header_) {
    return;
  }
  header_->state = static_cast<uint32_t>(StartupTombstoneState::kCleanShutdown);
  header_->last_update_time_us =
      base::TimeTicks::Now().since_origin().InMicroseconds();
  strncpy(header_->last_stage_name, "CleanShutdown",
          sizeof(header_->last_stage_name) - 1);
  UpdateHeaderChecksumLocked();
}

}  // namespace cobalt

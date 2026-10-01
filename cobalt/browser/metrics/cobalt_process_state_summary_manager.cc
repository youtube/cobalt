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

#include <cstring>

#include "base/hash/hash.h"

namespace cobalt {

CobaltProcessStateSummaryManager*
CobaltProcessStateSummaryManager::GetInstance() {
  static base::NoDestructor<CobaltProcessStateSummaryManager> instance;
  return instance.get();
}

CobaltProcessStateSummaryManager::CobaltProcessStateSummaryManager() = default;
CobaltProcessStateSummaryManager::~CobaltProcessStateSummaryManager() = default;

void CobaltProcessStateSummaryManager::SetStartupMilestone(int milestone) {
  if (milestone < 1 || milestone > 63) {
    return;
  }
  base::AutoLock auto_lock(lock_);
  current_data_.startup_milestones |= (1ULL << milestone);
  if (milestone > current_data_.highest_milestone) {
    current_data_.highest_milestone = static_cast<uint8_t>(milestone);
  }
  dirty_ = true;
  MaybeFlushSummaryLocked();
}

void CobaltProcessStateSummaryManager::SetStartupGuardArmed(bool armed) {
  base::AutoLock auto_lock(lock_);
  if (current_data_.startup_guard_armed == armed) {
    return;
  }
  current_data_.startup_guard_armed = armed;
  dirty_ = true;
  MaybeFlushSummaryLocked();
}

void CobaltProcessStateSummaryManager::FlushSummaryImmediate() {
  base::AutoLock auto_lock(lock_);
  if (!dirty_) {
    return;
  }
  std::vector<uint8_t> payload = Serialize(current_data_);
  PlatformSetProcessStateSummary(payload);
  last_flush_time_ = base::TimeTicks::Now();
  dirty_ = false;
}

void CobaltProcessStateSummaryManager::MaybeFlushSummaryLocked() {
  base::TimeTicks now = base::TimeTicks::Now();
  if (last_flush_time_.is_null() ||
      now - last_flush_time_ >= kMinFlushInterval) {
    std::vector<uint8_t> payload = Serialize(current_data_);
    PlatformSetProcessStateSummary(payload);
    last_flush_time_ = now;
    dirty_ = false;
  }
}

// static
std::vector<uint8_t> CobaltProcessStateSummaryManager::Serialize(
    const ProcessStateSummaryData& data) {
  std::vector<uint8_t> bytes(kSummaryPayloadSize, 0);
  bytes[0] = kMagicByte;
  bytes[1] = kVersionByte;

  uint8_t flags = 0;
  if (data.startup_guard_armed) {
    flags |= kFlagStartupGuardArmed;
  }
  if (data.startup_guard_triggered_kill) {
    flags |= kFlagStartupGuardTriggeredKill;
  }
  bytes[2] = flags;

  for (size_t i = 0; i < 8; ++i) {
    bytes[3 + i] =
        static_cast<uint8_t>((data.startup_milestones >> (i * 8)) & 0xFF);
  }
  bytes[11] = data.highest_milestone;

  uint32_t checksum =
      base::PersistentHash(base::span(bytes).first(kChecksumDataSize));
  bytes[12] = static_cast<uint8_t>(checksum & 0xFF);
  bytes[13] = static_cast<uint8_t>((checksum >> 8) & 0xFF);
  bytes[14] = static_cast<uint8_t>((checksum >> 16) & 0xFF);
  bytes[15] = static_cast<uint8_t>((checksum >> 24) & 0xFF);

  return bytes;
}

// static
std::optional<ProcessStateSummaryData>
CobaltProcessStateSummaryManager::Deserialize(base::span<const uint8_t> bytes) {
  if (bytes.size() < kSummaryPayloadSize) {
    return std::nullopt;
  }
  if (bytes[0] != kMagicByte || bytes[1] != kVersionByte) {
    return std::nullopt;
  }

  uint32_t expected_checksum =
      base::PersistentHash(bytes.first(kChecksumDataSize));
  uint32_t actual_checksum = static_cast<uint32_t>(bytes[12]) |
                             (static_cast<uint32_t>(bytes[13]) << 8) |
                             (static_cast<uint32_t>(bytes[14]) << 16) |
                             (static_cast<uint32_t>(bytes[15]) << 24);

  if (expected_checksum != actual_checksum) {
    return std::nullopt;
  }

  ProcessStateSummaryData data;
  data.startup_guard_armed = (bytes[2] & kFlagStartupGuardArmed) != 0;
  data.startup_guard_triggered_kill =
      (bytes[2] & kFlagStartupGuardTriggeredKill) != 0;

  uint64_t milestones = 0;
  for (size_t i = 0; i < 8; ++i) {
    milestones |= (static_cast<uint64_t>(bytes[3 + i]) << (i * 8));
  }
  data.startup_milestones = milestones;
  data.highest_milestone = bytes[11];

  return data;
}

std::optional<ProcessStateSummaryData>
CobaltProcessStateSummaryManager::GetPriorSessionSummary() {
  auto bytes = PlatformGetPriorSessionSummary();
  if (!bytes) {
    return std::nullopt;
  }
  return Deserialize(*bytes);
}

std::optional<ProcessStateSummaryData>
CobaltProcessStateSummaryManager::RecordLatestExitReasonAndGetSummary(
    const std::string& uma_name,
    int* out_exit_reason) {
  auto bytes =
      PlatformRecordLatestExitReasonAndGetSummary(uma_name, out_exit_reason);
  if (!bytes) {
    return std::nullopt;
  }
  return Deserialize(*bytes);
}

void CobaltProcessStateSummaryManager::ResetForTesting() {
  base::AutoLock auto_lock(lock_);
  current_data_ = ProcessStateSummaryData();
  last_flush_time_ = base::TimeTicks();
  dirty_ = false;
}

ProcessStateSummaryData
CobaltProcessStateSummaryManager::GetCurrentDataForTesting() {
  base::AutoLock auto_lock(lock_);
  return current_data_;
}

}  // namespace cobalt

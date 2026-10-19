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

#include <atomic>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/byte_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "cobalt/android/cobalt_process_state_summary_jni.h"
#include "cobalt/browser/features.h"

namespace cobalt {
namespace {

std::atomic<CobaltProcessStateSummaryManager*> g_instance_for_testing{nullptr};

// Early startup milestones (e.g., BasicStartupComplete) are recorded before
// base::FeatureList::SetInstance() registers the FeatureList. We must check
// base::FeatureList::GetInstance() first because querying a Feature or
// FeatureParam before FeatureList registration triggers a fatal CHECK in
// Chromium's EarlyFeatureAccessTracker when FeatureList::SetInstance() runs.
bool IsProcessStateSummaryEnabled() {
  return !base::FeatureList::GetInstance() ||
         base::FeatureList::IsEnabled(features::kCobaltProcessStateSummary);
}

}  // namespace

CobaltProcessStateSummaryManager*
CobaltProcessStateSummaryManager::GetInstance() {
  if (auto* instance = g_instance_for_testing.load(std::memory_order_acquire)) {
    return instance;
  }
  static base::NoDestructor<CobaltProcessStateSummaryManager> instance;
  return instance.get();
}

// static
void CobaltProcessStateSummaryManager::SetInstanceForTesting(
    CobaltProcessStateSummaryManager* instance) {
  g_instance_for_testing.store(instance, std::memory_order_release);
}

CobaltProcessStateSummaryManager::CobaltProcessStateSummaryManager() = default;
CobaltProcessStateSummaryManager::~CobaltProcessStateSummaryManager() = default;

void CobaltProcessStateSummaryManager::SetStartupMilestone(int milestone) {
  if (milestone < kMinStartupMilestone || milestone > kMaxStartupMilestone ||
      !IsProcessStateSummaryEnabled()) {
    return;
  }
  {
    base::AutoLock auto_lock(lock_);
    const uint64_t bit = 1ULL << milestone;
    if ((current_data_.startup_milestones & bit) != 0) {
      return;
    }
    current_data_.startup_milestones |= bit;
    if (milestone > current_data_.highest_milestone) {
      current_data_.highest_milestone = static_cast<uint8_t>(milestone);
    }
    dirty_ = true;
  }
  MaybeFlush();
}

void CobaltProcessStateSummaryManager::SetStartupGuardArmed(bool armed) {
  if (!IsProcessStateSummaryEnabled()) {
    return;
  }
  {
    base::AutoLock auto_lock(lock_);
    if (current_data_.startup_guard_armed == armed) {
      return;
    }
    current_data_.startup_guard_armed = armed;
    dirty_ = true;
    pending_immediate_flush_ = true;
  }
  MaybeFlush();
}

void CobaltProcessStateSummaryManager::FlushSummaryImmediate() {
  if (!IsProcessStateSummaryEnabled()) {
    return;
  }
  {
    base::AutoLock auto_lock(lock_);
    if (!dirty_) {
      return;
    }
    pending_immediate_flush_ = true;
  }
  MaybeFlush();
}

base::TimeDelta
CobaltProcessStateSummaryManager::GetRemainingFlushCooldownLocked(
    base::TimeTicks now) const {
  if (last_flush_time_.is_null()) {
    return base::TimeDelta();
  }
  // Guard against querying FeatureParam::Get() before FeatureList registration
  // during early startup (see IsProcessStateSummaryEnabled() comment above).
  base::TimeDelta min_flush_interval =
      base::FeatureList::GetInstance()
          ? features::kProcessStateSummaryMinFlushIntervalParam.Get()
          : features::kProcessStateSummaryMinFlushIntervalParam.default_value;
  base::TimeDelta elapsed = now - last_flush_time_;
  if (elapsed >= min_flush_interval) {
    return base::TimeDelta();
  }
  return min_flush_interval - elapsed;
}

bool CobaltProcessStateSummaryManager::IsFlushCooldownExpiredLocked(
    base::TimeTicks now) const {
  return GetRemainingFlushCooldownLocked(now).is_zero();
}

bool CobaltProcessStateSummaryManager::ShouldFlushLocked(
    base::TimeTicks now) const {
  if (!dirty_ || flush_in_progress_) {
    return false;
  }
  return pending_immediate_flush_ || IsFlushCooldownExpiredLocked(now);
}

std::optional<std::vector<uint8_t>>
CobaltProcessStateSummaryManager::ClaimNextFlushPayload() {
  base::AutoLock auto_lock(lock_);
  base::TimeTicks now = base::TimeTicks::Now();
  if (!ShouldFlushLocked(now)) {
    return std::nullopt;
  }
  flush_in_progress_ = true;
  dirty_ = false;
  pending_immediate_flush_ = false;
  last_flush_time_ = now;
  return Serialize(current_data_);
}

void CobaltProcessStateSummaryManager::CompleteCurrentFlush() {
  base::AutoLock auto_lock(lock_);
  flush_in_progress_ = false;
}

void CobaltProcessStateSummaryManager::MaybeScheduleDelayedFlush() {
  if (!base::SequencedTaskRunner::HasCurrentDefault()) {
    return;
  }
  base::TimeDelta delay;
  uint64_t generation = 0;
  {
    base::AutoLock auto_lock(lock_);
    if (!dirty_ || flush_in_progress_ || delayed_flush_scheduled_) {
      return;
    }
    delay = GetRemainingFlushCooldownLocked(base::TimeTicks::Now());
    if (delay.is_zero()) {
      return;
    }
    delayed_flush_scheduled_ = true;
    generation = flush_generation_;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &CobaltProcessStateSummaryManager::RunScheduledDelayedFlush,
          generation),
      delay);
}

// static
void CobaltProcessStateSummaryManager::RunScheduledDelayedFlush(
    uint64_t generation) {
  CobaltProcessStateSummaryManager::GetInstance()->OnDelayedFlushFired(
      generation);
}

void CobaltProcessStateSummaryManager::OnDelayedFlushFired(
    uint64_t generation) {
  {
    base::AutoLock auto_lock(lock_);
    if (generation != flush_generation_) {
      return;
    }
    delayed_flush_scheduled_ = false;
  }
  MaybeFlush();
}

void CobaltProcessStateSummaryManager::FlushPayloadToPlatform(
    base::span<const uint8_t> payload) {
  // Perform OS JNI/Binder IPC outside `lock_` so callers on other threads
  // never block on IPC latency and cannot cause lock-order inversions.
  const base::TimeTicks flush_start = base::TimeTicks::Now();
  android::SetProcessStateSummary(payload);
  base::UmaHistogramMicrosecondsTimes(
      "Cobalt.Stability.ProcessStateSummary.FlushDurationUs",
      base::TimeTicks::Now() - flush_start);
}

void CobaltProcessStateSummaryManager::MaybeFlush() {
  while (std::optional<std::vector<uint8_t>> payload =
             ClaimNextFlushPayload()) {
    FlushPayloadToPlatform(*payload);
    CompleteCurrentFlush();
  }
  MaybeScheduleDelayedFlush();
}

// static
std::vector<uint8_t> CobaltProcessStateSummaryManager::Serialize(
    const ProcessStateSummaryData& data) {
  std::vector<uint8_t> bytes(kSummaryPayloadSize, 0);
  auto span = base::span(bytes);
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

  span.subspan<3, 8>().copy_from(
      base::U64ToLittleEndian(data.startup_milestones));
  bytes[11] = data.highest_milestone;

  uint32_t checksum = base::PersistentHash(span.first<kChecksumDataSize>());
  span.subspan<12, 4>().copy_from(base::U32ToLittleEndian(checksum));

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
      base::PersistentHash(bytes.first<kChecksumDataSize>());
  uint32_t actual_checksum = base::U32FromLittleEndian(bytes.subspan<12, 4>());

  if (expected_checksum != actual_checksum) {
    return std::nullopt;
  }

  ProcessStateSummaryData data;
  data.startup_guard_armed = (bytes[2] & kFlagStartupGuardArmed) != 0;
  data.startup_guard_triggered_kill =
      (bytes[2] & kFlagStartupGuardTriggeredKill) != 0;
  data.startup_milestones = base::U64FromLittleEndian(bytes.subspan<3, 8>());
  data.highest_milestone = bytes[11];

  return data;
}

std::optional<ProcessStateSummaryData>
CobaltProcessStateSummaryManager::GetPriorSessionSummary(int* out_exit_reason) {
  if (out_exit_reason) {
    *out_exit_reason = -1;
  }
  if (!IsProcessStateSummaryEnabled()) {
    return std::nullopt;
  }
  const base::TimeTicks read_start = base::TimeTicks::Now();
  auto bytes = android::GetPriorSessionProcessStateSummary(out_exit_reason);
  base::UmaHistogramMicrosecondsTimes(
      "Cobalt.Stability.ProcessStateSummary.PriorSessionReadDurationUs",
      base::TimeTicks::Now() - read_start);
  if (!bytes) {
    return std::nullopt;
  }
  return Deserialize(*bytes);
}

void CobaltProcessStateSummaryManager::ResetForTesting() {
  base::AutoLock auto_lock(lock_);
  current_data_ = ProcessStateSummaryData();
  last_flush_time_ = base::TimeTicks();
  ++flush_generation_;
  dirty_ = false;
  flush_in_progress_ = false;
  pending_immediate_flush_ = false;
  delayed_flush_scheduled_ = false;
}

ProcessStateSummaryData
CobaltProcessStateSummaryManager::GetCurrentDataForTesting() {
  base::AutoLock auto_lock(lock_);
  return current_data_;
}

bool CobaltProcessStateSummaryManager::IsDirtyForTesting() {
  base::AutoLock auto_lock(lock_);
  return dirty_;
}

namespace android {

void SetStartupMilestoneFromJava(int milestone) {
  CobaltProcessStateSummaryManager::GetInstance()->SetStartupMilestone(
      milestone);
}

void SetStartupGuardArmedFromJava(bool armed) {
  CobaltProcessStateSummaryManager::GetInstance()->SetStartupGuardArmed(armed);
}

}  // namespace android

}  // namespace cobalt

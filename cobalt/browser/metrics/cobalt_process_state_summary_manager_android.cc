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
#include <optional>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "cobalt/android/jni_headers/CobaltProcessStateSummary_jni.h"

namespace cobalt {

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
      payload = PreparePayloadLocked();
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
    payload = PreparePayloadLocked();
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
    payload = PreparePayloadLocked();
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
    payload = PreparePayloadLocked();
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
    payload = PreparePayloadLocked();
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
    payload = PreparePayloadLocked();
  }

  if (payload) {
    SyncToSystemServer(*payload);
  }
}

std::optional<std::vector<uint8_t>>
CobaltProcessStateSummaryManager::PreparePayloadLocked() {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    return std::nullopt;
  }

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

void CobaltProcessStateSummaryManager::SyncToSystemServer(
    const std::vector<uint8_t>& payload) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> java_array =
      base::android::ToJavaByteArray(env, payload.data(), payload.size());

  Java_CobaltProcessStateSummary_setProcessStateSummary(env, java_array);
}

std::optional<ProcessStateSnapshot>
CobaltProcessStateSummaryManager::ReadPriorSessionSnapshot() {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    return std::nullopt;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> java_bytes =
      Java_CobaltProcessStateSummary_getPriorSessionProcessStateSummary(env);

  if (!java_bytes) {
    return std::nullopt;
  }

  std::vector<uint8_t> buffer;
  base::android::JavaByteArrayToByteVector(env, java_bytes, &buffer);

  return DeserializeSnapshot(buffer);
}

int CobaltProcessStateSummaryManager::RecordLatestExitReasonToUma(
    const std::string& uma_name) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    return -1;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  int exit_reason = Java_CobaltProcessStateSummary_recordLatestExitReasonToUma(
      env, base::android::ConvertUTF8ToJavaString(env, uma_name));
  if (exit_reason >= 0 && !uma_name.empty()) {
    int sample =
        std::min(exit_reason, static_cast<int>(AndroidExitReason::kMaxValue));
    base::UmaHistogramExactLinear(
        uma_name, sample, static_cast<int>(AndroidExitReason::kMaxValue) + 1);
  }
  return exit_reason;
}

std::optional<ProcessStateSnapshot> CobaltProcessStateSummaryManager::
    RecordLatestExitReasonAndGetPriorSessionSnapshot(
        const std::string& uma_name,
        int* out_exit_reason) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    if (out_exit_reason) {
      *out_exit_reason = -1;
    }
    return std::nullopt;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<int> exit_reason_vec = {-1};
  base::android::ScopedJavaLocalRef<jintArray> out_array =
      base::android::ToJavaIntArray(env, exit_reason_vec);

  base::android::ScopedJavaLocalRef<jbyteArray> java_bytes =
      Java_CobaltProcessStateSummary_recordLatestExitReasonAndGetSummary(
          env, base::android::ConvertUTF8ToJavaString(env, uma_name),
          out_array);

  int exit_reason = -1;
  std::vector<int> c_array;
  base::android::JavaIntArrayToIntVector(env, out_array, &c_array);
  if (!c_array.empty()) {
    exit_reason = c_array[0];
  }
  if (out_exit_reason) {
    *out_exit_reason = exit_reason;
  }
  if (exit_reason >= 0 && !uma_name.empty()) {
    int sample =
        std::min(exit_reason, static_cast<int>(AndroidExitReason::kMaxValue));
    base::UmaHistogramExactLinear(
        uma_name, sample, static_cast<int>(AndroidExitReason::kMaxValue) + 1);
  }

  if (!java_bytes) {
    return std::nullopt;
  }

  std::vector<uint8_t> buffer;
  base::android::JavaByteArrayToByteVector(env, java_bytes, &buffer);

  return DeserializeSnapshot(buffer);
}

}  // namespace cobalt

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
#include <vector>

#include "base/logging.h"
#include "build/build_config.h"
#include "components/crash/content/browser/process_exit_reason_from_system_android.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "cobalt/android/jni_headers/CobaltProcessStateSummary_jni.h"
#endif

namespace cobalt {

std::string_view ExitReasonToHistogramSuffix(int exit_reason) {
  // Matches Android ApplicationExitInfo reason codes mapped to UMA enum labels
  switch (exit_reason) {
    case 0:
      return "Anr";
    case 1:
      return "Crash";
    case 2:
      return "CrashNative";
    case 3:
      return "DependencyDied";
    case 4:
      return "ExcessiveResourceUsage";
    case 5:
      return "ExitSelf";
    case 6:
      return "InitializationFailure";
    case 7:
      return "LowMemory";
    case 8:
      return "Other";
    case 9:
      return "PermissionChange";
    case 10:
      return "Signaled";
    case 11:
      return "Unknown";
    case 12:
      return "UserRequested";
    case 13:
      return "UserStopped";
    case 14:
      return "ApiFailed";
    case 15:
      return "Freezer";
    case 16:
      return "PackageStateChange";
    case 17:
      return "PackageUpdated";
    default:
      return "Other";
  }
}

CobaltProcessStateSummaryManager*
CobaltProcessStateSummaryManager::GetInstance() {
  static base::NoDestructor<CobaltProcessStateSummaryManager> instance;
  return instance.get();
}

CobaltProcessStateSummaryManager::CobaltProcessStateSummaryManager()
    : start_ticks_(base::TimeTicks::Now()) {}

void CobaltProcessStateSummaryManager::UpdateMemoryFootprint(
    uint32_t current_rss_kb,
    uint32_t current_pmf_kb,
    uint32_t current_v8_code_kb) {
#if BUILDFLAG(IS_ANDROID)
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
    dirty_ = true;
    SyncToSystemServerLocked();
  }
#endif
}

void CobaltProcessStateSummaryManager::UpdateTrimMemoryLevel(uint8_t level) {
#if BUILDFLAG(IS_ANDROID)
  base::AutoLock auto_lock(lock_);
  last_trim_level_ = level;
  dirty_ = true;
  SyncToSystemServerLocked();
#endif
}

void CobaltProcessStateSummaryManager::SetFlags(uint8_t flags) {
#if BUILDFLAG(IS_ANDROID)
  base::AutoLock auto_lock(lock_);
  flags_ = flags;
  dirty_ = true;
  SyncToSystemServerLocked();
#endif
}

void CobaltProcessStateSummaryManager::SyncToSystemServerLocked() {
#if BUILDFLAG(IS_ANDROID)
  if (!dirty_) {
    return;
  }

  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    dirty_ = false;
    return;
  }

  uint32_t uptime_sec = static_cast<uint32_t>(
      (base::TimeTicks::Now() - start_ticks_).InSeconds());

  ProcessStateSnapshot snapshot;
  snapshot.last_trim_level = last_trim_level_;
  snapshot.flags = flags_;
  snapshot.peak_rss_kb = peak_rss_kb_;
  snapshot.peak_pmf_kb = peak_pmf_kb_;
  snapshot.peak_v8_code_kb = peak_v8_code_kb_;
  snapshot.uptime_sec = uptime_sec;

  std::vector<uint8_t> payload = SerializeSnapshot(snapshot);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> java_array =
      base::android::ToJavaByteArray(env, payload.data(), payload.size());

  Java_CobaltProcessStateSummary_setProcessStateSummary(env, java_array);
  dirty_ = false;
#endif
}

std::optional<ProcessStateSnapshot>
CobaltProcessStateSummaryManager::ReadPriorSessionSnapshot(
    base::ProcessId prior_pid) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    return std::nullopt;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> java_bytes =
      Java_CobaltProcessStateSummary_getPriorSessionProcessStateSummary(
          env, prior_pid);

  if (!java_bytes) {
    return std::nullopt;
  }

  std::vector<uint8_t> buffer;
  base::android::JavaByteArrayToByteVector(env, java_bytes, &buffer);

  return DeserializeSnapshot(buffer);
#else
  return std::nullopt;
#endif
}

int CobaltProcessStateSummaryManager::GetPriorSessionExitReason(
    base::ProcessId prior_pid) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    return -1;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_CobaltProcessStateSummary_getPriorSessionExitReason(env,
                                                                  prior_pid);
#else
  return -1;
#endif
}

// static
std::vector<uint8_t> CobaltProcessStateSummaryManager::SerializeSnapshot(
    const ProcessStateSnapshot& snapshot) {
  std::vector<uint8_t> buffer(kProcessStateSummaryPayloadSize, 0);
  buffer[0] = kProcessStateSummaryMagic;
  buffer[1] = kProcessStateSummaryVersion;
  buffer[2] = snapshot.last_trim_level;
  buffer[3] = snapshot.flags;

  // Little-endian serialization of 32-bit integers.
  buffer[4] = static_cast<uint8_t>(snapshot.peak_rss_kb & 0xFF);
  buffer[5] = static_cast<uint8_t>((snapshot.peak_rss_kb >> 8) & 0xFF);
  buffer[6] = static_cast<uint8_t>((snapshot.peak_rss_kb >> 16) & 0xFF);
  buffer[7] = static_cast<uint8_t>((snapshot.peak_rss_kb >> 24) & 0xFF);

  buffer[8] = static_cast<uint8_t>(snapshot.peak_pmf_kb & 0xFF);
  buffer[9] = static_cast<uint8_t>((snapshot.peak_pmf_kb >> 8) & 0xFF);
  buffer[10] = static_cast<uint8_t>((snapshot.peak_pmf_kb >> 16) & 0xFF);
  buffer[11] = static_cast<uint8_t>((snapshot.peak_pmf_kb >> 24) & 0xFF);

  buffer[12] = static_cast<uint8_t>(snapshot.peak_v8_code_kb & 0xFF);
  buffer[13] = static_cast<uint8_t>((snapshot.peak_v8_code_kb >> 8) & 0xFF);
  buffer[14] = static_cast<uint8_t>((snapshot.peak_v8_code_kb >> 16) & 0xFF);
  buffer[15] = static_cast<uint8_t>((snapshot.peak_v8_code_kb >> 24) & 0xFF);

  buffer[16] = static_cast<uint8_t>(snapshot.uptime_sec & 0xFF);
  buffer[17] = static_cast<uint8_t>((snapshot.uptime_sec >> 8) & 0xFF);
  buffer[18] = static_cast<uint8_t>((snapshot.uptime_sec >> 16) & 0xFF);
  buffer[19] = static_cast<uint8_t>((snapshot.uptime_sec >> 24) & 0xFF);

  return buffer;
}

// static
std::optional<ProcessStateSnapshot>
CobaltProcessStateSummaryManager::DeserializeSnapshot(
    base::span<const uint8_t> buffer) {
  if (buffer.size() < kProcessStateSummaryPayloadSize) {
    LOG(WARNING) << "Truncated process state summary payload: size="
                 << buffer.size();
    return std::nullopt;
  }

  if (buffer[0] != kProcessStateSummaryMagic ||
      buffer[1] != kProcessStateSummaryVersion) {
    LOG(WARNING) << "Invalid process state summary header: magic="
                 << static_cast<int>(buffer[0])
                 << ", version=" << static_cast<int>(buffer[1]);
    return std::nullopt;
  }

  ProcessStateSnapshot snapshot;
  snapshot.last_trim_level = buffer[2];
  snapshot.flags = buffer[3];

  snapshot.peak_rss_kb = static_cast<uint32_t>(buffer[4]) |
                         (static_cast<uint32_t>(buffer[5]) << 8) |
                         (static_cast<uint32_t>(buffer[6]) << 16) |
                         (static_cast<uint32_t>(buffer[7]) << 24);

  snapshot.peak_pmf_kb = static_cast<uint32_t>(buffer[8]) |
                         (static_cast<uint32_t>(buffer[9]) << 8) |
                         (static_cast<uint32_t>(buffer[10]) << 16) |
                         (static_cast<uint32_t>(buffer[11]) << 24);

  snapshot.peak_v8_code_kb = static_cast<uint32_t>(buffer[12]) |
                             (static_cast<uint32_t>(buffer[13]) << 8) |
                             (static_cast<uint32_t>(buffer[14]) << 16) |
                             (static_cast<uint32_t>(buffer[15]) << 24);

  snapshot.uptime_sec = static_cast<uint32_t>(buffer[16]) |
                        (static_cast<uint32_t>(buffer[17]) << 8) |
                        (static_cast<uint32_t>(buffer[18]) << 16) |
                        (static_cast<uint32_t>(buffer[19]) << 24);

  return snapshot;
}

void CobaltProcessStateSummaryManager::ResetForTesting() {
  base::AutoLock auto_lock(lock_);
  start_ticks_ = base::TimeTicks::Now();
  peak_rss_kb_ = 0;
  peak_pmf_kb_ = 0;
  peak_v8_code_kb_ = 0;
  last_trim_level_ = 0;
  flags_ = 0;
  dirty_ = false;
}

}  // namespace cobalt

namespace crash_reporter {
#if BUILDFLAG(IS_COBALT)
int ProcessExitReasonFromSystem::GetExitReason(base::ProcessId pid) {
  return cobalt::CobaltProcessStateSummaryManager::GetInstance()
      ->GetPriorSessionExitReason(pid);
}
#endif
}  // namespace crash_reporter

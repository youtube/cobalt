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
#include <vector>

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "cobalt/android/jni_headers/CobaltProcessStateSummary_jni.h"
#endif

namespace cobalt {

std::string_view ExitReasonToHistogramSuffix(int exit_reason) {
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
  std::vector<uint8_t> payload;
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
      dirty_ = true;
      if (base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_R) {
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
        payload = SerializeSnapshot(snapshot);
        dirty_ = false;
      }
    }
  }

  if (!payload.empty()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jbyteArray> java_array =
        base::android::ToJavaByteArray(env, payload.data(), payload.size());
    Java_CobaltProcessStateSummary_setProcessStateSummary(env, java_array);
  }
#endif
}

void CobaltProcessStateSummaryManager::UpdateTrimMemoryLevel(uint8_t level) {
#if BUILDFLAG(IS_ANDROID)
  std::vector<uint8_t> payload;
  {
    base::AutoLock auto_lock(lock_);
    last_trim_level_ = level;
    dirty_ = true;
    if (base::android::BuildInfo::GetInstance()->sdk_int() >=
        base::android::SDK_VERSION_R) {
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
      payload = SerializeSnapshot(snapshot);
      dirty_ = false;
    }
  }

  if (!payload.empty()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jbyteArray> java_array =
        base::android::ToJavaByteArray(env, payload.data(), payload.size());
    Java_CobaltProcessStateSummary_setProcessStateSummary(env, java_array);
  }
#endif
}

void CobaltProcessStateSummaryManager::SetFlags(uint8_t flags) {
#if BUILDFLAG(IS_ANDROID)
  std::vector<uint8_t> payload;
  {
    base::AutoLock auto_lock(lock_);
    flags_ = flags;
    dirty_ = true;
    if (base::android::BuildInfo::GetInstance()->sdk_int() >=
        base::android::SDK_VERSION_R) {
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
      payload = SerializeSnapshot(snapshot);
      dirty_ = false;
    }
  }

  if (!payload.empty()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jbyteArray> java_array =
        base::android::ToJavaByteArray(env, payload.data(), payload.size());
    Java_CobaltProcessStateSummary_setProcessStateSummary(env, java_array);
  }
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

  std::vector<uint8_t> payload = SerializeSnapshot(snapshot);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> java_array =
      base::android::ToJavaByteArray(env, payload.data(), payload.size());

  Java_CobaltProcessStateSummary_setProcessStateSummary(env, java_array);
  dirty_ = false;
#endif
}

std::optional<ProcessStateSnapshot>
CobaltProcessStateSummaryManager::ReadPriorSessionSnapshot() {
#if BUILDFLAG(IS_ANDROID)
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
#else
  return std::nullopt;
#endif
}

int CobaltProcessStateSummaryManager::RecordLatestExitReasonToUma(
    const std::string& uma_name) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    return -1;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  int exit_reason = Java_CobaltProcessStateSummary_recordLatestExitReasonToUma(
      env, base::android::ConvertUTF8ToJavaString(env, uma_name));
  if (exit_reason >= 0 && !uma_name.empty()) {
    base::UmaHistogramExactLinear(uma_name, exit_reason, 18);
  }
  return exit_reason;
#else
  return -1;
#endif
}

std::optional<ProcessStateSnapshot> CobaltProcessStateSummaryManager::
    RecordLatestExitReasonAndGetPriorSessionSnapshot(
        const std::string& uma_name,
        int* out_exit_reason) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    return std::nullopt;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jintArray> out_array;
  if (out_exit_reason) {
    out_array = base::android::ToJavaIntArray(env, std::vector<int>{-1});
  }

  base::android::ScopedJavaLocalRef<jbyteArray> java_bytes =
      Java_CobaltProcessStateSummary_recordLatestExitReasonAndGetSummary(
          env, base::android::ConvertUTF8ToJavaString(env, uma_name),
          out_array);

  int exit_reason = -1;
  if (out_array) {
    std::vector<int> c_array;
    base::android::JavaIntArrayToIntVector(env, out_array, &c_array);
    if (!c_array.empty()) {
      exit_reason = c_array[0];
    }
  }
  if (out_exit_reason) {
    *out_exit_reason = exit_reason;
  }
  if (exit_reason >= 0 && !uma_name.empty()) {
    base::UmaHistogramExactLinear(uma_name, exit_reason, 18);
  }

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

// static
std::vector<uint8_t> CobaltProcessStateSummaryManager::SerializeSnapshot(
    const ProcessStateSnapshot& snapshot) {
  std::vector<uint8_t> buffer(kProcessStateSummaryPayloadSize, 0);
  buffer[0] = kProcessStateSummaryMagic;
  buffer[1] = kProcessStateSummaryVersion;
  buffer[2] = snapshot.last_trim_level;
  buffer[3] = snapshot.flags;

  // Little-endian serialization of 32-bit integers.
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

  // Compute 32-bit PersistentHash checksum over bytes [0..23].
  uint32_t checksum = base::PersistentHash(base::span(buffer).first(24u));
  buffer[24] = static_cast<uint8_t>(checksum & 0xFF);
  buffer[25] = static_cast<uint8_t>((checksum >> 8) & 0xFF);
  buffer[26] = static_cast<uint8_t>((checksum >> 16) & 0xFF);
  buffer[27] = static_cast<uint8_t>((checksum >> 24) & 0xFF);

  return buffer;
}

// static
std::optional<ProcessStateSnapshot>
CobaltProcessStateSummaryManager::DeserializeSnapshot(
    base::span<const uint8_t> buffer) {
  // Layer 1: Payload size check.
  if (buffer.size() != kProcessStateSummaryPayloadSize) {
    LOG(WARNING) << "Invalid process state summary payload size: expected "
                 << kProcessStateSummaryPayloadSize << ", got "
                 << buffer.size();
    return std::nullopt;
  }

  // Layer 2: Magic and version check.
  if (buffer[0] != kProcessStateSummaryMagic ||
      buffer[1] != kProcessStateSummaryVersion) {
    LOG(WARNING) << "Invalid process state summary header: magic="
                 << static_cast<int>(buffer[0])
                 << ", version=" << static_cast<int>(buffer[1]);
    return std::nullopt;
  }

  // Layer 3: Checksum verification over bytes [0..23].
  uint32_t stored_checksum = static_cast<uint32_t>(buffer[24]) |
                             (static_cast<uint32_t>(buffer[25]) << 8) |
                             (static_cast<uint32_t>(buffer[26]) << 16) |
                             (static_cast<uint32_t>(buffer[27]) << 24);
  uint32_t computed_checksum = base::PersistentHash(buffer.first(24u));
  if (stored_checksum != computed_checksum) {
    LOG(WARNING) << "Process state summary checksum mismatch: stored="
                 << stored_checksum << ", computed=" << computed_checksum;
    return std::nullopt;
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

  if ((snapshot.flags & ~kProcessStateKnownFlagsMask) != 0) {
    LOG(WARNING) << "Process state summary contains unknown flag bits: "
                 << static_cast<int>(snapshot.flags);
    return std::nullopt;
  }

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

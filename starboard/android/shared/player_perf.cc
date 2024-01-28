// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/player_perf.h"

#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "starboard/android/shared/player_components_factory.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/time.h"
#include "starboard/configuration.h"
#include "starboard/extension/player_perf.h"
#include "starboard/file.h"
#include "starboard/media.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

// Definitions of any functions included as components in the extension
// are added here.
const char kProcDir[] = "/proc/";
const char kStatFile[] = "/stat";
const char kProcTaskDir[] = "/proc/self/task/";
const int64_t kMaxFileSizeBytes = 512;

std::unordered_map<std::string, std::unordered_map<int32_t, int64_t>>
    map_thread_ids;

Mutex mutex_;

// Counting the times of should_be_paused
int should_be_paused_count_ = 0;

// Used to store the previous times and CPU usage counts so we can
// compute the CPU usage between calls.
int64_t last_cpu_time_;
int64_t last_cumulative_cpu_ = 0;

AudioTrackBridge* audio_track_bridge_ = nullptr;
SbMediaVideoCodec media_video_codec_ = kSbMediaVideoCodecNone;
SbMediaAudioCodec media_audio_codec_ = kSbMediaAudioCodecNone;

// Fields from /proc/<pid>/stat, 0-based. See man 5 proc.
// If the ordering ever changes, carefully review functions that use these
// values.
enum ProcStatsFields {
  VM_COMM = 1,         // Filename of executable, without parentheses.
  VM_STATE = 2,        // Letter indicating the state of the process.
  VM_PPID = 3,         // PID of the parent.
  VM_PGRP = 4,         // Process group id.
  VM_MINFLT = 9,       // Minor page fault count excluding children.
  VM_MAJFLT = 11,      // Major page fault count excluding children.
  VM_UTIME = 13,       // Time scheduled in user mode in clock ticks.
  VM_STIME = 14,       // Time scheduled in kernel mode in clock ticks.
  VM_NUMTHREADS = 19,  // Number of threads.
  VM_STARTTIME = 21,   // The time the process started in clock ticks.
  VM_VSIZE = 22,       // Virtual memory size in bytes.
  VM_RSS = 23,         // Resident Set Size in pages.
};

std::string GetProcPidDir(pid_t pid) {
  std::string proc_pid_dir(kProcDir);
  proc_pid_dir.append(std::to_string(pid));
  return proc_pid_dir;
}

std::string GetProcTaskDir(int32_t thread_id) {
  std::string proc_task_dir(kProcTaskDir);
  proc_task_dir.append(std::to_string(thread_id));
  return proc_task_dir;
}

bool ReadProcFile(const std::string& file, std::string* buffer) {
  SbFile sb_file =
      SbFileOpen(file.c_str(), kSbFileOpenOnly | kSbFileRead, nullptr, nullptr);
  if (!SbFileIsValid(sb_file)) {
    SB_DLOG(WARNING) << "Failed to read " << file.c_str();
    return false;
  }
  int bytes_read = SbFileRead(sb_file, reinterpret_cast<char*>(buffer->data()),
                              kMaxFileSizeBytes);
  buffer->data()[bytes_read - 1] = '\0';
  if (bytes_read < kMaxFileSizeBytes) {
    buffer->erase(bytes_read, kMaxFileSizeBytes - bytes_read);
  }
  SbFileClose(sb_file);
  return !buffer->empty();
}

bool ReadProcStats(std::string& stat_file, std::string* buffer) {
  return ReadProcFile(stat_file, buffer);
}

bool ParseProcStats(const std::string& stats_data,
                    std::vector<std::string>* proc_stats) {
  // |stats_data| may be empty if the process disappeared somehow.
  // e.g. http://crbug.com/145811
  if (stats_data.empty())
    return false;

  // The stat file is formatted as:
  // pid (process name) data1 data2 .... dataN
  // Look for the closing paren by scanning backwards, to avoid being fooled by
  // processes with ')' in the name.
  size_t open_parens_idx = stats_data.find(" (");
  size_t close_parens_idx = stats_data.rfind(") ");
  if (open_parens_idx == std::string::npos ||
      close_parens_idx == std::string::npos ||
      open_parens_idx > close_parens_idx) {
    SB_DLOG(WARNING) << "Failed to find matched parens in '" << stats_data
                     << "'";
    SB_NOTREACHED();
    return false;
  }
  open_parens_idx++;

  proc_stats->clear();
  // PID.
  proc_stats->push_back(stats_data.substr(0, open_parens_idx));
  // Process name without parentheses.
  proc_stats->push_back(stats_data.substr(
      open_parens_idx + 1, close_parens_idx - (open_parens_idx + 1)));

  // Split the rest.
  std::string other_stats = stats_data.substr(close_parens_idx + 2);
  std::string delimiter = " ";
  size_t pos = 0;
  std::string token;
  while ((pos = other_stats.find(delimiter)) != std::string::npos) {
    token = other_stats.substr(0, pos);
    proc_stats->push_back(token);
    other_stats.erase(0, pos + delimiter.length());
  }
  proc_stats->push_back(other_stats);
  return true;
}

int64_t GetProcStatsFieldAsInt64(const std::vector<std::string>& proc_stats,
                                 ProcStatsFields field_num) {
  SB_DCHECK(field_num > VM_PPID);
  SB_CHECK(static_cast<size_t>(field_num) <= proc_stats.size());

  int64_t value = static_cast<int64_t>(std::stoll(proc_stats[field_num]));
  return value;
}

// Get the total CPU of a single process.  Return value is number of jiffies
// on success or -1 on error.
int64_t GetProcessCPU(std::string& stat_file) {
  std::string buffer(kMaxFileSizeBytes, ' ');
  std::vector<std::string> proc_stats;
  if (!ReadProcStats(stat_file, &buffer) ||
      !ParseProcStats(buffer, &proc_stats)) {
    return -1;
  }

  int64_t total_cpu = GetProcStatsFieldAsInt64(proc_stats, VM_UTIME) +
                      GetProcStatsFieldAsInt64(proc_stats, VM_STIME);
  return total_cpu;
}

int64_t ClockTicksToTimeDelta(int64_t clock_ticks) {
  // This queries the /proc-specific scaling factor which is
  // conceptually the system hertz.  To dump this value on another
  // system, try
  //   od -t dL /proc/self/auxv
  // and look for the number after 17 in the output; mine is
  //   0000040          17         100           3   134512692
  // which means the answer is 100.
  // It may be the case that this value is always 100.
  static const int64_t kHertz = sysconf(_SC_CLK_TCK);

  return 1'000'000 * clock_ticks / kHertz;
}

int64_t GetCumulativeCPUUsage(std::string& stat_file) {
  return ClockTicksToTimeDelta(GetProcessCPU(stat_file));
}

std::string exec(const char* cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    SB_LOG(ERROR) << "popen() failed!";
    return result;
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

std::unordered_map<std::string, double> GetPlatformIndependentCPUUsage() {
  std::unordered_map<std::string, double> map_cumulative_cpu;
  std::string pid_stat_file = GetProcPidDir(getpid()).append(kStatFile);
  int64_t cumulative_cpu = GetCumulativeCPUUsage(pid_stat_file);
  int64_t time = CurrentPosixTime();

  if (last_cumulative_cpu_ == 0) {
    // First call, just set the last values.
    last_cumulative_cpu_ = cumulative_cpu;
    last_cpu_time_ = time;
    map_cumulative_cpu["system"] = 0;
    return map_cumulative_cpu;
  }

  int64_t system_time_delta = cumulative_cpu - last_cumulative_cpu_;
  int64_t time_delta = time - last_cpu_time_;
  SB_DCHECK(time_delta != 0);
  if (time_delta == 0) {
    map_cumulative_cpu["system"] = 0;
    return map_cumulative_cpu;
  }

  last_cumulative_cpu_ = cumulative_cpu;
  last_cpu_time_ = time;

  double cpu_usage = 100.0 * static_cast<double>(system_time_delta) /
                     static_cast<double>(time_delta);
  map_cumulative_cpu["system"] = cpu_usage;
  {
    ScopedLock scoped_lock(mutex_);
    for (auto element : map_thread_ids) {
      int64_t task_time_delta = 0;
      for (auto value : element.second) {
        std::string task_stat_file =
            GetProcTaskDir(value.first).append(kStatFile);
        int64_t cumulative_cpu_task = GetCumulativeCPUUsage(task_stat_file);
        if (cumulative_cpu_task >= 0) {
          if (value.second != 0) {
            task_time_delta += (cumulative_cpu_task - value.second);
          }
          map_thread_ids[element.first][value.first] = cumulative_cpu_task;
        }
      }
      double cpu_usage = 100.0 * static_cast<double>(task_time_delta) /
                         static_cast<double>(time_delta);
      map_cumulative_cpu[element.first] = cpu_usage;
    }
  }
  return map_cumulative_cpu;
}

int GetAudioUnderrunCount() {
  if (audio_track_bridge_ != nullptr && audio_track_bridge_->is_valid()) {
    return audio_track_bridge_->GetUnderrunCount();
  }
  return -1;
}

bool IsSIMDEnabled() {
#if defined(USE_NEON)
  return true;
#else   // defined(USE_NEON)
  return false;
#endif  // defined(USE_NEON)
}

bool IsForceTunnelMode() {
  return starboard::android::shared::kForceTunnelMode;
}

const char* GetCurrentMediaAudioCodecName() {
  return GetMediaAudioCodecName(media_audio_codec_);
}

const char* GetCurrentMediaVideoCodecName() {
  return GetMediaVideoCodecName(media_video_codec_);
}

int GetCountShouldBePaused() {
  return should_be_paused_count_;
}

void IncreaseCountShouldBePaused() {
  should_be_paused_count_ += 1;
  SB_LOG(ERROR) << "Brown paused";
}

void AddThreadID(const char* thread_name, int32_t thread_id) {
  AddThreadIDInternal(thread_name, thread_id);
}

const StarboardExtensionPlayerPerfApi kGetPlayerPerfApi = {
    kStarboardExtensionPlayerPerfName,
    1,
    &GetPlatformIndependentCPUUsage,
    &GetAudioUnderrunCount,
    &IsSIMDEnabled,
    &IsForceTunnelMode,
    &GetCurrentMediaAudioCodecName,
    &GetCurrentMediaVideoCodecName,
    &IncreaseCountShouldBePaused,
    &GetCountShouldBePaused,
    &AddThreadID,
};

}  // namespace

void SetAudioTrackBridge(AudioTrackBridge* audio_track_bridge) {
  audio_track_bridge_ = audio_track_bridge;
}

void SetMediaVideoDecoderCodec(SbMediaVideoCodec codec) {
  media_video_codec_ = codec;
}

void SetMediaAudioDecoderCodec(SbMediaAudioCodec codec) {
  media_audio_codec_ = codec;
}

const void* GetPlayerPerfApi() {
  return &kGetPlayerPerfApi;
}

void AddThreadIDInternal(const char* thread_name, int32_t thread_id) {
  // SB_LOG(ERROR) << "Brown add " << thread_name << " (id " << thread_id <<
  // ").";
  ScopedLock scoped_lock(mutex_);
  auto found_thread_name = map_thread_ids.find(thread_name);
  if (found_thread_name == map_thread_ids.end()) {
    std::unordered_map<int32_t, int64_t> value_tmp;
    value_tmp[thread_id] = 0;
    map_thread_ids[thread_name] = value_tmp;
  } else {
    auto found_thread_id = map_thread_ids[thread_name].find(thread_id);
    if (found_thread_id == map_thread_ids[thread_name].end()) {
      map_thread_ids[thread_name][thread_id] = 0;
    }
  }
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

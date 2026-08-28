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

#include "starboard/linux/shared/low_memory_kill.h"

#include <fcntl.h>
#include <unistd.h>

#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/extension/low_memory_kill.h"
#include "starboard/shared/environment.h"
#include "starboard/system.h"

namespace starboard {

namespace {

constexpr char kTestEnvVar[] = "COBALT_WAS_LOW_MEMORY_KILLED";
constexpr char kTmpMarkerPath[] = "/tmp/cobalt_was_low_memory_killed";
constexpr char kMarkerFileName[] = "low_memory_kill_marker";
constexpr char kDefaultCgroupEventsPath[] = "/sys/fs/cgroup/memory.events";
static std::string g_cgroup_events_path = kDefaultCgroupEventsPath;
constexpr char kCgroupBaselineFileName[] = "cgroup_oom_baseline";

std::string GetStoragePath(const char* filename) {
  std::vector<char> path_buf(kSbFileMaxPath + 1, 0);
  if (SbSystemGetPath(kSbSystemPathStorageDirectory, path_buf.data(),
                      path_buf.size())) {
    std::string path(path_buf.data());
    if (!path.empty() && path.back() != '/') {
      path += '/';
    }
    path += filename;
    return path;
  }
  return "";
}

bool CheckAndConsumeMarker(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  if (access(path.c_str(), F_OK) == 0) {
    unlink(path.c_str());
    return true;
  }
  return false;
}

int64_t ReadCgroupOomKills() {
  std::ifstream events_file(g_cgroup_events_path);
  if (!events_file.is_open()) {
    return -1;
  }
  std::string key;
  int64_t value;
  while (events_file >> key >> value) {
    if (key == "oom_kill") {
      return value;
    }
  }
  return -1;
}

bool CheckCgroupOomKills() {
  int64_t current_kills = ReadCgroupOomKills();
  if (current_kills < 0) {
    return false;
  }

  std::string baseline_path = GetStoragePath(kCgroupBaselineFileName);
  if (baseline_path.empty()) {
    return false;
  }

  int64_t baseline_kills = 0;
  std::ifstream baseline_file(baseline_path);
  if (baseline_file.is_open()) {
    baseline_file >> baseline_kills;
    baseline_file.close();
  }

  bool oom_killed = (current_kills > baseline_kills);

  // Update baseline
  std::ofstream out_baseline(baseline_path, std::ios::trunc);
  if (out_baseline.is_open()) {
    out_baseline << current_kills << std::endl;
  }

  return oom_killed;
}

// Implements a multi-phase check for possible LMK signals. This is primarily
// provided for demonstration purposes, and platforms are not generally expected
// to implement more than one LMK signal.
//   1. Test Environment Variable Override - allows for easy testing of the LMK
//      API.
//   2. Supervisor/Watchdog Marker Files - checks for breadcrumb files left in
//      /tmp or Cobalt storage by a memory watchdog process, then deletes the
//      file.
//   3. Cgroups OOM Kill Counter Tracking - counts memory events recorded by
//      Linux cgroups v2, and compares to a previously stored count.
// Note that (1) is an immediate override and does not clear the other signals.
bool EvaluateWasLowMemoryKilled() {
  std::string env_val = starboard::GetEnvironment(kTestEnvVar);
  if (!env_val.empty()) {
    if (env_val == "1" || env_val == "true" || env_val == "TRUE" ||
        env_val == "True") {
      return true;
    }
    if (env_val == "0" || env_val == "false" || env_val == "FALSE" ||
        env_val == "False") {
      return false;
    }
  }

  // Immediate return would prevent proper clearing of any lower priority
  // signals that might be present.
  bool ret = false;

  if (CheckAndConsumeMarker(kTmpMarkerPath)) {
    ret = true;
  }
  std::string storage_marker = GetStoragePath(kMarkerFileName);
  if (CheckAndConsumeMarker(storage_marker)) {
    ret = true;
  }

  if (CheckCgroupOomKills()) {
    ret = true;
  }

  return ret;
}

// Since an LMK signal may be cleaned up after it is first read, we store the
// results of the first call to WasLowMemoryKilled() here and retain them for
// the life of the current process.
static std::mutex g_eval_mutex;
static bool g_evaluated = false;
static bool g_was_killed = false;

bool WasLowMemoryKilled() {
  std::lock_guard<std::mutex> lock(g_eval_mutex);
  if (!g_evaluated) {
    g_was_killed = EvaluateWasLowMemoryKilled();
    g_evaluated = true;
  }
  return g_was_killed;
}

const StarboardExtensionLowMemoryKillApi kLowMemoryKillApi = {
    kStarboardExtensionLowMemoryKillName,
    1,  // API version that's implemented.
    &WasLowMemoryKilled,
};

}  // namespace

const void* GetLowMemoryKillApi() {
  return &kLowMemoryKillApi;
}

namespace testing {

bool EvaluateLowMemoryKill() {
  return EvaluateWasLowMemoryKilled();
}

void ResetLowMemoryKillStateForTesting() {
  std::lock_guard<std::mutex> lock(g_eval_mutex);
  g_evaluated = false;
  g_was_killed = false;
}

void SetCgroupEventsPathForTesting(const char* path) {
  std::lock_guard<std::mutex> lock(g_eval_mutex);
  if (path && *path != '\0') {
    g_cgroup_events_path = path;
  } else {
    g_cgroup_events_path = kDefaultCgroupEventsPath;
  }
}

}  // namespace testing

}  // namespace starboard

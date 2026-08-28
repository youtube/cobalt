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

bool CheckAndConsumeMarker(const std::string& path) {
  return unlink(path.c_str()) == 0;
}

// Implements a two-phase check for possible LMK signals.
//   1. Test Environment Variable Override - allows for easy testing of the LMK
//      API.
//   2. Supervisor/Watchdog Marker Files - checks for breadcrumb files left in
//      /tmp or Cobalt storage by a memory watchdog process, then deletes the
//      file.
// Note that (1) does not clear any marker files that might exist.
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

  return CheckAndConsumeMarker(kTmpMarkerPath);
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
  std::lock_guard<std::mutex> lock(g_eval_mutex);
  return EvaluateWasLowMemoryKilled();
}

void ResetLowMemoryKillStateForTesting() {
  std::lock_guard<std::mutex> lock(g_eval_mutex);
  g_evaluated = false;
  g_was_killed = false;
}

}  // namespace testing

}  // namespace starboard

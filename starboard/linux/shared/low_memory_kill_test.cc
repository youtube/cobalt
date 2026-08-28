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

#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "starboard/configuration_constants.h"
#include "starboard/extension/low_memory_kill.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr char kTestEnvVar[] = "COBALT_WAS_LOW_MEMORY_KILLED";
constexpr char kTmpMarkerPath[] = "/tmp/cobalt_was_low_memory_killed";
constexpr char kMarkerFileName[] = "low_memory_kill_marker";
constexpr char kMockCgroupEventsPath[] = "/tmp/mock_cgroup_memory_events";
constexpr char kCgroupBaselineFileName[] = "cgroup_oom_baseline";

class LowMemoryKillTest : public ::testing::Test {
 protected:
  void SetUp() override {
    CleanEnvironmentAndFiles();
    testing::ResetLowMemoryKillStateForTesting();
    testing::SetCgroupEventsPathForTesting(nullptr);
  }

  void TearDown() override {
    CleanEnvironmentAndFiles();
    testing::ResetLowMemoryKillStateForTesting();
    testing::SetCgroupEventsPathForTesting(nullptr);
  }

  std::string GetStorageFilePath(const char* filename) {
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

  std::string GetStorageMarkerPath() {
    return GetStorageFilePath(kMarkerFileName);
  }

  std::string GetBaselinePath() {
    return GetStorageFilePath(kCgroupBaselineFileName);
  }

  void SetEnv(const char* val) { setenv(kTestEnvVar, val, 1); }

  void UnsetEnv() { unsetenv(kTestEnvVar); }

  void CreateTmpMarker() {
    std::ofstream out(kTmpMarkerPath, std::ios::trunc);
    out << "1\n";
    out.close();
  }

  void CreateStorageMarker() {
    std::string path = GetStorageMarkerPath();
    if (!path.empty()) {
      std::ofstream out(path, std::ios::trunc);
      out << "1\n";
      out.close();
    }
  }

  void CreateMockCgroupEvents(int64_t oom_kills) {
    std::ofstream out(kMockCgroupEventsPath, std::ios::trunc);
    out << "low 0\n";
    out << "high 0\n";
    out << "max 0\n";
    out << "oom 0\n";
    out << "oom_kill " << oom_kills << "\n";
    out << "oom_group_kill 0\n";
    out.close();
  }

  void SetBaselineOomKills(int64_t baseline) {
    std::string path = GetBaselinePath();
    if (!path.empty()) {
      std::ofstream out(path, std::ios::trunc);
      out << baseline << "\n";
      out.close();
    }
  }

  int64_t ReadBaselineOomKills() {
    std::string path = GetBaselinePath();
    if (path.empty()) {
      return -1;
    }
    std::ifstream in(path);
    if (!in.is_open()) {
      return -1;
    }
    int64_t val = -1;
    in >> val;
    return val;
  }

  bool TmpMarkerExists() { return access(kTmpMarkerPath, F_OK) == 0; }

  bool StorageMarkerExists() {
    std::string path = GetStorageMarkerPath();
    if (path.empty()) {
      return false;
    }
    return access(path.c_str(), F_OK) == 0;
  }

  void CleanEnvironmentAndFiles() {
    UnsetEnv();
    if (TmpMarkerExists()) {
      unlink(kTmpMarkerPath);
    }
    std::string storage_path = GetStorageMarkerPath();
    if (!storage_path.empty() && access(storage_path.c_str(), F_OK) == 0) {
      unlink(storage_path.c_str());
    }
    if (access(kMockCgroupEventsPath, F_OK) == 0) {
      unlink(kMockCgroupEventsPath);
    }
    std::string baseline_path = GetBaselinePath();
    if (!baseline_path.empty() && access(baseline_path.c_str(), F_OK) == 0) {
      unlink(baseline_path.c_str());
    }
  }
};

TEST_F(LowMemoryKillTest, DefaultReturnsFalse) {
  EXPECT_FALSE(testing::EvaluateLowMemoryKill());
}

TEST_F(LowMemoryKillTest, Env_PositiveValues) {
  const std::vector<std::string> positive_values = {"1", "true", "TRUE",
                                                    "True"};
  for (const auto& val : positive_values) {
    SetEnv(val.c_str());
    EXPECT_TRUE(testing::EvaluateLowMemoryKill())
        << "Failed for positive env value: " << val;
  }
}

TEST_F(LowMemoryKillTest, Env_NegativeValues) {
  const std::vector<std::string> negative_values = {"0", "false", "FALSE",
                                                    "False"};
  for (const auto& val : negative_values) {
    SetEnv(val.c_str());
    EXPECT_FALSE(testing::EvaluateLowMemoryKill())
        << "Failed for negative env value: " << val;
  }
}

TEST_F(LowMemoryKillTest, Watchdog_TmpMarkerDetectedAndConsumed) {
  CreateTmpMarker();
  ASSERT_TRUE(TmpMarkerExists());

  EXPECT_TRUE(testing::EvaluateLowMemoryKill());
  // Verify marker file was atomically unlinked upon evaluation
  EXPECT_FALSE(TmpMarkerExists());

  // Subsequent call without marker should return false
  EXPECT_FALSE(testing::EvaluateLowMemoryKill());
}

TEST_F(LowMemoryKillTest, Watchdog_StorageMarkerDetectedAndConsumed) {
  CreateStorageMarker();
  ASSERT_TRUE(StorageMarkerExists());

  EXPECT_TRUE(testing::EvaluateLowMemoryKill());
  // Verify marker file was atomically unlinked upon evaluation
  EXPECT_FALSE(StorageMarkerExists());

  // Subsequent call without marker should return false
  EXPECT_FALSE(testing::EvaluateLowMemoryKill());
}

TEST_F(LowMemoryKillTest, EnvOverridesWatchdog) {
  CreateTmpMarker();
  ASSERT_TRUE(TmpMarkerExists());
  SetEnv("0");

  EXPECT_FALSE(testing::EvaluateLowMemoryKill());
  EXPECT_TRUE(TmpMarkerExists());

  UnsetEnv();
  unlink(kTmpMarkerPath);
  SetEnv("1");
  EXPECT_TRUE(testing::EvaluateLowMemoryKill());
}

TEST_F(LowMemoryKillTest, Cgroup_IncrementDetectedAndUpdatesBaseline) {
  CreateMockCgroupEvents(5);
  testing::SetCgroupEventsPathForTesting(kMockCgroupEventsPath);
  SetBaselineOomKills(4);

  // Since current (5) > baseline (4), should return true and update baseline to
  // 5
  EXPECT_TRUE(testing::EvaluateLowMemoryKill());
  EXPECT_EQ(ReadBaselineOomKills(), 5);

  // Subsequent call with same count should return false
  EXPECT_FALSE(testing::EvaluateLowMemoryKill());
  EXPECT_EQ(ReadBaselineOomKills(), 5);
}

TEST_F(LowMemoryKillTest, Cgroup_NoIncrementReturnsFalse) {
  CreateMockCgroupEvents(5);
  testing::SetCgroupEventsPathForTesting(kMockCgroupEventsPath);
  SetBaselineOomKills(5);

  // Since current (5) == baseline (5), should return false
  EXPECT_FALSE(testing::EvaluateLowMemoryKill());
  EXPECT_EQ(ReadBaselineOomKills(), 5);
}

TEST_F(LowMemoryKillTest, Cgroup_MissingOrMalformedReturnsFalse) {
  testing::SetCgroupEventsPathForTesting(
      "/tmp/non_existent_cgroup_events_path");
  EXPECT_FALSE(testing::EvaluateLowMemoryKill());

  // Malformed cgroup file without oom_kill counter
  std::ofstream out(kMockCgroupEventsPath, std::ios::trunc);
  out << "low 0\nhigh 0\nmax 0\n";
  out.close();
  testing::SetCgroupEventsPathForTesting(kMockCgroupEventsPath);
  EXPECT_FALSE(testing::EvaluateLowMemoryKill());
}

TEST_F(LowMemoryKillTest, WatchdogOverridesCgroup) {
  CreateMockCgroupEvents(3);
  testing::SetCgroupEventsPathForTesting(kMockCgroupEventsPath);
  SetBaselineOomKills(3);  // Cgroup alone would evaluate to false

  CreateTmpMarker();  // Watchdog marker present
  EXPECT_TRUE(testing::EvaluateLowMemoryKill());
  EXPECT_FALSE(TmpMarkerExists());  // Consumed
}

TEST_F(LowMemoryKillTest, PublicApiSingletonAndIdempotency) {
  const StarboardExtensionLowMemoryKillApi* extension_api =
      static_cast<const StarboardExtensionLowMemoryKillApi*>(
          SbSystemGetExtension(kStarboardExtensionLowMemoryKillName));
  ASSERT_NE(extension_api, nullptr);
  EXPECT_STREQ(extension_api->name, kStarboardExtensionLowMemoryKillName);
  EXPECT_EQ(extension_api->version, 1u);
  ASSERT_NE(extension_api->WasLowMemoryKilled, nullptr);

  EXPECT_FALSE(extension_api->WasLowMemoryKilled());
  // Subsequent calls should be cached and return false.
  EXPECT_FALSE(extension_api->WasLowMemoryKilled());

  // Simulate a new process startup post-LMK by resetting testing state, setting
  // marker, then verify WasLowMemoryKilled returns true and caches.
  testing::ResetLowMemoryKillStateForTesting();
  CreateTmpMarker();
  EXPECT_TRUE(extension_api->WasLowMemoryKilled());
  EXPECT_FALSE(TmpMarkerExists());
  EXPECT_TRUE(extension_api->WasLowMemoryKilled());
  EXPECT_TRUE(extension_api->WasLowMemoryKilled());
}

}  // namespace
}  // namespace starboard

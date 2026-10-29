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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fstream>
#include <string>
#include <vector>

#include "starboard/extension/low_memory_kill.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr char kTestEnvVar[] = "COBALT_WAS_LOW_MEMORY_KILLED";
constexpr char kTmpMarkerPath[] = "/tmp/cobalt_was_low_memory_killed";

constexpr SbLowMemoryKillDetails kSentinelDetails = {
    123456789,
    kSbLowMemoryKillForegroundStatusForeground,
    kSbLowMemoryKillPressureSourceCobalt,
    100,
    200,
    300,
    400,
    "sentinel reason",
};

constexpr SbLowMemoryKillDetails kEmptyDetails = {
    0,
    kSbLowMemoryKillForegroundStatusUnknown,
    kSbLowMemoryKillPressureSourceUnknown,
    -1,
    -1,
    -1,
    -1,
    "",
};

void ExpectDetailsEq(const SbLowMemoryKillDetails& expected,
                     const SbLowMemoryKillDetails& actual) {
  EXPECT_EQ(actual.timestamp_posix_usec, expected.timestamp_posix_usec);
  EXPECT_EQ(actual.foreground_status, expected.foreground_status);
  EXPECT_EQ(actual.pressure_source, expected.pressure_source);
  EXPECT_EQ(actual.system_memory_available, expected.system_memory_available);
  EXPECT_EQ(actual.system_memory_total, expected.system_memory_total);
  EXPECT_EQ(actual.cobalt_memory_usage, expected.cobalt_memory_usage);
  EXPECT_EQ(actual.cobalt_memory_limit, expected.cobalt_memory_limit);
  ASSERT_LT(strnlen(actual.reason, sizeof(actual.reason)),
            sizeof(actual.reason));
  EXPECT_STREQ(actual.reason, expected.reason);
}

class LowMemoryKillTest : public ::testing::Test {
 protected:
  void SetUp() override {
    UnsetEnv();
    if (TmpMarkerExists()) {
      unlink(kTmpMarkerPath);
    }
    testing::ResetLowMemoryKillStateForTesting();
  }

  void TearDown() override {
    UnsetEnv();
    if (TmpMarkerExists()) {
      unlink(kTmpMarkerPath);
    }
    testing::ResetLowMemoryKillStateForTesting();
  }

  void SetEnv(const char* val) { setenv(kTestEnvVar, val, 1); }

  void UnsetEnv() { unsetenv(kTestEnvVar); }

  void CreateTmpMarker() {
    std::ofstream out(kTmpMarkerPath, std::ios::trunc);
    out << "1\n";
    out.close();
  }

  bool TmpMarkerExists() { return access(kTmpMarkerPath, F_OK) == 0; }
};

TEST_F(LowMemoryKillTest, DefaultReturnsFalse) {
  EXPECT_FALSE(testing::EvaluateLowMemoryKill());
}

TEST_F(LowMemoryKillTest, Env_PositiveValues) {
  const std::vector<std::string> positive_values = {"1", "true"};
  for (const auto& val : positive_values) {
    SetEnv(val.c_str());
    EXPECT_TRUE(testing::EvaluateLowMemoryKill())
        << "Failed for positive env value: " << val;
  }
}

TEST_F(LowMemoryKillTest, Env_NegativeValues) {
  const std::vector<std::string> negative_values = {"0", "false"};
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

TEST_F(LowMemoryKillTest, PublicApiSingletonAndIdempotency) {
  const StarboardExtensionLowMemoryKillApi* extension_api =
      static_cast<const StarboardExtensionLowMemoryKillApi*>(
          SbSystemGetExtension(kStarboardExtensionLowMemoryKillName));
  ASSERT_NE(extension_api, nullptr);
  EXPECT_STREQ(extension_api->name, kStarboardExtensionLowMemoryKillName);
  EXPECT_EQ(extension_api->version, 2u);
  ASSERT_NE(extension_api->WasLowMemoryKilled, nullptr);
  ASSERT_NE(extension_api->GetLowMemoryKillDetails, nullptr);

  EXPECT_FALSE(extension_api->WasLowMemoryKilled());
  // Subsequent calls should be cached and return false.
  EXPECT_FALSE(extension_api->WasLowMemoryKilled());

  // Struct fields should be unchanged if WasLowMemoryKilled() is false.
  SbLowMemoryKillDetails details = kSentinelDetails;
  EXPECT_FALSE(extension_api->GetLowMemoryKillDetails(&details));
  ExpectDetailsEq(kSentinelDetails, details);
  EXPECT_FALSE(extension_api->GetLowMemoryKillDetails(nullptr));

  // Simulate a new process startup post-LMK by resetting testing state and
  // setting a marker. Verify GetLowMemoryKillDetails(nullptr) does not consume
  // the marker, then verify WasLowMemoryKilled and GetLowMemoryKillDetails
  // return true, report empty/unavailable stub details, and cache.
  testing::ResetLowMemoryKillStateForTesting();
  CreateTmpMarker();
  EXPECT_FALSE(extension_api->GetLowMemoryKillDetails(nullptr));
  EXPECT_TRUE(TmpMarkerExists());

  EXPECT_TRUE(extension_api->WasLowMemoryKilled());
  EXPECT_FALSE(TmpMarkerExists());
  EXPECT_TRUE(extension_api->WasLowMemoryKilled());
  EXPECT_TRUE(extension_api->WasLowMemoryKilled());

  EXPECT_FALSE(extension_api->GetLowMemoryKillDetails(nullptr));
  details = kSentinelDetails;
  EXPECT_TRUE(extension_api->GetLowMemoryKillDetails(&details));
  ExpectDetailsEq(kEmptyDetails, details);

  // Calling GetLowMemoryKillDetails(&details) before WasLowMemoryKilled()
  // should also evaluate, consume the marker, and cache the state.
  testing::ResetLowMemoryKillStateForTesting();
  CreateTmpMarker();
  details = kSentinelDetails;
  EXPECT_TRUE(extension_api->GetLowMemoryKillDetails(&details));
  EXPECT_FALSE(TmpMarkerExists());
  ExpectDetailsEq(kEmptyDetails, details);
  EXPECT_TRUE(extension_api->WasLowMemoryKilled());
}

}  // namespace
}  // namespace starboard

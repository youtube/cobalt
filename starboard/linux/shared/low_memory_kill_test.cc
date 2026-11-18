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
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "starboard/extension/low_memory_kill.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr char kTestEnvVar[] = "COBALT_WAS_LOW_MEMORY_KILLED";
constexpr char kTmpMarkerPath[] = "/tmp/cobalt_was_low_memory_killed";
constexpr int64_t kInt64Max = std::numeric_limits<int64_t>::max();

constexpr SbLowMemoryKillDetails kDefaultDetails = {
    0,
    kSbLowMemoryKillForegroundStatusUnknown,
    kSbLowMemoryKillPressureSourceUnknown,
    -1,
    -1,
    -1,
    -1,
    "",
};

constexpr SbLowMemoryKillDetails kSentinelDetails = {
    999,
    kSbLowMemoryKillForegroundStatusForeground,
    kSbLowMemoryKillPressureSourceCobalt,
    100,
    200,
    300,
    400,
    "sentinel reason",
};

constexpr SbLowMemoryKillDetails kSampleDetails = {
    1700000000123456ll,
    kSbLowMemoryKillForegroundStatusBackground,
    kSbLowMemoryKillPressureSourceSystem,
    104857600ll,
    2147483648ll,
    943718400ll,
    1073741824ll,
    "system memory exhausted",
};

constexpr char kSampleMarkerContents[] =
    "# Lines starting with '#' are comments. Blank lines are ignored.\n"
    "\n"
    "  timestamp_posix_usec = 1700000000123456\r\n"
    "  foreground_status = 2\r\n"
    "\tpressure_source=system\t\r\n"
    "system_memory_available=104857600\n"
    "system_memory_total=2147483648\n"
    "cobalt_memory_usage=943718400\n"
    "  reason = system memory exhausted \r\n"
    "cobalt_memory_limit=1073741824";

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
    RemoveTmpMarker();
    testing::ResetLowMemoryKillStateForTesting();
    api_ = static_cast<const StarboardExtensionLowMemoryKillApi*>(
        SbSystemGetExtension(kStarboardExtensionLowMemoryKillName));
    ASSERT_NE(api_, nullptr);
    ASSERT_GE(api_->version, 2u);
  }

  void TearDown() override {
    UnsetEnv();
    RemoveTmpMarker();
    testing::ResetLowMemoryKillStateForTesting();
  }

  void SetEnv(const char* val) { setenv(kTestEnvVar, val, 1); }

  void UnsetEnv() { unsetenv(kTestEnvVar); }

  void CreateTmpMarker() { CreateTmpMarkerWithContents("1\n"); }

  void CreateTmpMarkerWithContents(const std::string& contents) {
    std::ofstream out(kTmpMarkerPath, std::ios::trunc | std::ios::binary);
    out << contents;
    out.close();
  }

  void RemoveTmpMarker() {
    unlink(kTmpMarkerPath);
    rmdir(kTmpMarkerPath);
  }

  bool TmpMarkerExists() { return access(kTmpMarkerPath, F_OK) == 0; }

  SbLowMemoryKillDetails GetDetailsFromPublicApi() {
    SbLowMemoryKillDetails details = kSentinelDetails;
    EXPECT_TRUE(api_->WasLowMemoryKilled());
    EXPECT_TRUE(api_->GetLowMemoryKillDetails(&details));
    EXPECT_FALSE(TmpMarkerExists());
    return details;
  }

  const StarboardExtensionLowMemoryKillApi* api_ = nullptr;
};

TEST_F(LowMemoryKillTest, DefaultReturnsFalse) {
  EXPECT_FALSE(testing::EvaluateLowMemoryKill());
}

TEST_F(LowMemoryKillTest, Env_PositiveValues) {
  for (const char* val : {"1", "true"}) {
    SetEnv(val);
    EXPECT_TRUE(testing::EvaluateLowMemoryKill())
        << "Failed for positive env value: " << val;
  }
}

TEST_F(LowMemoryKillTest, Env_NegativeValues) {
  for (const char* val : {"0", "false"}) {
    SetEnv(val);
    EXPECT_FALSE(testing::EvaluateLowMemoryKill())
        << "Failed for negative env value: " << val;
  }
}

TEST_F(LowMemoryKillTest, Watchdog_TmpMarkerDetectedAndConsumed) {
  CreateTmpMarker();
  ASSERT_TRUE(TmpMarkerExists());

  EXPECT_TRUE(testing::EvaluateLowMemoryKill());
  EXPECT_FALSE(TmpMarkerExists());
  EXPECT_FALSE(testing::EvaluateLowMemoryKill());
}

TEST_F(LowMemoryKillTest, EnvOverridesWatchdog) {
  CreateTmpMarkerWithContents(kSampleMarkerContents);
  ASSERT_TRUE(TmpMarkerExists());
  SetEnv("0");

  SbLowMemoryKillDetails details = kSentinelDetails;
  EXPECT_FALSE(testing::EvaluateLowMemoryKill(&details));
  EXPECT_TRUE(TmpMarkerExists());

  // Env "1" overrides the marker with default details without consuming it.
  SetEnv("1");
  details = kSentinelDetails;
  EXPECT_TRUE(testing::EvaluateLowMemoryKill(&details));
  ExpectDetailsEq(kDefaultDetails, details);
  EXPECT_TRUE(TmpMarkerExists());

  // Unrecognized env falls back to the marker file.
  SetEnv("maybe");
  ExpectDetailsEq(kSampleDetails, GetDetailsFromPublicApi());
}

TEST_F(LowMemoryKillTest, PublicApiSingletonAndIdempotency) {
  EXPECT_STREQ(api_->name, kStarboardExtensionLowMemoryKillName);
  EXPECT_EQ(api_->version, 2u);
  ASSERT_NE(api_->WasLowMemoryKilled, nullptr);
  ASSERT_NE(api_->GetLowMemoryKillDetails, nullptr);

  EXPECT_FALSE(api_->WasLowMemoryKilled());
  EXPECT_FALSE(api_->WasLowMemoryKilled());

  SbLowMemoryKillDetails details = kSentinelDetails;
  EXPECT_FALSE(api_->GetLowMemoryKillDetails(&details));
  ExpectDetailsEq(kSentinelDetails, details);
  EXPECT_FALSE(api_->GetLowMemoryKillDetails(nullptr));

  // Simulate a new process startup with a detailed marker.
  testing::ResetLowMemoryKillStateForTesting();
  CreateTmpMarkerWithContents(kSampleMarkerContents);
  EXPECT_FALSE(api_->GetLowMemoryKillDetails(nullptr));
  EXPECT_TRUE(TmpMarkerExists());

  EXPECT_TRUE(api_->WasLowMemoryKilled());
  EXPECT_FALSE(TmpMarkerExists());
  EXPECT_TRUE(api_->WasLowMemoryKilled());

  EXPECT_FALSE(api_->GetLowMemoryKillDetails(nullptr));
  details = kSentinelDetails;
  EXPECT_TRUE(api_->GetLowMemoryKillDetails(&details));
  ExpectDetailsEq(kSampleDetails, details);

  // Calling GetLowMemoryKillDetails(&details) before WasLowMemoryKilled()
  // also evaluates, consumes a legacy marker, and caches default details.
  testing::ResetLowMemoryKillStateForTesting();
  CreateTmpMarker();
  details = kSentinelDetails;
  EXPECT_TRUE(api_->GetLowMemoryKillDetails(&details));
  EXPECT_FALSE(TmpMarkerExists());
  ExpectDetailsEq(kDefaultDetails, details);
  EXPECT_TRUE(api_->WasLowMemoryKilled());
}

TEST_F(LowMemoryKillTest, MarkerThatCannotBeRemovedIsNotKilled) {
  ASSERT_EQ(mkdir(kTmpMarkerPath, 0700), 0);
  SbLowMemoryKillDetails details = kSentinelDetails;
  EXPECT_FALSE(api_->WasLowMemoryKilled());
  EXPECT_FALSE(api_->GetLowMemoryKillDetails(&details));
  ExpectDetailsEq(kSentinelDetails, details);
  EXPECT_EQ(rmdir(kTmpMarkerPath), 0);
}

TEST_F(LowMemoryKillTest, PartialAndEmptyMarkersPopulateOnlyPresentFields) {
  CreateTmpMarkerWithContents("");
  ExpectDetailsEq(kDefaultDetails, GetDetailsFromPublicApi());

  testing::ResetLowMemoryKillStateForTesting();
  CreateTmpMarkerWithContents(
      "pressure_source=cobalt\n"
      "cobalt_memory_usage=5000000000\n");
  SbLowMemoryKillDetails expected = kDefaultDetails;
  expected.pressure_source = kSbLowMemoryKillPressureSourceCobalt;
  expected.cobalt_memory_usage = 5000000000ll;
  ExpectDetailsEq(expected, GetDetailsFromPublicApi());
}

TEST_F(LowMemoryKillTest, AllEnumValuesAndMemoryBoundaries) {
  ExpectDetailsEq(
      SbLowMemoryKillDetails{
          0,
          kSbLowMemoryKillForegroundStatusForeground,
          kSbLowMemoryKillPressureSourceCobalt,
          0,
          -1,
          kInt64Max,
          0,
          "",
      },
      testing::ParseLowMemoryKillMarker(
          "timestamp_posix_usec=0\n"
          "foreground_status=foreground\n"
          "pressure_source=cobalt\n"
          "system_memory_available=0\n"
          "system_memory_total=-1\n"
          "cobalt_memory_usage=9223372036854775807\n"
          "cobalt_memory_limit=0\n"));

  ExpectDetailsEq(
      SbLowMemoryKillDetails{
          kInt64Max,
          kSbLowMemoryKillForegroundStatusUnknown,
          kSbLowMemoryKillPressureSourceUnknown,
          -1,
          -1,
          -1,
          -1,
          "",
      },
      testing::ParseLowMemoryKillMarker(
          "timestamp_posix_usec=9223372036854775807\n"
          "foreground_status=unknown\n"
          "pressure_source=unknown\n"));
}

TEST_F(LowMemoryKillTest, ReasonParsingAndTruncation) {
  constexpr size_t kMaxLen = kSbLowMemoryKillReasonMaxSize - 1;
  std::string long_reason(200, 'a');
  long_reason[kMaxLen - 1] = 'z';
  long_reason[kMaxLen] = '!';

  SbLowMemoryKillDetails truncated =
      testing::ParseLowMemoryKillMarker("reason=" + long_reason + "\n");
  EXPECT_EQ(strlen(truncated.reason), kMaxLen);
  EXPECT_EQ(truncated.reason[kMaxLen - 1], 'z');

  // '=' and '#' inside reason are preserved; empty reason clears earlier value;
  // non-printable or non-ASCII bytes are rejected without overwriting.
  SbLowMemoryKillDetails special = testing::ParseLowMemoryKillMarker(
      "  reason = oom_kill: rss=950MiB #1 consumer \t\r\n");
  EXPECT_STREQ(special.reason, "oom_kill: rss=950MiB #1 consumer");

  ExpectDetailsEq(kDefaultDetails,
                  testing::ParseLowMemoryKillMarker("reason=initial\n"
                                                    "reason=   \n"));

  const char kWithBadReasons[] =
      "reason=kept reason\n"
      "reason=bad\x00nul\n"
      "reason=bad\ttab\n"
      "reason=bad\x7f"
      "del\n"
      "reason=bad\xffhigh\n";
  SbLowMemoryKillDetails kept = testing::ParseLowMemoryKillMarker(
      std::string_view(kWithBadReasons, sizeof(kWithBadReasons) - 1));
  EXPECT_STREQ(kept.reason, "kept reason");
}

TEST_F(LowMemoryKillTest, MalformedEntriesIgnoredAndLastValidWins) {
  const char kContents[] =
      "timestamp_posix_usec=100\n"
      "timestamp_posix_usec=200\n"
      "timestamp_posix_usec=-1\n"
      "timestamp_posix_usec=-0\n"
      "timestamp_posix_usec=+5\n"
      "timestamp_posix_usec=9223372036854775808\n"
      "timestamp_posix_usec=abc\n"
      "timestamp_posix_usec=\n"
      "foreground_status=foreground\n"
      "foreground_status=3\n"
      "foreground_status=-1\n"
      "foreground_status=other\n"
      "foreground_status=\n"
      "pressure_source=cobalt\n"
      "pressure_source=99\n"
      "system_memory_available=512\n"
      "system_memory_available=-1\n"
      "system_memory_available=-2\n"
      "system_memory_available=-0\n"
      "system_memory_total=1024\n"
      "system_memory_total=12abc\n"
      "cobalt_memory_usage=2048\n"
      "cobalt_memory_usage=0x10\n"
      "cobalt_memory_limit=4096\n"
      "cobalt_memory_limit=18446744073709551615\n"
      "unknown_key=5\n"
      "=5\n"
      "just some text\n"
      "\xff\x00garbage\n";
  CreateTmpMarkerWithContents(std::string(kContents, sizeof(kContents) - 1));
  SbLowMemoryKillDetails expected = {
      200,
      kSbLowMemoryKillForegroundStatusForeground,
      kSbLowMemoryKillPressureSourceCobalt,
      -1,
      1024,
      2048,
      4096,
      "",
  };
  ExpectDetailsEq(expected, GetDetailsFromPublicApi());
}

}  // namespace
}  // namespace starboard

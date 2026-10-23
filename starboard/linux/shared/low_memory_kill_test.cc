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

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "starboard/extension/low_memory_kill.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr char kTestEnvVar[] = "COBALT_WAS_LOW_MEMORY_KILLED";
constexpr char kTmpMarkerPath[] = "/tmp/cobalt_was_low_memory_killed";

constexpr uint64_t kUint64Max = std::numeric_limits<uint64_t>::max();

// Values that differ from those in kSampleDetails, used to detect whether an
// out-parameter was modified.
constexpr SbLowMemoryKillDetails kSentinelDetails = {
    kSbLowMemoryKillForegroundStatusForeground,
    kSbLowMemoryKillPressureSourceCobalt,
    100,
    200,
    300,
    400,
};

// Non-default values, with every memory value larger than 2^32.
constexpr SbLowMemoryKillDetails kSampleDetails = {
    kSbLowMemoryKillForegroundStatusBackground,
    kSbLowMemoryKillPressureSourceSystem,
    (1ull << 32) + 1,
    (1ull << 40) + 12345,
    (1ull << 33) + 7,
    kUint64Max,
};

void ExpectDetailsEq(const SbLowMemoryKillDetails& expected,
                     const SbLowMemoryKillDetails& actual) {
  EXPECT_EQ(actual.foreground_status, expected.foreground_status);
  EXPECT_EQ(actual.pressure_source, expected.pressure_source);
  EXPECT_EQ(actual.system_memory_available, expected.system_memory_available);
  EXPECT_EQ(actual.system_memory_total, expected.system_memory_total);
  EXPECT_EQ(actual.cobalt_memory_usage, expected.cobalt_memory_usage);
  EXPECT_EQ(actual.cobalt_memory_limit, expected.cobalt_memory_limit);
}

void ExpectDetailsEmpty(const SbLowMemoryKillDetails& actual) {
  ExpectDetailsEq(SbLowMemoryKillDetails{}, actual);
}

const StarboardExtensionLowMemoryKillApi* GetApi() {
  return static_cast<const StarboardExtensionLowMemoryKillApi*>(
      SbSystemGetExtension(kStarboardExtensionLowMemoryKillName));
}

class LowMemoryKillTest : public ::testing::Test {
 protected:
  void SetUp() override {
    UnsetEnv();
    RemoveTmpMarker();
    testing::ResetLowMemoryKillStateForTesting();
    api_ = GetApi();
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

  void CreateTmpMarkerWithDetails(const SbLowMemoryKillDetails& details) {
    ASSERT_TRUE(WriteLowMemoryKillMarker(kTmpMarkerPath, details));
  }

  void RemoveTmpMarker() {
    unlink(kTmpMarkerPath);
    rmdir(kTmpMarkerPath);  // In case a test failed with a directory there.
  }

  bool TmpMarkerExists() { return access(kTmpMarkerPath, F_OK) == 0; }

  // Evaluates through the public API, as a fresh process would.
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
  // Always return false if given a nullptr.
  EXPECT_FALSE(extension_api->GetLowMemoryKillDetails(nullptr));

  // Simulate a new process startup post-LMK by resetting testing state, setting
  // a marker with details, then verify WasLowMemoryKilled and
  // GetLowMemoryKillDetails return true, report the marker's details, and
  // cache.
  testing::ResetLowMemoryKillStateForTesting();
  CreateTmpMarkerWithDetails(kSampleDetails);
  EXPECT_TRUE(extension_api->WasLowMemoryKilled());
  EXPECT_FALSE(TmpMarkerExists());
  EXPECT_TRUE(extension_api->WasLowMemoryKilled());
  EXPECT_TRUE(extension_api->WasLowMemoryKilled());

  // Always return false if given a nullptr.
  EXPECT_FALSE(extension_api->GetLowMemoryKillDetails(nullptr));
  details = kSentinelDetails;
  EXPECT_TRUE(extension_api->GetLowMemoryKillDetails(&details));
  ExpectDetailsEq(kSampleDetails, details);
}

TEST_F(LowMemoryKillTest, GetLowMemoryKillDetailsEvaluatesAndCachesState) {
  const StarboardExtensionLowMemoryKillApi* extension_api =
      static_cast<const StarboardExtensionLowMemoryKillApi*>(
          SbSystemGetExtension(kStarboardExtensionLowMemoryKillName));
  ASSERT_NE(extension_api, nullptr);
  ASSERT_NE(extension_api->GetLowMemoryKillDetails, nullptr);

  CreateTmpMarker();
  ASSERT_TRUE(TmpMarkerExists());

  // Calling GetLowMemoryKillDetails(nullptr) should fail without consuming the
  // marker.
  EXPECT_FALSE(extension_api->GetLowMemoryKillDetails(nullptr));
  EXPECT_TRUE(TmpMarkerExists());

  // Calling GetLowMemoryKillDetails(&details) before WasLowMemoryKilled()
  // should evaluate, consume the marker, and cache the state.
  SbLowMemoryKillDetails details = {};
  EXPECT_TRUE(extension_api->GetLowMemoryKillDetails(&details));
  EXPECT_FALSE(TmpMarkerExists());
  EXPECT_TRUE(extension_api->WasLowMemoryKilled());
  EXPECT_TRUE(extension_api->GetLowMemoryKillDetails(&details));
}

// --- Marker format: writer and round trips. ---

TEST_F(LowMemoryKillTest, FormatMatchesDocumentedFormat) {
  EXPECT_EQ(FormatLowMemoryKillMarker(kSampleDetails),
            "foreground_status=background\n"
            "pressure_source=system\n"
            "system_memory_available=4294967297\n"
            "system_memory_total=1099511640121\n"
            "cobalt_memory_usage=8589934599\n"
            "cobalt_memory_limit=18446744073709551615\n");
  EXPECT_EQ(FormatLowMemoryKillMarker(SbLowMemoryKillDetails{}),
            "foreground_status=unknown\n"
            "pressure_source=unknown\n"
            "system_memory_available=0\n"
            "system_memory_total=0\n"
            "cobalt_memory_usage=0\n"
            "cobalt_memory_limit=0\n");
}

TEST_F(LowMemoryKillTest, FormatWritesOutOfRangeEnumsAsUnknown) {
  // A C caller can store any value in the enum fields. Store them through the
  // object representation, since converting an out-of-range value to the C++
  // enum type is undefined.
  using ForegroundRaw = std::underlying_type_t<SbLowMemoryKillForegroundStatus>;
  using PressureRaw = std::underlying_type_t<SbLowMemoryKillPressureSource>;
  for (uint64_t value : {4ull, 42ull, 0xffffffffull}) {
    SCOPED_TRACE(::testing::Message() << "value=" << value);
    SbLowMemoryKillDetails details = {};
    const auto foreground_raw = static_cast<ForegroundRaw>(value);
    const auto pressure_raw = static_cast<PressureRaw>(value);
    memcpy(&details.foreground_status, &foreground_raw, sizeof(foreground_raw));
    memcpy(&details.pressure_source, &pressure_raw, sizeof(pressure_raw));
    const std::string marker = FormatLowMemoryKillMarker(details);
    EXPECT_NE(marker.find("foreground_status=unknown\n"), std::string::npos);
    EXPECT_NE(marker.find("pressure_source=unknown\n"), std::string::npos);
    ExpectDetailsEmpty(ParseLowMemoryKillMarker(marker));
  }
}

TEST_F(LowMemoryKillTest, RoundTrip_AllFieldsWithLargeValues) {
  CreateTmpMarkerWithDetails(kSampleDetails);
  ASSERT_TRUE(TmpMarkerExists());
  ExpectDetailsEq(kSampleDetails, GetDetailsFromPublicApi());
}

TEST_F(LowMemoryKillTest, RoundTrip_AllEnumValues) {
  const SbLowMemoryKillForegroundStatus kForegroundStatuses[] = {
      kSbLowMemoryKillForegroundStatusUnknown,
      kSbLowMemoryKillForegroundStatusOther,
      kSbLowMemoryKillForegroundStatusForeground,
      kSbLowMemoryKillForegroundStatusBackground,
  };
  const SbLowMemoryKillPressureSource kPressureSources[] = {
      kSbLowMemoryKillPressureSourceUnknown,
      kSbLowMemoryKillPressureSourceOther,
      kSbLowMemoryKillPressureSourceSystem,
      kSbLowMemoryKillPressureSourceCobalt,
  };
  for (auto foreground_status : kForegroundStatuses) {
    for (auto pressure_source : kPressureSources) {
      SCOPED_TRACE(::testing::Message()
                   << "foreground_status=" << foreground_status
                   << " pressure_source=" << pressure_source);
      testing::ResetLowMemoryKillStateForTesting();
      SbLowMemoryKillDetails expected = kSampleDetails;
      expected.foreground_status = foreground_status;
      expected.pressure_source = pressure_source;
      CreateTmpMarkerWithDetails(expected);
      ExpectDetailsEq(expected, GetDetailsFromPublicApi());
    }
  }
}

TEST_F(LowMemoryKillTest, WriteFailsForUnwritablePath) {
  EXPECT_FALSE(WriteLowMemoryKillMarker(
      "/nonexistent_cobalt_dir/cobalt_was_low_memory_killed", kSampleDetails));
  EXPECT_FALSE(WriteLowMemoryKillMarker(nullptr, kSampleDetails));
  EXPECT_FALSE(WriteLowMemoryKillMarker("", kSampleDetails));
}

std::vector<std::string> ListDirectory(const std::string& dir) {
  std::vector<std::string> names;
  DIR* d = opendir(dir.c_str());
  if (!d) {
    return names;
  }
  while (struct dirent* entry = readdir(d)) {
    const std::string name = entry->d_name;
    if (name != "." && name != "..") {
      names.push_back(name);
    }
  }
  closedir(d);
  return names;
}

TEST_F(LowMemoryKillTest, WriteLeavesNoTemporaryFiles) {
  char dir_template[] = "/tmp/cobalt_lmk_writer_test.XXXXXX";
  ASSERT_NE(mkdtemp(dir_template), nullptr);
  const std::string dir = dir_template;
  const std::string marker = dir + "/marker";

  ASSERT_TRUE(WriteLowMemoryKillMarker(marker.c_str(), kSampleDetails));
  EXPECT_EQ(ListDirectory(dir), std::vector<std::string>{"marker"});
  struct stat info = {};
  ASSERT_EQ(stat(marker.c_str(), &info), 0);
  EXPECT_EQ(info.st_mode & 0777, 0644u);
  std::ifstream in(marker, std::ios::binary);
  const std::string contents((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
  ExpectDetailsEq(kSampleDetails, ParseLowMemoryKillMarker(contents));
  ASSERT_EQ(unlink(marker.c_str()), 0);

  // A directory at the target path makes the final rename() fail; the
  // temporary file must be cleaned up.
  ASSERT_EQ(mkdir(marker.c_str(), 0700), 0);
  EXPECT_FALSE(WriteLowMemoryKillMarker(marker.c_str(), kSampleDetails));
  EXPECT_EQ(ListDirectory(dir), std::vector<std::string>{"marker"});

  EXPECT_EQ(rmdir(marker.c_str()), 0);
  EXPECT_EQ(rmdir(dir.c_str()), 0);
}

// --- Backward compatibility and robust parsing. ---

TEST_F(LowMemoryKillTest, LegacyMarkerIsKilledWithEmptyDetails) {
  CreateTmpMarker();  // Writes "1\n".
  ExpectDetailsEmpty(GetDetailsFromPublicApi());
}

TEST_F(LowMemoryKillTest, EmptyMarkerIsKilledWithEmptyDetails) {
  CreateTmpMarkerWithContents("");
  ASSERT_TRUE(TmpMarkerExists());
  ExpectDetailsEmpty(GetDetailsFromPublicApi());
}

TEST_F(LowMemoryKillTest, PartialMarkerPopulatesOnlyPresentFields) {
  CreateTmpMarkerWithContents(
      "pressure_source=cobalt\n"
      "cobalt_memory_usage=5000000000\n");
  SbLowMemoryKillDetails expected = {};
  expected.pressure_source = kSbLowMemoryKillPressureSourceCobalt;
  expected.cobalt_memory_usage = 5000000000ull;
  ExpectDetailsEq(expected, GetDetailsFromPublicApi());
}

TEST_F(LowMemoryKillTest, HandWrittenMarkerFromDocumentation) {
  // Mirrors the example in low_memory_kill.h, plus documented leniencies:
  // comments, blank lines, surrounding whitespace, CRLF line endings, numeric
  // enum values, and no trailing newline.
  CreateTmpMarkerWithContents(
      "# Lines starting with '#' are comments. Blank lines are ignored.\n"
      "\n"
      "  foreground_status = 3\r\n"
      "\tpressure_source=system\t\r\n"
      "system_memory_available=104857600\n"
      "system_memory_total=2147483648\n"
      "cobalt_memory_usage=943718400\n"
      "cobalt_memory_limit=1073741824");
  SbLowMemoryKillDetails expected = {
      kSbLowMemoryKillForegroundStatusBackground,
      kSbLowMemoryKillPressureSourceSystem,
      104857600ull,
      2147483648ull,
      943718400ull,
      1073741824ull,
  };
  ExpectDetailsEq(expected, GetDetailsFromPublicApi());
}

TEST_F(LowMemoryKillTest, Malformed_NonNumericValues) {
  SbLowMemoryKillDetails details = ParseLowMemoryKillMarker(
      "system_memory_available=abc\n"
      "system_memory_total=12abc\n"
      "cobalt_memory_usage=0x10\n"
      "cobalt_memory_limit=1 000\n"
      "pressure_source=cobalt\n");
  SbLowMemoryKillDetails expected = {};
  expected.pressure_source = kSbLowMemoryKillPressureSourceCobalt;
  ExpectDetailsEq(expected, details);
}

TEST_F(LowMemoryKillTest, Malformed_EmptyValues) {
  SbLowMemoryKillDetails details = ParseLowMemoryKillMarker(
      "foreground_status=\n"
      "system_memory_total=\n"
      "cobalt_memory_limit=4096\n");
  SbLowMemoryKillDetails expected = {};
  expected.cobalt_memory_limit = 4096;
  ExpectDetailsEq(expected, details);
}

TEST_F(LowMemoryKillTest, Malformed_NegativeValues) {
  SbLowMemoryKillDetails details = ParseLowMemoryKillMarker(
      "foreground_status=-1\n"
      "system_memory_total=-1\n"
      "cobalt_memory_usage=+5\n"
      "cobalt_memory_limit=4096\n");
  SbLowMemoryKillDetails expected = {};
  expected.cobalt_memory_limit = 4096;
  ExpectDetailsEq(expected, details);
}

TEST_F(LowMemoryKillTest, Malformed_OutOfRangeEnums) {
  SbLowMemoryKillDetails details = ParseLowMemoryKillMarker(
      "foreground_status=4\n"
      "pressure_source=99\n"
      "system_memory_total=8192\n");
  SbLowMemoryKillDetails expected = {};
  expected.system_memory_total = 8192;
  ExpectDetailsEq(expected, details);

  details = ParseLowMemoryKillMarker(
      "foreground_status=Foreground\n"
      "pressure_source=memory\n"
      "foreground_status=18446744073709551616\n"
      "system_memory_total=8192\n");
  ExpectDetailsEq(expected, details);
}

TEST_F(LowMemoryKillTest, Malformed_Uint64Overflow) {
  SbLowMemoryKillDetails details = ParseLowMemoryKillMarker(
      "cobalt_memory_usage=18446744073709551616\n"
      "system_memory_available=99999999999999999999999999\n"
      "system_memory_total=18446744073709551615\n");
  SbLowMemoryKillDetails expected = {};
  expected.system_memory_total = kUint64Max;
  ExpectDetailsEq(expected, details);
}

TEST_F(LowMemoryKillTest, Malformed_UnknownKeysAndNonEntries) {
  SbLowMemoryKillDetails details = ParseLowMemoryKillMarker(
      "1\n"
      "unknown_key=5\n"
      "FOREGROUND_STATUS=foreground\n"
      "=5\n"
      "===\n"
      "just some text\n"
      "foreground_status=foreground\n");
  SbLowMemoryKillDetails expected = {};
  expected.foreground_status = kSbLowMemoryKillForegroundStatusForeground;
  ExpectDetailsEq(expected, details);
}

TEST_F(LowMemoryKillTest, Malformed_GarbageAndBinaryLines) {
  const char kGarbage[] =
      "\xff\xfe\x00\x01\x02=\x80\n"
      "cobalt_memory_usage=12\x00"
      "34\n"
      "system_memory_total=\xff\n"
      "\x00\x00\x00\n"
      "pressure_source=other\n"
      "cobalt_memory_limit=4096\n";
  const std::string_view contents(kGarbage, sizeof(kGarbage) - 1);
  SbLowMemoryKillDetails details = ParseLowMemoryKillMarker(contents);
  SbLowMemoryKillDetails expected = {};
  expected.pressure_source = kSbLowMemoryKillPressureSourceOther;
  expected.cobalt_memory_limit = 4096;
  ExpectDetailsEq(expected, details);
}

TEST_F(LowMemoryKillTest, Malformed_DuplicateKeysLastWellFormedWins) {
  SbLowMemoryKillDetails details = ParseLowMemoryKillMarker(
      "system_memory_total=1\n"
      "system_memory_total=2\n"
      "cobalt_memory_limit=4096\n"
      "cobalt_memory_limit=bogus\n");
  SbLowMemoryKillDetails expected = {};
  expected.system_memory_total = 2;
  expected.cobalt_memory_limit = 4096;
  ExpectDetailsEq(expected, details);
}

TEST_F(LowMemoryKillTest, MalformedMarkerFileStillKilledWithValidFields) {
  const char kContents[] =
      "foreground_status=7\n"
      "pressure_source=cobalt\n"
      "system_memory_available=abc\n"
      "system_memory_total=-1\n"
      "cobalt_memory_usage=18446744073709551616\n"
      "cobalt_memory_limit=4294967296\n"
      "unknown_key=1\n"
      "\xff\x00garbage\n";
  CreateTmpMarkerWithContents(std::string(kContents, sizeof(kContents) - 1));
  SbLowMemoryKillDetails expected = {};
  expected.pressure_source = kSbLowMemoryKillPressureSourceCobalt;
  expected.cobalt_memory_limit = 4294967296ull;
  ExpectDetailsEq(expected, GetDetailsFromPublicApi());
}

TEST_F(LowMemoryKillTest, FifoMarkerDoesNotBlockAndIsKilled) {
  ASSERT_EQ(mkfifo(kTmpMarkerPath, 0600), 0);
  ExpectDetailsEmpty(GetDetailsFromPublicApi());
}

TEST_F(LowMemoryKillTest, MarkerThatCannotBeRemovedIsNotKilled) {
  // As before details were added, a kill is only reported if the marker is
  // actually removed. unlink() fails on a directory.
  ASSERT_EQ(mkdir(kTmpMarkerPath, 0700), 0);
  SbLowMemoryKillDetails details = kSentinelDetails;
  EXPECT_FALSE(api_->WasLowMemoryKilled());
  EXPECT_FALSE(api_->GetLowMemoryKillDetails(&details));
  ExpectDetailsEq(kSentinelDetails, details);
  EXPECT_TRUE(TmpMarkerExists());
  EXPECT_EQ(rmdir(kTmpMarkerPath), 0);
}

// --- Lifecycle semantics. ---

TEST_F(LowMemoryKillTest, ResultsAreCachedAfterMarkerIsConsumed) {
  CreateTmpMarkerWithDetails(kSampleDetails);
  ExpectDetailsEq(kSampleDetails, GetDetailsFromPublicApi());

  // A new marker written later in the same process must not change the cached
  // results, and must not be consumed.
  SbLowMemoryKillDetails other = {};
  other.foreground_status = kSbLowMemoryKillForegroundStatusOther;
  other.cobalt_memory_usage = 1;
  CreateTmpMarkerWithDetails(other);
  ASSERT_TRUE(TmpMarkerExists());

  SbLowMemoryKillDetails details = kSentinelDetails;
  EXPECT_TRUE(api_->WasLowMemoryKilled());
  EXPECT_TRUE(api_->GetLowMemoryKillDetails(&details));
  ExpectDetailsEq(kSampleDetails, details);
  EXPECT_TRUE(TmpMarkerExists());

  // After the test reset hook (simulating a new process), the new marker is
  // read.
  testing::ResetLowMemoryKillStateForTesting();
  ExpectDetailsEq(other, GetDetailsFromPublicApi());
}

TEST_F(LowMemoryKillTest, NotKilledResultIsCachedDespiteLaterMarker) {
  EXPECT_FALSE(api_->WasLowMemoryKilled());
  CreateTmpMarkerWithDetails(kSampleDetails);

  SbLowMemoryKillDetails details = kSentinelDetails;
  EXPECT_FALSE(api_->WasLowMemoryKilled());
  EXPECT_FALSE(api_->GetLowMemoryKillDetails(&details));
  ExpectDetailsEq(kSentinelDetails, details);
  EXPECT_TRUE(TmpMarkerExists());
}

TEST_F(LowMemoryKillTest, EnvOneOverridesDetailedMarkerWithEmptyDetails) {
  CreateTmpMarkerWithDetails(kSampleDetails);
  SetEnv("1");

  SbLowMemoryKillDetails details = kSentinelDetails;
  EXPECT_TRUE(api_->WasLowMemoryKilled());
  EXPECT_TRUE(api_->GetLowMemoryKillDetails(&details));
  ExpectDetailsEmpty(details);
  // The env-var override does not clear marker files.
  EXPECT_TRUE(TmpMarkerExists());

  details = kSentinelDetails;
  EXPECT_TRUE(testing::EvaluateLowMemoryKill(&details));
  ExpectDetailsEmpty(details);
}

TEST_F(LowMemoryKillTest, EnvZeroOverridesDetailedMarkerWithoutConsuming) {
  CreateTmpMarkerWithDetails(kSampleDetails);
  SetEnv("0");

  SbLowMemoryKillDetails details = kSentinelDetails;
  EXPECT_FALSE(api_->WasLowMemoryKilled());
  EXPECT_FALSE(api_->GetLowMemoryKillDetails(&details));
  ExpectDetailsEq(kSentinelDetails, details);
  EXPECT_TRUE(TmpMarkerExists());
}

TEST_F(LowMemoryKillTest, UnrecognizedEnvFallsBackToDetailedMarker) {
  CreateTmpMarkerWithDetails(kSampleDetails);
  SetEnv("maybe");
  ExpectDetailsEq(kSampleDetails, GetDetailsFromPublicApi());
}

TEST_F(LowMemoryKillTest, NullDetailsDoesNotConsumeDetailedMarker) {
  CreateTmpMarkerWithDetails(kSampleDetails);
  EXPECT_FALSE(api_->GetLowMemoryKillDetails(nullptr));
  EXPECT_TRUE(TmpMarkerExists());

  // The marker is still available for the first real evaluation.
  ExpectDetailsEq(kSampleDetails, GetDetailsFromPublicApi());
  // nullptr still returns false once killed and cached.
  EXPECT_FALSE(api_->GetLowMemoryKillDetails(nullptr));
}

TEST_F(LowMemoryKillTest, OversizedMarkerKeepsOnlyLinesWithinLimit) {
  constexpr size_t kLimit = 16 * 1024;
  const std::string head = "cobalt_memory_limit=4096\n";
  const std::string last = "system_memory_total=8192";
  const std::string filler(kLimit - head.size() - last.size() - 1, 'x');
  // |last| ends exactly at the limit; its newline is the first byte past it.
  CreateTmpMarkerWithContents(head + filler + "\n" + last +
                              "\ncobalt_memory_usage=1\n");
  SbLowMemoryKillDetails expected = {};
  expected.cobalt_memory_limit = 4096;
  expected.system_memory_total = 8192;
  ExpectDetailsEq(expected, GetDetailsFromPublicApi());

  // A value cut off by the limit is dropped rather than misread as 8192.
  testing::ResetLowMemoryKillStateForTesting();
  CreateTmpMarkerWithContents(head + filler + "\n" + last + "5\n");
  expected.system_memory_total = 0;
  ExpectDetailsEq(expected, GetDetailsFromPublicApi());
}

}  // namespace
}  // namespace starboard

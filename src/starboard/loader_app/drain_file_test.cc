// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/drain_file.h"

#include <string>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/loader_app/drain_file_helper.h"
#include "starboard/system.h"
#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace loader_app {
namespace {

const char kAppKeyOne[] = "b25lDQo=";
const char kAppKeyTwo[] = "dHdvDQo=";
const char kAppKeyThree[] = "dGhyZWUNCg==";

class DrainFileTest : public ::testing::Test {
 protected:
  // This function is used to set the temporary directory used for testing once,
  // and to create a subdir in this test directory, once for all test cases.
  static void SetUpTestCase() {
    std::vector<char> path(kSbFileMaxPath, 0);

    ASSERT_TRUE(
        SbSystemGetPath(kSbSystemPathTempDirectory, path.data(), path.size()));

    dir_.assign(path.data());
  }

  void TearDown() override {
    DrainFileClear(GetTempDir(), NULL, false);
  }

  const char* GetTempDir() const { return dir_.c_str(); }

 private:
  static std::string dir_;
};

std::string DrainFileTest::dir_ = "";

// Typical drain file usage.
TEST_F(DrainFileTest, SunnyDay) {
  EXPECT_TRUE(DrainFileTryDrain(GetTempDir(), kAppKeyOne));
  EXPECT_TRUE(DrainFileRankAndCheck(GetTempDir(), kAppKeyOne));
}

// Drain file creation should ignore expired files, even if it has a matching
// app key.
TEST_F(DrainFileTest, SunnyDayIgnoreExpired) {
  ScopedDrainFile stale(GetTempDir(), kAppKeyOne,
                        SbTimeGetNow() - kDrainFileMaximumAge);

  EXPECT_FALSE(DrainFileDraining(GetTempDir(), kAppKeyOne));
  EXPECT_TRUE(DrainFileTryDrain(GetTempDir(), kAppKeyOne));
  EXPECT_TRUE(DrainFileDraining(GetTempDir(), kAppKeyOne));
}

// Previously created drain file should be reused if it has not expired.
TEST_F(DrainFileTest, SunnyDayTryDrainReusePreviousDrainFile) {
  for (int i = 0; i < 2; ++i)
    EXPECT_TRUE(DrainFileTryDrain(GetTempDir(), kAppKeyOne));
}

// Draining status should return whether or not the file exists and has not yet
// expired.
TEST_F(DrainFileTest, SunnyDayDraining) {
  EXPECT_FALSE(DrainFileDraining(GetTempDir(), kAppKeyOne));
  EXPECT_TRUE(DrainFileTryDrain(GetTempDir(), kAppKeyOne));
  EXPECT_TRUE(DrainFileDraining(GetTempDir(), kAppKeyOne));

  DrainFileClear(GetTempDir(), NULL, false);

  EXPECT_FALSE(DrainFileDraining(GetTempDir(), kAppKeyOne));
}

// Remove an existing drain file.
TEST_F(DrainFileTest, SunnyDayRemove) {
  EXPECT_TRUE(DrainFileTryDrain(GetTempDir(), kAppKeyOne));
  EXPECT_TRUE(DrainFileDraining(GetTempDir(), kAppKeyOne));
  EXPECT_TRUE(DrainFileRemove(GetTempDir(), kAppKeyOne));
  EXPECT_FALSE(DrainFileDraining(GetTempDir(), kAppKeyOne));
}

// Try to remove a drain file that does not exist.
TEST_F(DrainFileTest, SunnyDayRemoveNonexistentDrainFile) {
  EXPECT_FALSE(DrainFileDraining(GetTempDir(), kAppKeyOne));
  EXPECT_TRUE(DrainFileRemove(GetTempDir(), kAppKeyOne));
}

// When clearing drain files it should be possible to provide a file to ignore,
// and it should be possible to only clear expire files if desired.
TEST_F(DrainFileTest, SunnyDayClear) {
  EXPECT_TRUE(DrainFileTryDrain(GetTempDir(), kAppKeyOne));

  ScopedDrainFile valid_file(GetTempDir(), kAppKeyTwo, SbTimeGetNow());
  ScopedDrainFile stale_file(GetTempDir(), kAppKeyThree,
                             SbTimeGetNow() - kDrainFileMaximumAge);

  EXPECT_TRUE(DrainFileDraining(GetTempDir(), kAppKeyOne));
  EXPECT_TRUE(DrainFileDraining(GetTempDir(), kAppKeyTwo));
  EXPECT_TRUE(SbFileExists(stale_file.path().c_str()));

  DrainFileClear(GetTempDir(), kAppKeyOne, true);

  EXPECT_TRUE(DrainFileDraining(GetTempDir(), kAppKeyOne));
  EXPECT_TRUE(DrainFileDraining(GetTempDir(), kAppKeyTwo));
  EXPECT_FALSE(SbFileExists(stale_file.path().c_str()));

  DrainFileClear(GetTempDir(), kAppKeyOne, false);

  EXPECT_TRUE(DrainFileDraining(GetTempDir(), kAppKeyOne));
  EXPECT_FALSE(DrainFileDraining(GetTempDir(), kAppKeyTwo));

  DrainFileClear(GetTempDir(), NULL, false);

  EXPECT_FALSE(DrainFileDraining(GetTempDir(), kAppKeyOne));
}

// Ranking drain files should first be done by timestamp, with the app key being
// used as a tie breaker.
TEST_F(DrainFileTest, SunnyDayRankCorrectlyRanksFiles) {
  const SbTime timestamp = SbTimeGetNow();

  ScopedDrainFile early_and_least(GetTempDir(), "a", timestamp);
  ScopedDrainFile later_and_least(GetTempDir(), "c", timestamp);
  ScopedDrainFile later_and_greatest(GetTempDir(), "b",
                                     timestamp + kDrainFileAgeUnit);

  std::vector<char> result(kSbFileMaxName);

  EXPECT_TRUE(DrainFileRankAndCheck(GetTempDir(), "a"));
  EXPECT_TRUE(SbFileDelete(early_and_least.path().c_str()));

  EXPECT_TRUE(DrainFileRankAndCheck(GetTempDir(), "c"));
  EXPECT_TRUE(SbFileDelete(later_and_least.path().c_str()));

  EXPECT_TRUE(DrainFileRankAndCheck(GetTempDir(), "b"));
  EXPECT_TRUE(SbFileDelete(later_and_greatest.path().c_str()));
}

// All files in the directory should be cleared except for drain files with an
// app key matching the provided app key.
TEST_F(DrainFileTest, SunnyDayPrepareDirectory) {
  EXPECT_TRUE(DrainFileTryDrain(GetTempDir(), kAppKeyOne));

  // Create a directory to delete.
  std::string dir(GetTempDir());
  dir.append(kSbFileSepString);
  dir.append("to_delete");

  EXPECT_TRUE(SbDirectoryCreate(dir.c_str()));
  EXPECT_TRUE(SbFileExists(dir.c_str()));

  // Create a file with the app key in the name.
  std::string path(GetTempDir());
  path.append(kSbFileSepString);
  path.append(kAppKeyOne);

  {
    ScopedFile file(path.c_str(), kSbFileOpenAlways | kSbFileWrite, NULL, NULL);
  }

  EXPECT_TRUE(SbFileExists(path.c_str()));

  DrainFilePrepareDirectory(GetTempDir(), kAppKeyOne);

  EXPECT_TRUE(DrainFileRankAndCheck(GetTempDir(), kAppKeyOne));
  EXPECT_FALSE(SbFileExists(dir.c_str()));
  EXPECT_FALSE(SbFileExists(path.c_str()));

  DrainFilePrepareDirectory(GetTempDir(), "nonexistent");

  EXPECT_FALSE(DrainFileRankAndCheck(GetTempDir(), kAppKeyOne));
}

// Creating a new drain file in the same directory as an existing, valid drain
// file should fail.
TEST_F(DrainFileTest, RainyDayDrainFileAlreadyExists) {
  EXPECT_TRUE(DrainFileTryDrain(GetTempDir(), kAppKeyOne));
  EXPECT_FALSE(DrainFileTryDrain(GetTempDir(), kAppKeyTwo));
}

}  // namespace
}  // namespace loader_app
}  // namespace starboard

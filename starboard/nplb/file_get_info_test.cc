// Copyright 2015 Google Inc. All Rights Reserved.
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

// GetInfo is mostly tested in the course of other tests.

#include <string>

#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbFileGetInfoTest, InvalidFileErrors) {
  SbFileInfo info = {0};
  bool result = SbFileGetInfo(kSbFileInvalid, &info);
  EXPECT_FALSE(result);
}

TEST(SbFileGetInfoTest, WorksOnARegularFile) {
  // This test is potentially flaky because it's comparing times. So, building
  // in extra sensitivity to make flakiness more apparent.
  const int kTrials = 100;
  for (int i = 0; i < kTrials; ++i) {
    // Assuming platforms have at least 1 second precision on filesystem
    // timestamps, we need to go back two seconds to avoid rounding issues.
    SbTime time = SbTimeGetNow() - (2 * kSbTimeSecond);

    const int kFileSize = 12;
    starboard::nplb::ScopedRandomFile random_file(kFileSize);
    const std::string& filename = random_file.filename();

    SbFile file =
        SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
    ASSERT_TRUE(SbFileIsValid(file));

    {
      SbFileInfo info = {0};
      bool result = SbFileGetInfo(file, &info);
      EXPECT_EQ(kFileSize, info.size);
      EXPECT_FALSE(info.is_directory);
      EXPECT_FALSE(info.is_symbolic_link);
      EXPECT_LE(time, info.last_modified);
      EXPECT_LE(time, info.last_accessed);
      EXPECT_LE(time, info.creation_time);
    }

    bool result = SbFileClose(file);
    EXPECT_TRUE(result);
  }
}

TEST(SbFileGetInfoTest, WorksOnADirectory) {
  char path[SB_FILE_MAX_PATH] = {0};
  bool result =
      SbSystemGetPath(kSbSystemPathTempDirectory, path, SB_FILE_MAX_PATH);
  EXPECT_TRUE(result);

  SbFile file = SbFileOpen(path, kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

  {
    SbFileInfo info = {0};
    bool result = SbFileGetInfo(file, &info);
    EXPECT_LE(0, info.size);
    EXPECT_TRUE(info.is_directory);
    EXPECT_FALSE(info.is_symbolic_link);
    EXPECT_LE(0, info.last_modified);
    EXPECT_LE(0, info.last_accessed);
    EXPECT_LE(0, info.creation_time);
  }

  result = SbFileClose(file);
  EXPECT_TRUE(result);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

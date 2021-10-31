// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbFileGetPathInfoTest, InvalidFileErrors) {
  SbFileInfo info = {0};
  bool result = SbFileGetPathInfo(NULL, &info);
  EXPECT_FALSE(result);

  result = SbFileGetPathInfo("", &info);
  EXPECT_FALSE(result);

  result = SbFileGetPathInfo(".", NULL);
  EXPECT_FALSE(result);

  result = SbFileGetPathInfo(NULL, NULL);
  EXPECT_FALSE(result);

  result = SbFileGetPathInfo("", NULL);
  EXPECT_FALSE(result);
}

TEST(SbFileGetPathInfoTest, WorksOnARegularFile) {
  // This test is potentially flaky because it's comparing times. So, building
  // in extra sensitivity to make flakiness more apparent.
  const int kTrials = 100;
  for (int i = 0; i < kTrials; ++i) {
    // We can't assume filesystem timestamp precision, so go back a minute
    // for a better chance to contain the imprecision and rounding errors.
    SbTime time = SbTimeGetNow() - kSbTimeMinute;
#if !SB_HAS_QUIRK(FILESYSTEM_ZERO_FILEINFO_TIME)
#if SB_HAS_QUIRK(FILESYSTEM_COARSE_ACCESS_TIME)
    // On platforms with coarse access time, we assume 1 day precision and go
    // back 2 days to avoid rounding issues.
    SbTime coarse_time = SbTimeGetNow() - (2 * kSbTimeDay);
#endif  // FILESYSTEM_COARSE_ACCESS_TIME
#endif  // FILESYSTEM_ZERO_FILEINFO_TIME

    const int kFileSize = 12;
    ScopedRandomFile random_file(kFileSize);
    const std::string& filename = random_file.filename();

    {
      SbFileInfo info = {0};
      bool result = SbFileGetPathInfo(filename.c_str(), &info);
      EXPECT_EQ(kFileSize, info.size);
      EXPECT_FALSE(info.is_directory);
      EXPECT_FALSE(info.is_symbolic_link);
#if SB_HAS_QUIRK(FILESYSTEM_ZERO_FILEINFO_TIME)
      EXPECT_LE(0, info.last_accessed);
      EXPECT_LE(0, info.last_accessed);
      EXPECT_LE(0, info.creation_time);
#else
      EXPECT_LE(time, info.last_modified);
#if SB_HAS_QUIRK(FILESYSTEM_COARSE_ACCESS_TIME)
      EXPECT_LE(coarse_time, info.last_accessed);
#else
      EXPECT_LE(time, info.last_accessed);
#endif  // FILESYSTEM_COARSE_ACCESS_TIME
      EXPECT_LE(time, info.creation_time);
#endif  // FILESYSTEM_ZERO_FILEINFO_TIME
    }
  }
}

TEST(SbFileGetPathInfoTest, WorksOnADirectory) {
  std::vector<char> path(kSbFileMaxPath);
  bool result =
      SbSystemGetPath(kSbSystemPathTempDirectory, path.data(), kSbFileMaxPath);
  EXPECT_TRUE(result);

  {
    SbFileInfo info = {0};
    bool result = SbFileGetPathInfo(path.data(), &info);
    EXPECT_LE(0, info.size);
    EXPECT_TRUE(info.is_directory);
    EXPECT_FALSE(info.is_symbolic_link);
    EXPECT_LE(0, info.last_modified);
    EXPECT_LE(0, info.last_accessed);
    EXPECT_LE(0, info.creation_time);
  }
}

TEST(SbFileGetPathInfoTest, WorksOnStaticContentFiles) {
  for (auto filename : GetFileTestsFilePaths()) {
    SbFileInfo info = {0};
    bool result = SbFileGetPathInfo(filename.c_str(), &info);
    size_t content_length = GetTestFileExpectedContent(filename).length();
    EXPECT_EQ(content_length, info.size);
    EXPECT_FALSE(info.is_directory);
    EXPECT_FALSE(info.is_symbolic_link);
  }
}

TEST(SbFileGetPathInfoTest, WorksOnStaticContentDirectories) {
  for (auto path : GetFileTestsDirectoryPaths()) {
    SbFileInfo info = {0};
    bool result = SbFileGetPathInfo(path.data(), &info);
    EXPECT_LE(0, info.size);
    EXPECT_TRUE(info.is_directory);
    EXPECT_FALSE(info.is_symbolic_link);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

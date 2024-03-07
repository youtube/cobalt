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

// GetInfo is mostly tested in the course of other tests.

#include <sys/stat.h>

#include <string>

#include "starboard/common/time.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixFileGetPathInfoTest, InvalidFileErrors) {
  struct stat file_info;

  EXPECT_FALSE(stat("", &file_info) == 0);

  EXPECT_TRUE(stat(".", &file_info) == 0);
}

TEST(PosixFileGetPathInfoTest, WorksOnARegularFile) {
  // This test is potentially flaky because it's comparing times. So, building
  // in extra sensitivity to make flakiness more apparent.
  const int kTrials = 100;
  for (int i = 0; i < kTrials; ++i) {
    // We can't assume filesystem timestamp precision, so go back a minute
    // for a better chance to contain the imprecision and rounding errors.
    const int64_t kOneMinuteInMicroseconds = 60'000'000;
    int64_t time = CurrentPosixTime() - kOneMinuteInMicroseconds;
#if !SB_HAS_QUIRK(FILESYSTEM_ZERO_FILEINFO_TIME)
#if SB_HAS_QUIRK(FILESYSTEM_COARSE_ACCESS_TIME)
    // On platforms with coarse access time, we assume 1 day precision and go
    // back 2 days to avoid rounding issues.
    const int64_t kOneDayInMicroseconds = 1'000'000LL * 60LL * 60LL * 24LL;
    int64_t coarse_time = PosixTimeToWindowsTime(CurrentPosixTime()) -
                          (2 * kOneDayInMicroseconds);
#endif  // FILESYSTEM_COARSE_ACCESS_TIME
#endif  // FILESYSTEM_ZERO_FILEINFO_TIME

    const int kFileSize = 12;
    ScopedRandomFile random_file(kFileSize);
    const std::string& filename = random_file.filename();

    {
      struct stat file_info;
      EXPECT_TRUE(stat(filename.c_str(), &file_info) == 0);
      EXPECT_EQ(kFileSize, file_info.st_size);
      EXPECT_FALSE(S_ISDIR(file_info.st_mode));
      EXPECT_FALSE(S_ISLNK(file_info.st_mode));
#if SB_HAS_QUIRK(FILESYSTEM_ZERO_FILEINFO_TIME)
      EXPECT_LE(0, static_cast<int64_t>(file_info.at_ctime) * 1000000);
      EXPECT_LE(0, static_cast<int64_t>(file_info.mt_ctime) * 1000000);
      EXPECT_LE(0, static_cast<int64_t>(file_info.st_ctime) * 1000000);
#else
      EXPECT_LE(time, static_cast<int64_t>(file_info.st_mtime) * 1000000);
#if SB_HAS_QUIRK(FILESYSTEM_COARSE_ACCESS_TIME)
      EXPECT_LE(coarse_time,
                static_cast<int64_t>(file_info.at_ctime) * 1000000);
#else
      EXPECT_LE(time, static_cast<int64_t>(file_info.st_atime) * 1000000);
#endif  // FILESYSTEM_COARSE_ACCESS_TIME
      EXPECT_LE(time, static_cast<int64_t>(file_info.st_ctime) * 1000000);
#endif  // FILESYSTEM_ZERO_FILEINFO_TIME
    }
  }
}

TEST(PosixFileGetPathInfoTest, WorksOnADirectory) {
  std::vector<char> path(kSbFileMaxPath);
  bool result =
      SbSystemGetPath(kSbSystemPathTempDirectory, path.data(), kSbFileMaxPath);
  EXPECT_TRUE(result);

  {
    struct stat file_info;
    bool result = stat(path.data(), &file_info) == 0;
    EXPECT_LE(0, file_info.st_size);
    EXPECT_TRUE(S_ISDIR(file_info.st_mode));
    EXPECT_FALSE(S_ISLNK(file_info.st_mode));
    EXPECT_LE(0, static_cast<int64_t>(file_info.st_mtime) * 1000000);
    EXPECT_LE(0, static_cast<int64_t>(file_info.st_atime) * 1000000);
    EXPECT_LE(0, static_cast<int64_t>(file_info.st_ctime) * 1000000);
  }
}

TEST(PosixFileGetPathInfoTest, WorksOnStaticContentFiles) {
  for (auto filename : GetFileTestsFilePaths()) {
    SbFileInfo info = {0};
    bool result = SbFileGetPathInfo(filename.c_str(), &info);
    size_t content_length = GetTestFileExpectedContent(filename).length();
    EXPECT_EQ(content_length, info.size);
    EXPECT_FALSE(info.is_directory);
    EXPECT_FALSE(info.is_symbolic_link);
  }
}

TEST(PosixFileGetPathInfoTest, WorksOnStaticContentDirectories) {
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

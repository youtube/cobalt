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

constexpr int64_t kMicrosecond = 1'000'000;
auto ToMicroseconds(const struct timespec& ts) {
  return ts.tv_sec * kMicrosecond + ts.tv_nsec / 1000;
}

TEST(PosixFileGetPathInfoTest, WorksOnARegularFile) {
  // This test is potentially flaky because it's comparing times. So, building
  // in extra sensitivity to make flakiness more apparent.
  constexpr int kTrials = 100;
  for (int i = 0; i < kTrials; ++i) {
    int64_t time_usec = CurrentPosixTime();

    constexpr int kFileSize = 12;
    ScopedRandomFile random_file(kFileSize);
    const std::string& filename = random_file.filename();

    {
      struct stat file_info;
      EXPECT_TRUE(stat(filename.c_str(), &file_info) == 0);
      EXPECT_EQ(kFileSize, file_info.st_size);
      EXPECT_FALSE(S_ISDIR(file_info.st_mode));
      EXPECT_FALSE(S_ISLNK(file_info.st_mode));
      // We can't assume filesystem timestamp precision, so allow a minute
      // difference.
      constexpr int64_t kOneMinute = 60 * kMicrosecond;
      EXPECT_NEAR(time_usec, ToMicroseconds(file_info.st_mtim), kOneMinute);
      EXPECT_NEAR(time_usec, ToMicroseconds(file_info.st_atim), kOneMinute);
      EXPECT_NEAR(time_usec, ToMicroseconds(file_info.st_ctim), kOneMinute);
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
    EXPECT_TRUE(stat(path.data(), &file_info) == 0);
    EXPECT_LE(0, file_info.st_size);
    EXPECT_TRUE(S_ISDIR(file_info.st_mode));
    EXPECT_FALSE(S_ISLNK(file_info.st_mode));
    EXPECT_LE(0, file_info.st_mtime);
    EXPECT_LE(0, file_info.st_atime);
    EXPECT_LE(0, file_info.st_ctime);
  }
}

TEST(PosixFileGetPathInfoTest, WorksOnStaticContentFiles) {
  for (auto filename : GetFileTestsFilePaths()) {
    struct stat info;
    EXPECT_TRUE(stat(filename.c_str(), &info) == 0);
    size_t content_length = GetTestFileExpectedContent(filename).length();
    EXPECT_EQ(static_cast<long>(content_length), info.st_size);
    EXPECT_FALSE(S_ISDIR(info.st_mode));
    EXPECT_FALSE(S_ISLNK(info.st_mode));
  }
}

TEST(PosixFileGetPathInfoTest, WorksOnStaticContentDirectories) {
  for (auto path : GetFileTestsDirectoryPaths()) {
    struct stat info;
    EXPECT_TRUE(stat(path.data(), &info) == 0);
    EXPECT_LE(0, info.st_size);
    EXPECT_TRUE(S_ISDIR(info.st_mode));
    EXPECT_FALSE(S_ISLNK(info.st_mode));
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

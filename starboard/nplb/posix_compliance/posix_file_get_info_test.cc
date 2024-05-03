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

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "starboard/common/time.h"
#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

inline int64_t TimeTToWindowsUsec(time_t time) {
  int64_t posix_usec = static_cast<int64_t>(time) * 1000000;
  return PosixTimeToWindowsTime(posix_usec);
}

TEST(PosixFileGetInfoTest, InvalidFileErrors) {
  struct stat info;
  int result = fstat(-1, &info);
  EXPECT_FALSE(result == 0);
}

TEST(PosixFileGetInfoTest, WorksOnARegularFile) {
  // This test is potentially flaky because it's comparing times. So, building
  // in extra sensitivity to make flakiness more apparent.
  const int kTrials = 100;
  for (int i = 0; i < kTrials; ++i) {
    // We can't assume filesystem timestamp precision, so go back a minute
    // for a better chance to contain the imprecision and rounding errors.
    const int64_t kOneMinuteInMicroseconds = 60'000'000;
    int64_t time =
        PosixTimeToWindowsTime(CurrentPosixTime()) - kOneMinuteInMicroseconds;

    const int kFileSize = 12;
    starboard::nplb::ScopedRandomFile random_file(kFileSize);
    const std::string& filename = random_file.filename();

    int file = open(filename.c_str(), O_RDONLY);
    ASSERT_TRUE(file >= 0);

    {
      struct stat info;
      int result = fstat(file, &info);
      EXPECT_EQ(kFileSize, info.st_size);
      EXPECT_FALSE(S_ISDIR(info.st_mode));
      EXPECT_FALSE(S_ISLNK(info.st_mode));
      EXPECT_LE(time, TimeTToWindowsUsec(info.st_atime));
      EXPECT_LE(time, TimeTToWindowsUsec(info.st_atime));
      EXPECT_LE(time, TimeTToWindowsUsec(info.st_ctime));
    }

    int result = close(file);
    EXPECT_TRUE(result == 0);
  }
}

TEST(PosixFileGetInfoTest, WorksOnStaticContentFiles) {
  int count = 1;
  for (auto filename : GetFileTestsFilePaths()) {
    int file = open(filename.c_str(), O_RDONLY);
    ASSERT_TRUE(file >= 0);

    struct stat info;
    EXPECT_TRUE(fstat(file, &info) == 0);
    size_t content_length = GetTestFileExpectedContent(filename).length();
    EXPECT_EQ(content_length, info.st_size);
    EXPECT_FALSE(S_ISDIR(info.st_mode));
    EXPECT_FALSE(S_ISLNK(info.st_mode));

    EXPECT_TRUE(close(file) == 0);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

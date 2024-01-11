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

#include "starboard/common/time.h"
#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
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
    // We can't assume filesystem timestamp precision, so go back a minute
    // for a better chance to contain the imprecision and rounding errors.
    const int64_t kOneMinuteInMicroseconds = 60'000'000;
    int64_t time =
        PosixTimeToWindowsTime(CurrentPosixTime()) - kOneMinuteInMicroseconds;
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

    bool result = SbFileClose(file);
    EXPECT_TRUE(result);
  }
}

TEST(SbFileGetInfoTest, WorksOnStaticContentFiles) {
  for (auto filename : GetFileTestsFilePaths()) {
    SbFile file =
        SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
    ASSERT_TRUE(SbFileIsValid(file));

    SbFileInfo info = {0};
    bool result = SbFileGetInfo(file, &info);
    size_t content_length = GetTestFileExpectedContent(filename).length();
    EXPECT_EQ(content_length, info.size);
    EXPECT_FALSE(info.is_directory);
    EXPECT_FALSE(info.is_symbolic_link);

    EXPECT_TRUE(SbFileClose(file));
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

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

#include <string>

#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const std::string kManyFileSeparators =  // NOLINT(runtime/string)
    std::string(kSbFileSepString) + kSbFileSepString + kSbFileSepString +
    kSbFileSepString;

// NOTE: There is a test missing here, for creating a directory right off of the
// root. But, this is likely to fail due to permissions, so we can't make a
// reliable test for this.

TEST(PosixDirectoryCreateTest, SunnyDay) {
  ScopedRandomFile dir(ScopedRandomFile::kDontCreate);

  const std::string& path = dir.filename();

  struct stat info;
  EXPECT_FALSE(stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode));
  EXPECT_TRUE(mkdir(path.c_str(), 0700) == 0 ||
              stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode));
  EXPECT_TRUE(stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode));

  // Should return true if called redundantly.
  EXPECT_TRUE(mkdir(path.c_str(), 0700) == 0 ||
              stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode));
}

TEST(PosixDirectoryCreateTest, SunnyDayTrailingSeparators) {
  ScopedRandomFile dir(ScopedRandomFile::kDontCreate);

  std::string path = dir.filename() + kManyFileSeparators.c_str();

  struct stat info;
  EXPECT_FALSE(stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode));
  EXPECT_TRUE(mkdir(path.c_str(), 0700) == 0 ||
              stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode));
}

TEST(PosixDirectoryCreateTest, SunnyDayTempDirectory) {
  std::vector<char> temp_path(kSbFileMaxPath);
  bool system_path_success = SbSystemGetPath(kSbSystemPathTempDirectory,
                                             temp_path.data(), kSbFileMaxPath);
  ASSERT_TRUE(system_path_success);
  struct stat info;
  EXPECT_TRUE(stat(temp_path.data(), &info) == 0 && S_ISDIR(info.st_mode));
  EXPECT_TRUE(mkdir(temp_path.data(), 0700) == 0 ||
              stat(temp_path.data(), &info) == 0 && S_ISDIR(info.st_mode));
}

TEST(PosixDirectoryCreateTest, SunnyDayTempDirectoryManySeparators) {
  std::vector<char> temp_path(kSbFileMaxPath);
  bool system_path_success = SbSystemGetPath(
      kSbSystemPathTempDirectory, temp_path.data(), temp_path.size());
  ASSERT_TRUE(system_path_success);
  const int new_size = starboard::strlcat(
      temp_path.data(), kManyFileSeparators.c_str(), kSbFileMaxPath);
  ASSERT_LT(new_size, static_cast<int>(kSbFileMaxPath));

  struct stat info;
  EXPECT_TRUE(stat(temp_path.data(), &info) == 0 && S_ISDIR(info.st_mode));
  EXPECT_TRUE(mkdir(temp_path.data(), 0700) == 0 ||
              stat(temp_path.data(), &info) == 0 && S_ISDIR(info.st_mode));
}

TEST(PosixDirectoryCreateTest, FailureEmptyPath) {
  EXPECT_FALSE(mkdir("", 0700) == 0);
}

TEST(PosixDirectoryCreateTest, FailureNonexistentParent) {
  ScopedRandomFile dir(ScopedRandomFile::kDontCreate);
  std::string path = dir.filename() + kSbFileSepString + "test";

  struct stat info;
  EXPECT_FALSE(stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode));
  EXPECT_FALSE(mkdir(path.c_str(), 0700) == 0 ||
               stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

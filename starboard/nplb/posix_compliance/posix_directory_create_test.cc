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

#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/file.h"
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

  EXPECT_FALSE(SbDirectoryCanOpen(path.c_str()));
  EXPECT_TRUE(mkdir(path.c_str(), 0700) == 0 ||
              SbDirectoryCanOpen(path.c_str()));
  EXPECT_TRUE(SbDirectoryCanOpen(path.c_str()));

  // Should return true if called redundantly.
  EXPECT_TRUE(mkdir(path.c_str(), 0700) == 0 ||
              SbDirectoryCanOpen(path.c_str()));
  EXPECT_TRUE(SbDirectoryCanOpen(path.c_str()));
}

TEST(PosixDirectoryCreateTest, SunnyDayTrailingSeparators) {
  ScopedRandomFile dir(ScopedRandomFile::kDontCreate);

  std::string path = dir.filename() + kManyFileSeparators.c_str();

  EXPECT_FALSE(SbDirectoryCanOpen(path.c_str()));
  EXPECT_TRUE(mkdir(path.c_str(), 0700) == 0 ||
              SbDirectoryCanOpen(path.c_str()));
  EXPECT_TRUE(SbDirectoryCanOpen(path.c_str()));
}

TEST(PosixDirectoryCreateTest, SunnyDayTempDirectory) {
  std::vector<char> temp_path(kSbFileMaxPath);
  bool system_path_success = SbSystemGetPath(kSbSystemPathTempDirectory,
                                             temp_path.data(), kSbFileMaxPath);
  ASSERT_TRUE(system_path_success);
  EXPECT_TRUE(SbDirectoryCanOpen(temp_path.data()));
  EXPECT_TRUE(mkdir(temp_path.data(), 0700) == 0 ||
              SbDirectoryCanOpen(temp_path.data()));
  EXPECT_TRUE(SbDirectoryCanOpen(temp_path.data()));
}

TEST(PosixDirectoryCreateTest, SunnyDayTempDirectoryManySeparators) {
  std::vector<char> temp_path(kSbFileMaxPath);
  bool system_path_success = SbSystemGetPath(
      kSbSystemPathTempDirectory, temp_path.data(), temp_path.size());
  ASSERT_TRUE(system_path_success);
  const int new_size = starboard::strlcat(
      temp_path.data(), kManyFileSeparators.c_str(), kSbFileMaxPath);
  ASSERT_LT(new_size, kSbFileMaxPath);

  EXPECT_TRUE(SbDirectoryCanOpen(temp_path.data()));
  EXPECT_TRUE(mkdir(temp_path.data(), 0700) == 0 ||
              SbDirectoryCanOpen(temp_path.data()));
  EXPECT_TRUE(SbDirectoryCanOpen(temp_path.data()));
}

TEST(PosixDirectoryCreateTest, FailureEmptyPath) {
  EXPECT_FALSE(mkdir("", 0700) == 0);
}

TEST(PosixDirectoryCreateTest, FailureNonexistentParent) {
  ScopedRandomFile dir(ScopedRandomFile::kDontCreate);
  std::string path = dir.filename() + kSbFileSepString + "test";

  EXPECT_FALSE(SbDirectoryCanOpen(path.c_str()));
  EXPECT_FALSE(mkdir(path.c_str(), 0700) == 0 ||
               SbDirectoryCanOpen(path.c_str()));
  EXPECT_FALSE(SbDirectoryCanOpen(path.c_str()));
}

// TEST(PosixDirectoryCreateTest, FailureNotAbsolute) {
//   const char* kPath = "hallo";

//   EXPECT_FALSE(SbDirectoryCanOpen(kPath));
//   EXPECT_FALSE(mkdir(kPath, 0700) == 0 || SbDirectoryCanOpen(kPath));
//   EXPECT_FALSE(SbDirectoryCanOpen(kPath));
// }

}  // namespace
}  // namespace nplb
}  // namespace starboard

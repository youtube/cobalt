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

const std::string kManyFileSeparators = std::string(kSbFileSepString) +
                                        kSbFileSepString + kSbFileSepString +
                                        kSbFileSepString;

// NOTE: There is a test missing here, for creating a directory right off of the
// root. But, this is likely to fail due to permissions, so we can't make a
// reliable test for this.

TEST(SbDirectoryCreateTest, SunnyDay) {
  ScopedRandomFile dir(ScopedRandomFile::kDontCreate);

  const std::string& path = dir.filename();

  EXPECT_FALSE(SbDirectoryCanOpen(path.c_str()));
  EXPECT_TRUE(SbDirectoryCreate(path.c_str()));
  EXPECT_TRUE(SbDirectoryCanOpen(path.c_str()));

  // Should return true if called redundantly.
  EXPECT_TRUE(SbDirectoryCreate(path.c_str()));
  EXPECT_TRUE(SbDirectoryCanOpen(path.c_str()));
}

TEST(SbDirectoryCreateTest, SunnyDayTrailingSeparators) {
  ScopedRandomFile dir(ScopedRandomFile::kDontCreate);

  std::string path = dir.filename() + kManyFileSeparators.c_str();

  EXPECT_FALSE(SbDirectoryCanOpen(path.c_str()));
  EXPECT_TRUE(SbDirectoryCreate(path.c_str()));
  EXPECT_TRUE(SbDirectoryCanOpen(path.c_str()));
}

TEST(SbDirectoryCreateTest, SunnyDayTempDirectory) {
  std::vector<char> temp_path(kSbFileMaxPath);
  bool system_path_success = SbSystemGetPath(kSbSystemPathTempDirectory,
                                             temp_path.data(), kSbFileMaxPath);
  ASSERT_TRUE(system_path_success);
  EXPECT_TRUE(SbDirectoryCanOpen(temp_path.data()));
  EXPECT_TRUE(SbDirectoryCreate(temp_path.data()));
  EXPECT_TRUE(SbDirectoryCanOpen(temp_path.data()));
}

TEST(SbDirectoryCreateTest, SunnyDayTempDirectoryManySeparators) {
  std::vector<char> temp_path(kSbFileMaxPath);
  bool system_path_success = SbSystemGetPath(
      kSbSystemPathTempDirectory, temp_path.data(), temp_path.size());
  ASSERT_TRUE(system_path_success);
  const int new_size = starboard::strlcat(
      temp_path.data(), kManyFileSeparators.c_str(), kSbFileMaxPath);
  ASSERT_LT(new_size, kSbFileMaxPath);

  EXPECT_TRUE(SbDirectoryCanOpen(temp_path.data()));
  EXPECT_TRUE(SbDirectoryCreate(temp_path.data()));
  EXPECT_TRUE(SbDirectoryCanOpen(temp_path.data()));
}

TEST(SbDirectoryCreateTest, FailureNullPath) {
  EXPECT_FALSE(SbDirectoryCreate(NULL));
}

TEST(SbDirectoryCreateTest, FailureEmptyPath) {
  EXPECT_FALSE(SbDirectoryCreate(""));
}

TEST(SbDirectoryCreateTest, FailureNonexistentParent) {
  ScopedRandomFile dir(ScopedRandomFile::kDontCreate);
  std::string path = dir.filename() + kSbFileSepString + "test";

  EXPECT_FALSE(SbDirectoryCanOpen(path.c_str()));
  EXPECT_FALSE(SbDirectoryCreate(path.c_str()));
  EXPECT_FALSE(SbDirectoryCanOpen(path.c_str()));
}

TEST(SbDirectoryCreateTest, FailureNotAbsolute) {
  const char* kPath = "hallo";

  EXPECT_FALSE(SbDirectoryCanOpen(kPath));
  EXPECT_FALSE(SbDirectoryCreate(kPath));
  EXPECT_FALSE(SbDirectoryCanOpen(kPath));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

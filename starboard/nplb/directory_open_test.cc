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

#include <string>

#include "starboard/directory.h"
#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbDirectoryOpenTest, SunnyDay) {
  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(SbFileExists(path.c_str()));

  SbFileError error = kSbFileErrorMax;
  SbDirectory directory = SbDirectoryOpen(path.c_str(), &error);
  EXPECT_TRUE(SbDirectoryIsValid(directory));
  EXPECT_EQ(kSbFileOk, error);
  EXPECT_TRUE(SbDirectoryClose(directory));
}

TEST(SbDirectoryOpenTest, SunnyDayWithNullError) {
  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(SbFileExists(path.c_str()));

  SbDirectory directory = SbDirectoryOpen(path.c_str(), NULL);
  EXPECT_TRUE(SbDirectoryIsValid(directory));
  EXPECT_TRUE(SbDirectoryClose(directory));
}

TEST(SbDirectoryOpenTest, ManySunnyDay) {
  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(SbFileExists(path.c_str()));

  const int kMany = SB_FILE_MAX_OPEN;
  SbDirectory directories[kMany] = {0};

  for (int i = 0; i < SB_ARRAY_SIZE_INT(directories); ++i) {
    SbFileError error = kSbFileErrorMax;
    directories[i] = SbDirectoryOpen(path.c_str(), &error);
    EXPECT_TRUE(SbDirectoryIsValid(directories[i]));
    EXPECT_EQ(kSbFileOk, error);
  }

  for (int i = 0; i < SB_ARRAY_SIZE_INT(directories); ++i) {
    EXPECT_TRUE(SbDirectoryClose(directories[i]));
  }
}

TEST(SbDirectoryOpenTest, FailsInvalidPath) {
  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(SbFileExists(path.c_str()));

  // Funny way to make sure the directory seems valid but doesn't exist.
  int len = static_cast<int>(path.length());
  if (path[len - 1] != 'z') {
    path[len - 1] = 'z';
  } else {
    path[len - 1] = 'y';
  }

  ASSERT_FALSE(SbFileExists(path.c_str()));

  SbFileError error = kSbFileErrorMax;
  SbDirectory directory = SbDirectoryOpen(path.c_str(), &error);
  EXPECT_FALSE(SbDirectoryIsValid(directory));
  EXPECT_EQ(kSbFileErrorNotFound, error);
  if (SbDirectoryIsValid(directory)) {
    SbDirectoryClose(directory);
  }
}

TEST(SbDirectoryOpenTest, FailsNullPath) {
  SbFileError error = kSbFileErrorMax;
  SbDirectory directory = SbDirectoryOpen(NULL, &error);
  EXPECT_FALSE(SbDirectoryIsValid(directory));
  EXPECT_EQ(kSbFileErrorNotFound, error);
  if (SbDirectoryIsValid(directory)) {
    SbDirectoryClose(directory);
  }
}

TEST(SbDirectoryOpenTest, FailsNullPathWithNullError) {
  SbDirectory directory = SbDirectoryOpen(NULL, NULL);
  EXPECT_FALSE(SbDirectoryIsValid(directory));
  if (SbDirectoryIsValid(directory)) {
    SbDirectoryClose(directory);
  }
}

TEST(SbDirectoryOpenTest, FailsEmptyPath) {
  SbFileError error = kSbFileErrorMax;
  SbDirectory directory = SbDirectoryOpen("", &error);
  EXPECT_FALSE(SbDirectoryIsValid(directory));
  EXPECT_EQ(kSbFileErrorNotFound, error);
  if (SbDirectoryIsValid(directory)) {
    SbDirectoryClose(directory);
  }
}

TEST(SbDirectoryOpenTest, FailsRegularFile) {
  ScopedRandomFile file;

  SbFileError error = kSbFileErrorMax;
  SbDirectory directory = SbDirectoryOpen(file.filename().c_str(), &error);
  EXPECT_FALSE(SbDirectoryIsValid(directory));
  EXPECT_EQ(kSbFileErrorNotADirectory, error);
  if (SbDirectoryIsValid(directory)) {
    SbDirectoryClose(directory);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

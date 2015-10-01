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

#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(SbFileSeekTest, InvalidFileErrors) {
  int result = SbFileSeek(kSbFileInvalid, kSbFileFromBegin, 50);
  EXPECT_EQ(-1, result);

  result = SbFileSeek(kSbFileInvalid, kSbFileFromEnd, -50);
  EXPECT_EQ(-1, result);

  result = SbFileSeek(kSbFileInvalid, kSbFileFromCurrent, -50);
  EXPECT_EQ(-1, result);

  result = SbFileSeek(kSbFileInvalid, kSbFileFromCurrent, 50);
  EXPECT_EQ(-1, result);
}

TEST(SbFileSeekTest, FromEndWorks) {
  starboard::nplb::ScopedRandomFile random_file(128);
  const std::string &filename = random_file.filename();
  SbFile file = SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead,
                           NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

  SbFileInfo info;
  bool result = SbFileGetInfo(file, &info);
  EXPECT_TRUE(result);

  int64_t position = SbFileSeek(file, kSbFileFromEnd, 0);
  EXPECT_EQ(info.size, position);

  position = SbFileSeek(file, kSbFileFromEnd, -10);
  EXPECT_EQ(info.size - 10, position);

  position = SbFileSeek(file, kSbFileFromEnd, -info.size);
  EXPECT_EQ(0, position);

  result = SbFileClose(file);
  EXPECT_TRUE(result);
}

TEST(SbFileSeekTest, FromCurrentWorks) {
  starboard::nplb::ScopedRandomFile random_file(128);
  const std::string &filename = random_file.filename();
  SbFile file = SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead,
                           NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

  SbFileInfo info;
  bool result = SbFileGetInfo(file, &info);
  EXPECT_TRUE(result);

  int64_t position = SbFileSeek(file, kSbFileFromCurrent, 0);
  EXPECT_EQ(0, position);

  position = SbFileSeek(file, kSbFileFromCurrent, 10);
  EXPECT_EQ(10, position);

  position = SbFileSeek(file, kSbFileFromCurrent, 10);
  EXPECT_EQ(20, position);

  position = SbFileSeek(file, kSbFileFromCurrent, 0);
  EXPECT_EQ(20, position);

  position = SbFileSeek(file, kSbFileFromCurrent, info.size - position);
  EXPECT_EQ(info.size, position);

  position = SbFileSeek(file, kSbFileFromCurrent, -info.size);
  EXPECT_EQ(0, position);

  result = SbFileClose(file);
  EXPECT_TRUE(result);
}

TEST(SbFileSeekTest, FromBeginWorks) {
  starboard::nplb::ScopedRandomFile random_file(128);
  const std::string &filename = random_file.filename();
  SbFile file = SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead,
                           NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

  SbFileInfo info;
  bool result = SbFileGetInfo(file, &info);
  EXPECT_TRUE(result);

  int64_t position = SbFileSeek(file, kSbFileFromBegin, 0);
  EXPECT_EQ(0, position);

  position = SbFileSeek(file, kSbFileFromBegin, 10);
  EXPECT_EQ(10, position);

  position = SbFileSeek(file, kSbFileFromBegin, 20);
  EXPECT_EQ(20, position);

  position = SbFileSeek(file, kSbFileFromBegin, info.size - 10);
  EXPECT_EQ(info.size - 10, position);

  position = SbFileSeek(file, kSbFileFromBegin, info.size);
  EXPECT_EQ(info.size, position);

  result = SbFileClose(file);
  EXPECT_TRUE(result);
}

}  // namespace

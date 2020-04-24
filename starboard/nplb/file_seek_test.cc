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

#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbFileSeekTest, InvalidFileErrors) {
  int result =
      static_cast<int>(SbFileSeek(kSbFileInvalid, kSbFileFromBegin, 50));
  EXPECT_EQ(-1, result);

  result = static_cast<int>(SbFileSeek(kSbFileInvalid, kSbFileFromEnd, -50));
  EXPECT_EQ(-1, result);

  result =
      static_cast<int>(SbFileSeek(kSbFileInvalid, kSbFileFromCurrent, -50));
  EXPECT_EQ(-1, result);

  result = static_cast<int>(SbFileSeek(kSbFileInvalid, kSbFileFromCurrent, 50));
  EXPECT_EQ(-1, result);
}

TEST(SbFileSeekTest, FromEndWorks) {
  starboard::nplb::ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();
  SbFile file =
      SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

  SbFileInfo info;
  bool result = SbFileGetInfo(file, &info);
  EXPECT_TRUE(result);

  int64_t position = SbFileSeek(file, kSbFileFromEnd, 0);
  EXPECT_EQ(info.size, position);

  int64_t target = -(random_file.size() / 6);
  position = SbFileSeek(file, kSbFileFromEnd, target);
  EXPECT_EQ(info.size + target, position);

  position = SbFileSeek(file, kSbFileFromEnd, -info.size);
  EXPECT_EQ(0, position);

  result = SbFileClose(file);
  EXPECT_TRUE(result);
}

TEST(SbFileSeekTest, FromCurrentWorks) {
  starboard::nplb::ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();
  SbFile file =
      SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

  SbFileInfo info;
  bool result = SbFileGetInfo(file, &info);
  EXPECT_TRUE(result);

  int64_t position = SbFileSeek(file, kSbFileFromCurrent, 0);
  EXPECT_EQ(0, position);

  int64_t target = random_file.size() / 6;
  position = SbFileSeek(file, kSbFileFromCurrent, target);
  EXPECT_EQ(target, position);

  position = SbFileSeek(file, kSbFileFromCurrent, target);
  EXPECT_EQ(target * 2, position);

  position = SbFileSeek(file, kSbFileFromCurrent, 0);
  EXPECT_EQ(target * 2, position);

  position = SbFileSeek(file, kSbFileFromCurrent, info.size - position);
  EXPECT_EQ(info.size, position);

  position = SbFileSeek(file, kSbFileFromCurrent, -info.size);
  EXPECT_EQ(0, position);

  result = SbFileClose(file);
  EXPECT_TRUE(result);
}

TEST(SbFileSeekTest, FromBeginWorks) {
  starboard::nplb::ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();
  SbFile file =
      SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

  SbFileInfo info;
  bool result = SbFileGetInfo(file, &info);
  EXPECT_TRUE(result);

  int64_t position = SbFileSeek(file, kSbFileFromBegin, 0);
  EXPECT_EQ(0, position);

  int64_t target = random_file.size() / 6;
  position = SbFileSeek(file, kSbFileFromBegin, target);
  EXPECT_EQ(target, position);

  target = random_file.size() / 3;
  position = SbFileSeek(file, kSbFileFromBegin, target);
  EXPECT_EQ(target, position);

  target = info.size - random_file.size() / 6;
  position = SbFileSeek(file, kSbFileFromBegin, target);
  EXPECT_EQ(target, position);

  position = SbFileSeek(file, kSbFileFromBegin, info.size);
  EXPECT_EQ(info.size, position);

  result = SbFileClose(file);
  EXPECT_TRUE(result);
}

std::string GetTestStaticContentFile() {
  std::string filename = GetFileTestsFilePaths().front();
  int content_length = GetTestFileExpectedContent(filename).length();
  EXPECT_GT(content_length, 40);
  return filename;
}

TEST(SbFileSeekTest, FromEndInStaticContentWorks) {
  std::string filename = GetTestStaticContentFile();
  SbFile file =
      SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

  int content_length = GetTestFileExpectedContent(filename).length();

  SbFileInfo info;
  bool result = SbFileGetInfo(file, &info);
  EXPECT_TRUE(result);

  int64_t position = SbFileSeek(file, kSbFileFromEnd, 0);
  EXPECT_EQ(info.size, position);

  int64_t target = -(content_length / 6);
  position = SbFileSeek(file, kSbFileFromEnd, target);
  EXPECT_EQ(info.size + target, position);

  position = SbFileSeek(file, kSbFileFromEnd, -info.size);
  EXPECT_EQ(0, position);

  result = SbFileClose(file);
  EXPECT_TRUE(result);
}

TEST(SbFileSeekTest, FromCurrentInStaticContentWorks) {
  std::string filename = GetTestStaticContentFile();
  SbFile file =
      SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

  int content_length = GetTestFileExpectedContent(filename).length();

  SbFileInfo info;
  bool result = SbFileGetInfo(file, &info);
  EXPECT_TRUE(result);

  int64_t position = SbFileSeek(file, kSbFileFromCurrent, 0);
  EXPECT_EQ(0, position);

  int64_t target = content_length / 6;
  position = SbFileSeek(file, kSbFileFromCurrent, target);
  EXPECT_EQ(target, position);

  position = SbFileSeek(file, kSbFileFromCurrent, target);
  EXPECT_EQ(target * 2, position);

  position = SbFileSeek(file, kSbFileFromCurrent, 0);
  EXPECT_EQ(target * 2, position);

  position = SbFileSeek(file, kSbFileFromCurrent, info.size - position);
  EXPECT_EQ(info.size, position);

  position = SbFileSeek(file, kSbFileFromCurrent, -info.size);
  EXPECT_EQ(0, position);

  result = SbFileClose(file);
  EXPECT_TRUE(result);
}

TEST(SbFileSeekTest, FromBeginInStaticContentWorks) {
  std::string filename = GetFileTestsFilePaths().front();
  SbFile file =
      SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

  int content_length = GetTestFileExpectedContent(filename).length();

  SbFileInfo info;
  bool result = SbFileGetInfo(file, &info);
  EXPECT_TRUE(result);

  int64_t position = SbFileSeek(file, kSbFileFromBegin, 0);
  EXPECT_EQ(0, position);

  int64_t target = content_length / 6;
  position = SbFileSeek(file, kSbFileFromBegin, target);
  EXPECT_EQ(target, position);

  target = content_length / 3;
  position = SbFileSeek(file, kSbFileFromBegin, target);
  EXPECT_EQ(target, position);

  target = info.size - content_length / 6;
  position = SbFileSeek(file, kSbFileFromBegin, target);
  EXPECT_EQ(target, position);

  position = SbFileSeek(file, kSbFileFromBegin, info.size);
  EXPECT_EQ(info.size, position);

  result = SbFileClose(file);
  EXPECT_TRUE(result);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

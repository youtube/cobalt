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

TEST(SbFileTruncateTest, InvalidFileErrors) {
  bool result = SbFileTruncate(kSbFileInvalid, 0);
  EXPECT_FALSE(result);

  result = SbFileTruncate(kSbFileInvalid, -1);
  EXPECT_FALSE(result);

  result = SbFileTruncate(kSbFileInvalid, 100);
  EXPECT_FALSE(result);
}

TEST(SbFileTruncateTest, TruncateToZero) {
  const int kStartSize = 123;
  const int kEndSize = 0;
  ScopedRandomFile random_file(kStartSize);
  const std::string& filename = random_file.filename();

  SbFile file =
      SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileWrite | kSbFileRead,
                 NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

  {
    SbFileInfo info = {0};
    bool result = SbFileGetInfo(file, &info);
    EXPECT_EQ(kStartSize, info.size);
  }

  bool result = SbFileTruncate(file, kEndSize);
  EXPECT_TRUE(result);

  {
    SbFileInfo info = {0};
    result = SbFileGetInfo(file, &info);
    EXPECT_EQ(kEndSize, info.size);
  }

  result = SbFileClose(file);
  EXPECT_TRUE(result);
}

TEST(SbFileTruncateTest, TruncateUpInSize) {
  // "Truncate," I don't think that word means what you think it means.
  const int kStartSize = 123;
  const int kEndSize = kStartSize * 2;
  ScopedRandomFile random_file(kStartSize);
  const std::string& filename = random_file.filename();

  SbFile file =
      SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileWrite | kSbFileRead,
                 NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

  {
    SbFileInfo info = {0};
    bool result = SbFileGetInfo(file, &info);
    EXPECT_TRUE(result);
    EXPECT_EQ(kStartSize, info.size);
  }

  int position = static_cast<int>(SbFileSeek(file, kSbFileFromCurrent, 0));
  EXPECT_EQ(0, position);

  bool result = SbFileTruncate(file, kEndSize);
  EXPECT_TRUE(result);

  position = static_cast<int>(SbFileSeek(file, kSbFileFromCurrent, 0));
  EXPECT_EQ(0, position);

  {
    SbFileInfo info = {0};
    result = SbFileGetInfo(file, &info);
    EXPECT_TRUE(result);
    EXPECT_EQ(kEndSize, info.size);
  }

  char buffer[kEndSize] = {0};
  int bytes = SbFileReadAll(file, buffer, kEndSize);
  EXPECT_EQ(kEndSize, bytes);

  ScopedRandomFile::ExpectPattern(0, buffer, kStartSize, __LINE__);

  for (int i = kStartSize; i < kEndSize; ++i) {
    EXPECT_EQ(0, buffer[i]);
  }

  result = SbFileClose(file);
  EXPECT_TRUE(result);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

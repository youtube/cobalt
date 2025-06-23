// Copyright 2024 The Cobalt Authors. All Rights Reserved.
// PosixFileWriteTest
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
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "starboard/common/file.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixFileFtruncateTest, InvalidFileErrors) {
  int result = ftruncate(-1, 0);
  EXPECT_FALSE(result == 0);

  result = ftruncate(-1, -1);
  EXPECT_FALSE(result == 0);

  result = ftruncate(-1, 100);
  EXPECT_FALSE(result == 0);
}

TEST(PosixFileFtruncateTest, TruncateToZero) {
  const int kStartSize = 123;
  const int kEndSize = 0;
  ScopedRandomFile random_file(kStartSize);
  const std::string& filename = random_file.filename();

  int file = open(filename.c_str(), O_RDWR);
  ASSERT_TRUE(file >= 0);

  {
    struct stat info = {0};
    fstat(file, &info);
    EXPECT_EQ(kStartSize, info.st_size);
  }

  int result = ftruncate(file, kEndSize);
  EXPECT_TRUE(result == 0);

  {
    struct stat info = {0};
    result = fstat(file, &info);
    EXPECT_EQ(kEndSize, info.st_size);
  }

  result = close(file);
  EXPECT_TRUE(result == 0);
}

TEST(PosixFileFtruncateTest, TruncateUpInSize) {
  // "Truncate," I don't think that word means what you think it means.
  const int kStartSize = 123;
  const int kEndSize = kStartSize * 2;
  ScopedRandomFile random_file(kStartSize);
  const std::string& filename = random_file.filename();

  int file = open(filename.c_str(), O_RDWR);
  ASSERT_TRUE(file >= 0);

  {
    struct stat info = {0};
    int result = fstat(file, &info);
    EXPECT_TRUE(result == 0);
    EXPECT_EQ(kStartSize, info.st_size);
  }

  int position = static_cast<int>(lseek(file, 0, SEEK_CUR));
  EXPECT_EQ(0, position);

  int result = ftruncate(file, kEndSize);
  EXPECT_TRUE(result == 0);

  position = static_cast<int>(lseek(file, 0, SEEK_CUR));
  EXPECT_EQ(0, position);

  {
    struct stat info = {0};
    result = fstat(file, &info);
    EXPECT_TRUE(result == 0);
    EXPECT_EQ(kEndSize, info.st_size);
  }

  char buffer[kEndSize] = {0};
  int bytes = static_cast<int>(ReadAll(file, buffer, kEndSize));
  EXPECT_EQ(kEndSize, bytes);

  ScopedRandomFile::ExpectPattern(0, buffer, kStartSize, __LINE__);

  for (int i = kStartSize; i < kEndSize; ++i) {
    EXPECT_EQ(0, buffer[i]);
  }

  result = close(file);
  EXPECT_TRUE(result == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

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
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixFileSeekTest, InvalidFileErrors) {
  int invalid_fd = -1;
  int result = static_cast<int>(lseek(invalid_fd, 50, SEEK_SET));
  EXPECT_EQ(-1, result);

  result = static_cast<int>(lseek(invalid_fd, -50, SEEK_END));
  EXPECT_EQ(-1, result);

  result = static_cast<int>(lseek(invalid_fd, -50, SEEK_CUR));
  EXPECT_EQ(-1, result);

  result = static_cast<int>(lseek(invalid_fd, 50, SEEK_CUR));
  EXPECT_EQ(-1, result);
}

TEST(PosixFileSeekTest, FromEndWorks) {
  starboard::nplb::ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();
  int file = open(filename.c_str(), O_RDONLY);
  ASSERT_TRUE(file >= 0);

  struct stat info;
  int result = fstat(file, &info);
  EXPECT_TRUE(result == 0);

  int64_t position = lseek(file, 0, SEEK_END);
  EXPECT_EQ(info.st_size, position);

  int64_t target = -(random_file.size() / 6);
  position = lseek(file, target, SEEK_END);
  EXPECT_EQ(info.st_size + target, position);

  position = lseek(file, -info.st_size, SEEK_END);
  EXPECT_EQ(0, position);

  result = close(file);
  EXPECT_TRUE(result == 0);
}

TEST(PosixFileSeekTest, FromCurrentWorks) {
  starboard::nplb::ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();
  int file = open(filename.c_str(), O_RDONLY);
  ASSERT_TRUE(file >= 0);

  struct stat info;
  int result = fstat(file, &info);
  EXPECT_TRUE(result == 0);

  int64_t position = lseek(file, 0, SEEK_CUR);
  EXPECT_EQ(0, position);

  int64_t target = random_file.size() / 6;
  position = lseek(file, target, SEEK_CUR);
  EXPECT_EQ(target, position);

  position = lseek(file, target, SEEK_CUR);
  EXPECT_EQ(target * 2, position);

  position = lseek(file, 0, SEEK_CUR);
  EXPECT_EQ(target * 2, position);

  position = lseek(file, info.st_size - position, SEEK_CUR);
  EXPECT_EQ(info.st_size, position);

  position = lseek(file, -info.st_size, SEEK_CUR);
  EXPECT_EQ(0, position);

  result = close(file);
  EXPECT_TRUE(result == 0);
}

TEST(PosixFileSeekTest, FromBeginWorks) {
  starboard::nplb::ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();
  int file = open(filename.c_str(), O_RDONLY);
  ASSERT_TRUE(file >= 0);

  struct stat info;
  int result = fstat(file, &info);
  EXPECT_TRUE(result == 0);

  int64_t position = lseek(file, 0, SEEK_SET);
  EXPECT_EQ(0, position);

  int64_t target = random_file.size() / 6;
  position = lseek(file, target, SEEK_SET);
  EXPECT_EQ(target, position);

  target = random_file.size() / 3;
  position = lseek(file, target, SEEK_SET);
  EXPECT_EQ(target, position);

  target = info.st_size - random_file.size() / 6;
  position = lseek(file, target, SEEK_SET);
  EXPECT_EQ(target, position);

  position = lseek(file, info.st_size, SEEK_SET);
  EXPECT_EQ(info.st_size, position);

  result = close(file);
  EXPECT_TRUE(result == 0);
}

std::string GetTestStaticContentFile() {
  std::string filename = GetFileTestsFilePaths().front();
  int content_length = GetTestFileExpectedContent(filename).length();
  EXPECT_GT(content_length, 40);
  return filename;
}

TEST(PosixFileSeekTest, FromEndInStaticContentWorks) {
  std::string filename = GetTestStaticContentFile();
  int file = open(filename.c_str(), O_RDONLY);
  ASSERT_TRUE(file >= 0);

  int content_length = GetTestFileExpectedContent(filename).length();

  struct stat info;
  int result = fstat(file, &info);
  EXPECT_TRUE(result == 0);

  int64_t position = lseek(file, 0, SEEK_END);
  EXPECT_EQ(info.st_size, position);

  int64_t target = -(content_length / 6);
  position = lseek(file, target, SEEK_END);
  EXPECT_EQ(info.st_size + target, position);

  position = lseek(file, -info.st_size, SEEK_END);
  EXPECT_EQ(0, position);

  result = close(file);
  EXPECT_TRUE(result == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
